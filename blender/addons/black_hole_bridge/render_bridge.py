"""Bridge Blender ⇄ the C++ headless CPU renderer ``bh_render_cpu``.

CLI contract (physics stays on the C++ side):

    bh_render_cpu [--scene scene.json] --out image.png [--width W] [--height H]
                  [--mode legacy|relativistic|blackbody] [--integrator rk4|rk45]
                  [--azimuth RAD] [--elevation RAD] [--radius-rs X] [--fov-y-deg D]
                  [--frames N] [--threads T] [--supersample S] [--stars]

* ``--out`` extension picks the format (.png / .bmp / .ppm); Blender loads PNG/BMP.
* ``--frames N`` (N > 1) writes ``stem_0000.png … stem_{N-1:04d}.png`` sweeping
  azimuth from the given value through +2π (frame 0 = given azimuth).
* Exit code 0 on success and one JSON summary line on stdout, e.g.
  ``{"frames":1,"width":200,"height":150,"shadow_fraction":0.12,...}``.

The pure helpers (``build_render_command``, ``parse_render_summary``,
``expected_frame_paths``, ``find_sequence_files``, ``plane_size_for_fov``)
have no bpy dependency and are covered by tests/test_blender_addon_parity.py.
"""

import json
import math
import os
import re
import shutil
import subprocess
from pathlib import Path
from typing import Iterable, List, Optional, Sequence, Tuple

from . import camera_orbit
from . import constants as C
from . import json_io

try:
    import bpy
    from bpy.props import StringProperty
    from bpy.types import Operator
    from bpy_extras.io_utils import ImportHelper
except ImportError:
    bpy = None  # type: ignore

    def StringProperty(**_kw):  # type: ignore[no-redef]
        return None

    class Operator:  # type: ignore[no-redef]
        """Headless placeholder so the module imports without bpy."""

    class ImportHelper:  # type: ignore[no-redef]
        """Headless placeholder so the module imports without bpy."""


_SEQ_RE = re.compile(r"^(?P<stem>.*)_(?P<num>\d{4})$")
_LOADABLE_EXT = (".png", ".bmp")


# --------------------------------------------------------------------------- #
# Pure helpers (no bpy)
# --------------------------------------------------------------------------- #
def _num(value: float) -> str:
    return repr(float(value))


def build_render_command(
    renderer_path: str,
    scene_json_path: Optional[str],
    out_path: str,
    width: int,
    height: int,
    mode: str,
    integrator: str,
    azimuth: float,
    elevation: float,
    radius_rs: float,
    fov_y_deg: float,
    frames: int = 1,
    supersample: int = 1,
    threads: Optional[int] = None,
    stars: bool = False,
) -> List[str]:
    """Return the argv list for ``bh_render_cpu`` following the CLI contract.

    ``mode``/``integrator`` are lower-cased ("LEGACY" → "legacy", "RK45" →
    "rk45") and validated; ``--scene`` is omitted when ``scene_json_path`` is
    empty; ``--threads`` only when ``threads`` is given.
    """
    mode_l = str(mode).strip().lower()
    integ_l = str(integrator).strip().lower()
    if mode_l not in C.RENDER_MODES:
        raise ValueError(f"mode must be one of {C.RENDER_MODES}, got {mode!r}")
    if integ_l not in C.RENDER_INTEGRATORS:
        raise ValueError(f"integrator must be one of {C.RENDER_INTEGRATORS}, got {integrator!r}")
    if int(frames) < 1 or int(width) < 1 or int(height) < 1 or int(supersample) < 1:
        raise ValueError("frames, width, height and supersample must be >= 1")

    argv: List[str] = [str(renderer_path)]
    if scene_json_path:
        argv += ["--scene", str(scene_json_path)]
    argv += ["--out", str(out_path)]
    argv += ["--width", str(int(width)), "--height", str(int(height))]
    argv += ["--mode", mode_l, "--integrator", integ_l]
    argv += ["--azimuth", _num(azimuth), "--elevation", _num(elevation)]
    argv += ["--radius-rs", _num(radius_rs), "--fov-y-deg", _num(fov_y_deg)]
    argv += ["--frames", str(int(frames))]
    if threads is not None:
        argv += ["--threads", str(int(threads))]
    argv += ["--supersample", str(int(supersample))]
    if stars:
        argv.append("--stars")
    return argv


