# Convenience wrapper around CMake/CTest. Everything here is optional:
# the canonical build is plain CMake (see README.md / docs/ROTEIRO_BUILD.md).
#
#   make configure      scientific-only build tree (no OpenGL deps)   -> build/scientific
#   make build          build it
#   make test           run headless CTest suite (scientific + Python contracts)
#   make configure-gl   full OpenGL build tree                          -> build/gl
#   make build-gl       build BlackHole2D/3D + everything
#   make test-gl        full CTest incl. GPU<->CPU agreement (xvfb-run/Mesa if no display)
#   make shaders        glslangValidator on all compute/grid shaders (GL tree)
#   make render         render showcase PNGs with bh_render_cpu into build/renders/
#   make package-addon  rebuild the deterministic Blender addon zips
#   make blender-scene  build the .blend headless (needs `blender` on PATH)
#   make style-check    static baseline + style + shader contracts only
#   make clean          remove build/

CMAKE      ?= cmake
CTEST      ?= ctest
PYTHON     ?= python3
GENERATOR  ?= $(shell command -v ninja >/dev/null 2>&1 && echo "-G Ninja")
BUILD_TYPE ?= Release
SCI_DIR    := build/scientific
GL_DIR     := build/gl
RENDER_DIR := build/renders

.PHONY: all configure build test test-scientific configure-gl build-gl test-gl test-all shaders \
        render package-addon blender-scene style-check clean help

all: build

help:
	@sed -n '1,20p' Makefile

configure:
	$(CMAKE) -S . -B $(SCI_DIR) $(GENERATOR) -DCMAKE_BUILD_TYPE=$(BUILD_TYPE) \
	    -DBLACK_HOLE_BUILD_GL=OFF -DBUILD_TESTING=ON

build: configure
	$(CMAKE) --build $(SCI_DIR)

test: build
	$(CTEST) --test-dir $(SCI_DIR) --output-on-failure

test-scientific: test

configure-gl:
	$(CMAKE) -S . -B $(GL_DIR) $(GENERATOR) -DCMAKE_BUILD_TYPE=$(BUILD_TYPE) \
	    -DBLACK_HOLE_BUILD_GL=ON -DBUILD_TESTING=ON

build-gl: configure-gl
	$(CMAKE) --build $(GL_DIR)

test-gl: build-gl
	$(CTEST) --test-dir $(GL_DIR) --output-on-failure

test-all: test test-gl

shaders: configure-gl
	$(CMAKE) --build $(GL_DIR) --target validate_shaders

render: build
	@mkdir -p $(RENDER_DIR)
	$(SCI_DIR)/bh_render_cpu --out $(RENDER_DIR)/legacy_el125.png --width 640 --height 480 --elevation 1.25
	$(SCI_DIR)/bh_render_cpu --scene examples/scene_relativistic_showcase.json --mode relativistic \
	    --width 640 --height 360 --supersample 2 --stars --out $(RENDER_DIR)/showcase_relativistic.png
	$(SCI_DIR)/bh_render_cpu --scene examples/scene_relativistic_showcase.json --mode blackbody \
	    --width 640 --height 360 --supersample 2 --stars --out $(RENDER_DIR)/showcase_blackbody.png

package-addon:
	$(PYTHON) blender/scripts/package_addon.py

blender-scene:
	blender --background --factory-startup --python blender/scripts/build_scene_headless.py -- \
	    --out $(abspath build/black_hole_sim.blend)

style-check:
	$(PYTHON) tests/validate_source_invariants.py --root .
	$(PYTHON) tests/test_style_contract.py --root .
	$(PYTHON) tests/test_shader_contract.py --root .

clean:
	rm -rf build