def parse_render_summary(stdout_text: str) -> Optional[dict]:
    """Return the last stdout line that parses as a JSON object with "frames"."""
    for line in reversed((stdout_text or "").splitlines()):
        line = line.strip()
        if not line.startswith("{"):
            continue
        try:
            data = json.loads(line)
        except json.JSONDecodeError:
            continue
        if isinstance(data, dict) and "frames" in data:
            return data
    return None


def expected_frame_paths(out_path: str, frames: int) -> List[str]:
    """Files ``bh_render_cpu`` writes for ``--out out_path --frames frames``."""
    p = Path(out_path)
    if int(frames) <= 1:
        return [str(p)]
    return [
        str(p.with_name(f"{p.stem}_{i:0{C.RENDER_FRAME_DIGITS}d}{p.suffix}"))
        for i in range(int(frames))
    ]


def find_sequence_files(path: str, existing: Optional[Iterable[str]] = None) -> List[str]:
    """Return the sorted ``stem_NNNN.ext`` siblings of ``path`` (itself included).

    A file whose stem does not end in ``_NNNN`` is a single image → ``[path]``.
    ``existing`` lets tests inject a directory listing instead of touching disk.
    """
    p = Path(path)
    m = _SEQ_RE.match(p.stem)
    if m is None:
        return [str(p)]
    stem = m.group("stem")
    names = list(existing) if existing is not None else (
        [e.name for e in p.parent.iterdir()] if p.parent.is_dir() else [p.name]
    )
    pattern = re.compile(rf"^{re.escape(stem)}_\d{{4}}{re.escape(p.suffix)}$")
    matches = sorted(n for n in names if pattern.match(n))
    if not matches:
        return [str(p)]
    return [str(p.with_name(n)) for n in matches]


def sequence_first_number(path: str) -> int:
    """Frame number embedded in ``stem_NNNN.ext`` (0 when not a sequence)."""
    m = _SEQ_RE.match(Path(path).stem)
    return int(m.group("num")) if m else 0


def find_renderer(explicit: str = "", search_roots: Sequence[str] = ()) -> Optional[str]:
    """Locate ``bh_render_cpu``: explicit path → build dirs under roots → PATH."""
    exe_names = [C.RENDER_BINARY_NAME, C.RENDER_BINARY_NAME + ".exe"]
    if explicit:
        p = Path(explicit)
        if p.is_file():
            return str(p)
        if p.is_dir():
            for name in exe_names:
                if (p / name).is_file():
                    return str(p / name)
        return None
    for root in search_roots:
        for rel in C.RENDER_BINARY_CANDIDATES:
            for cand in (Path(root) / rel, Path(root) / (rel + ".exe")):
                if cand.is_file():
                    return str(cand)
    return shutil.which(C.RENDER_BINARY_NAME)


def orbit_params_from_cpp_position(p: Sequence[float]) -> Tuple[float, float, float]:
    """Inverse of constants.camera_position (C++ Y-up frame): (radius, azimuth, elevation).

    x = r sin(el) cos(az), y = r cos(el), z = r sin(el) sin(az).
    Used to drive bh_render_cpu from the camera's ACTUAL animated position, so
    every frame of a C++ render matches the Blender camera exactly, whatever
    preset, easing or hand-made keyframes produced it.
    """
    x, y, z = (float(v) for v in p)
    r = math.sqrt(x * x + y * y + z * z)
    if r <= 0.0:
        raise ValueError("camera at the origin")
    el = math.acos(max(-1.0, min(1.0, y / r)))
    az = math.atan2(z, x)
    return r, az, el


def camera_fov_y(sensor_fit: str, angle: float, angle_y: float, aspect: float) -> float:
    """Vertical FOV (radians) of a Blender camera for an image of ``aspect`` = w/h."""
    if sensor_fit == "VERTICAL":
        return angle_y
    if sensor_fit == "HORIZONTAL" or aspect >= 1.0:
        return 2.0 * math.atan(math.tan(0.5 * angle) / aspect)
    return angle


def plane_size_for_fov(fov_y_rad: float, aspect: float, depth: float) -> Tuple[float, float]:
    """(width, height) of a plane at ``depth`` that exactly fills a vertical FOV."""
    h = 2.0 * depth * math.tan(0.5 * fov_y_rad)
    return (h * max(aspect, 1e-9), h)


# --------------------------------------------------------------------------- #
# bpy side
# --------------------------------------------------------------------------- #
def _abspath(p: str) -> str:
    return bpy.path.abspath(p) if p else ""


def _search_roots() -> List[str]:
    roots: List[str] = []
    blend_dir = bpy.path.abspath("//") if bpy.data.filepath else ""
    if blend_dir:
        d = Path(blend_dir)
        roots += [str(d), *[str(x) for x in d.parents][:3]]
    roots += [os.getcwd(), str(Path(__file__).resolve().parents[3])]  # repo root
    return roots


def _configure_sequence(image, image_user, files: List[str], scene) -> int:
    """Make ``image`` a SEQUENCE spanning ``files`` starting at the scene's
    frame_start (file 0000 ↔ frame_start, as rendered by BH_OT_run_cpu_render)."""
    n = len(files)
    if n <= 1:
        return 1
    first = int(scene.frame_start)
    image.source = "SEQUENCE"
    image_user.frame_duration = n
    image_user.frame_start = first
    # Blender picks file number = (scene_frame − frame_start + 1) + frame_offset.
    image_user.frame_offset = sequence_first_number(files[0]) - 1
    image_user.use_cyclic = False
    image_user.use_auto_refresh = True
    scene.frame_end = first + n - 1
    return n


def _load_image(path: str, scene=None):
    """Load (or re-read) a bh_render_cpu frame.

    * ``reload()``: re-rendering to the same path must show the NEW pixels
      (``check_existing=True`` alone returns the stale datablock).
    * Under the add-on's "Raw" view transform the PNG must be read as
      Non-Color so its 8-bit values reach the render unchanged (as sRGB they
      would be linearised and come out much darker).
    """
    image = bpy.data.images.load(path, check_existing=True)
    image.reload()
    scene = scene if scene is not None else bpy.context.scene
    raw = scene is not None and scene.view_settings.view_transform == "Raw"
    try:
        image.colorspace_settings.name = "Non-Color" if raw else "sRGB"
    except TypeError:
        pass  # colour-management config without these names: keep default
    image["bh_role"] = "cpp_render"
    return image


def _ensure_camera(context):
    from . import scene_builder

    coll = scene_builder.ensure_collections()[C.COLL_CAMERA]
    _empty, cam = camera_orbit.ensure_orbit_rig(coll)
    context.scene.camera = cam
    return cam


def import_render_background(context, path: str) -> Tuple[object, int]:
    """Load ``path`` as the BH_OrbitCamera background (FIT, alpha 1).

    Returns (image, frame_count). Numbered ``stem_0000.png`` siblings become
    an image sequence and the scene frame range is set to match.
    """
    cam = _ensure_camera(context)
    files = find_sequence_files(path)
    scene = context.scene
    image = _load_image(files[0], scene)
    scene.render.resolution_x, scene.render.resolution_y = image.size[0], image.size[1]

    cam.data.show_background_images = True
    slot = None
    for b in cam.data.background_images:
        if b.image is not None and b.image.get("bh_role") == "cpp_render":
            slot = b
            break
    if slot is None:
        slot = cam.data.background_images.new()
    slot.source = "IMAGE"
    slot.image = image
    slot.alpha = 1.0
    slot.frame_method = "FIT"
    slot.display_depth = "BACK"
    slot.show_background_image = True
    n = _configure_sequence(image, slot.image_user, files, scene)
    cam["bh_render_background"] = files[0]
    return image, n


def _ensure_render_plane_material(image, name: str = "BH_RenderPlane_Mat"):
    mat = bpy.data.materials.get(name)
    if mat is None:
        mat = bpy.data.materials.new(name)
    mat.use_nodes = True
    nodes, links = mat.node_tree.nodes, mat.node_tree.links
    nodes.clear()
    out = nodes.new("ShaderNodeOutputMaterial")
    tex = nodes.new("ShaderNodeTexImage")
    tex.name = "BH_RenderImage"
    tex.image = image
    tex.interpolation = "Closest" if image.size[0] <= C.COMPUTE_WH[0] else "Linear"
    emission = nodes.new("ShaderNodeEmission")
    emission.inputs["Strength"].default_value = 1.0
    links.new(tex.outputs["Color"], emission.inputs["Color"])
    links.new(emission.outputs["Emission"], out.inputs["Surface"])
    return mat, tex


def import_render_as_plane(context, path: str, depth: float = 10.0) -> Tuple[object, int]:
    """Unlit emissive plane parented to BH_OrbitCamera at local Z = −depth,
    sized to fill the camera frame (vertical FOV × image aspect)."""
    cam = _ensure_camera(context)
    files = find_sequence_files(path)
    scene = context.scene
    image = _load_image(files[0], scene)
    w_px, h_px = image.size[0], image.size[1]
    aspect = (w_px / h_px) if h_px else 4.0 / 3.0
    scene.render.resolution_x, scene.render.resolution_y = w_px, h_px

    cam_data = cam.data
    fov_y = camera_fov_y(cam_data.sensor_fit, cam_data.angle, cam_data.angle_y, aspect)
    w, h = plane_size_for_fov(fov_y, aspect, depth)

    name = C.OBJ_RENDER_PLANE
    obj = bpy.data.objects.get(name)
    if obj is not None:
        mesh_old = obj.data
        bpy.data.objects.remove(obj, do_unlink=True)
        if mesh_old is not None and mesh_old.users == 0:
            bpy.data.meshes.remove(mesh_old)

    mesh = bpy.data.meshes.new(name + "_Mesh")
    hw, hh = 0.5 * w, 0.5 * h
    mesh.from_pydata(
        [(-hw, -hh, 0.0), (hw, -hh, 0.0), (hw, hh, 0.0), (-hw, hh, 0.0)],
        [],
        [(0, 1, 2, 3)],
    )
    mesh.update()
    uv = mesh.uv_layers.new(name="UVMap")
    for loop_index, (u, v) in zip(mesh.polygons[0].loop_indices, ((0, 0), (1, 0), (1, 1), (0, 1))):
        uv.data[loop_index].uv = (u, v)

    obj = bpy.data.objects.new(name, mesh)
    target_coll = cam.users_collection[0] if cam.users_collection else scene.collection
    target_coll.objects.link(obj)
    obj.parent = cam
    obj.matrix_parent_inverse.identity()
    obj.location = (0.0, 0.0, -depth)
    obj.rotation_euler = (0.0, 0.0, 0.0)

    mat, tex = _ensure_render_plane_material(image)
    mesh.materials.append(mat)
    n = _configure_sequence(image, tex.image_user, files, scene)
    obj["bh_role"] = "render_plane"
    obj["bh_render_source"] = files[0]
    obj["bh_depth"] = depth
    return obj, n


class BH_OT_run_cpu_render(Operator):
    bl_idname = "bh.run_cpu_render"
    bl_label = "Render via C++ (CPU)"
    bl_description = (
        "Export scene JSON, run bh_render_cpu (legacy / relativistic / blackbody) "
        "for each frame with the camera's actual animated pose, then load the "
        "result as the orbit camera background"
    )

    def execute(self, context):
        s = context.scene.bh_bridge
        renderer = find_renderer(_abspath(s.renderer_path), _search_roots())
        if not renderer:
            self.report(
                {"ERROR"},
                "bh_render_cpu not found. Build it (cmake --build build/scientific --target "
                "bh_render_cpu) and set 'Renderer path' in the panel.",
            )
            return {"CANCELLED"}

        out_path = Path(_abspath(s.render_output_path) or "bh_render/frame.png")
        if out_path.suffix.lower() not in (".png", ".bmp", ".ppm"):
            out_path = out_path.with_suffix(".png")
        out_path.parent.mkdir(parents=True, exist_ok=True)
        scene_json = out_path.with_name(out_path.stem + "_scene_params.json")
        json_io.write_json(scene_json, json_io.params_from_settings(s))

        scene = context.scene
        # Always render from BH_OrbitCamera — the camera that receives the
        # background afterwards. A freshly created rig sits at the origin:
        # place it from the panel parameters first.
        cam = _ensure_camera(context)
        if cam.matrix_world.translation.length < 1e-9:
            camera_orbit.align_camera_from_params(
                cam,
                radius=camera_orbit.geo_radius_from_settings(s),
                azimuth=float(s.camera_azimuth),
                elevation=float(s.camera_elevation),
                fov_y_deg=float(s.camera_fov_y_deg),
            )
            context.view_layer.update()
        aspect = int(s.render_width) / max(1, int(s.render_height))
        n_frames = int(s.render_frames)
        outs = expected_frame_paths(str(out_path), n_frames)
        frame_before = scene.frame_current
        # Sequences sample the scene frames frame_start … frame_start+N−1; a
        # single image uses the current frame. Each frame is rendered with the
        # camera's ACTUAL animated pose (exact sync with the Blender viewport).
        frames = [scene.frame_current] if n_frames == 1 else [scene.frame_start + k for k in range(n_frames)]
        summaries = []
        commands = []
        try:
            for frame, out_k in zip(frames, outs):
                scene.frame_set(frame)
                pos_cpp = C.blender_to_cpp(tuple(cam.matrix_world.translation))
                try:
                    radius, azimuth, elevation = orbit_params_from_cpp_position(pos_cpp)
                except ValueError:
                    self.report({"ERROR"}, f"{cam.name} is at the black-hole centre at frame {frame}; "
                                           "use Align Camera or Build Full Scene first")
                    return {"CANCELLED"}
                fov_y = camera_fov_y(cam.data.sensor_fit, cam.data.angle, cam.data.angle_y, aspect)
                argv = build_render_command(
                    renderer_path=renderer,
                    scene_json_path=str(scene_json),
                    out_path=str(out_k),
                    width=int(s.render_width),
                    height=int(s.render_height),
                    mode=s.render_mode,
                    integrator=s.render_integrator,
                    azimuth=azimuth,
                    elevation=elevation,
                    radius_rs=radius,
                    fov_y_deg=math.degrees(fov_y),
                    frames=1,
                    supersample=int(s.render_supersample),
                    stars=bool(s.render_stars),
                )
                commands.append(argv)
                try:
                    proc = subprocess.run(
                        argv, capture_output=True, text=True, timeout=float(s.render_timeout_s)
                    )
                except subprocess.TimeoutExpired:
                    self.report({"ERROR"}, f"bh_render_cpu timed out after {s.render_timeout_s}s (frame {frame})")
                    return {"CANCELLED"}
                except OSError as exc:
                    self.report({"ERROR"}, f"cannot run {renderer}: {exc}")
                    return {"CANCELLED"}
                if proc.returncode != 0:
                    tail = (proc.stderr or proc.stdout or "").strip().splitlines()[-3:]
                    self.report({"ERROR"}, f"bh_render_cpu exit {proc.returncode}: {' | '.join(tail)}")
                    return {"CANCELLED"}
                summaries.append(parse_render_summary(proc.stdout) or {})
        finally:
            scene.frame_set(frame_before)

        summary = dict(summaries[-1]) if summaries else {}
        summary["frames"] = n_frames
        summary["scene_frames"] = [frames[0], frames[-1]]
        summary["command"] = " ".join(commands[0]) if commands else ""
        context.scene["bh_last_render_summary"] = json.dumps(summary)

        first = expected_frame_paths(str(out_path), int(s.render_frames))[0]
        if not Path(first).is_file():
            self.report({"WARNING"}, f"Renderer exited 0 but {first} is missing")
            return {"FINISHED"}
        if Path(first).suffix.lower() not in _LOADABLE_EXT:
            self.report({"INFO"}, f"Rendered {first} (PPM not loadable by Blender; use PNG/BMP)")
            return {"FINISHED"}
        _image, n = import_render_background(context, first)
        self.report({"INFO"}, f"{s.render_mode.lower()} render OK: {n} frame(s) → camera background")
        return {"FINISHED"}


class BH_OT_import_render_background(Operator, ImportHelper):
    bl_idname = "bh.import_render_background"
    bl_label = "Import Render as Camera Background"
    bl_description = "Load a bh_render_cpu PNG/BMP (or stem_0000 sequence) behind BH_OrbitCamera"
    filename_ext = ".png"
    filter_glob: StringProperty(default="*.png;*.bmp", options={"HIDDEN"})

    def execute(self, context):
        if not Path(self.filepath).is_file():
            self.report({"ERROR"}, f"not a file: {self.filepath}")
            return {"CANCELLED"}
        _image, n = import_render_background(context, self.filepath)
        self.report({"INFO"}, f"Background: {n} frame(s) — look through the camera (Numpad 0)")
        return {"FINISHED"}


class BH_OT_import_render_as_plane(Operator, ImportHelper):
    bl_idname = "bh.import_render_as_plane"
    bl_label = "Import Render as Camera Plane"
    bl_description = "Emissive plane parented to the camera, sized to fill the frame"
    filename_ext = ".png"
    filter_glob: StringProperty(default="*.png;*.bmp", options={"HIDDEN"})

    def execute(self, context):
        if not Path(self.filepath).is_file():
            self.report({"ERROR"}, f"not a file: {self.filepath}")
            return {"CANCELLED"}
        obj, n = import_render_as_plane(context, self.filepath)
        self.report({"INFO"}, f"{obj.name}: {n} frame(s), unlit emission 1.0")
        return {"FINISHED"}


ALL_OPERATORS = (
    BH_OT_run_cpu_render,
    BH_OT_import_render_background,
    BH_OT_import_render_as_plane,
)
