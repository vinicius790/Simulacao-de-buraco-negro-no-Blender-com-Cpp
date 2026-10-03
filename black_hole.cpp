#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <algorithm>
#include <cctype>
#include <array>
#include <cstdint>
#include <vector>
#include <iostream>
#define _USE_MATH_DEFINES
#include <cmath>
#include <sstream>
#include <iomanip>
#include <cstring>
#include <chrono>
#include <fstream>
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif
#include "black_hole/scene_params.hpp"
#include "black_hole/cpu_renderer.hpp"
#include "black_hole/image_io.hpp"
#include "black_hole/disk_emission.hpp"
#include "black_hole/orbits.hpp"
using namespace glm;
using namespace std;
using Clock = std::chrono::high_resolution_clock;

// VARS
double lastPrintTime = 0.0;
int    framesCount   = 0;
double c = 299792458.0;
double G = 6.67430e-11;
struct Ray;
bool Gravity = false;

// ---------------------------------------------------------------------------
// Optional runtime configuration (0.8.0). Defaults reproduce the historical
// baseline exactly: no scene file, legacy geodesic.comp, legacy constants.
//   --scene file.json   load black_hole.scene_params/v1 (camera, disk, objects, Gravity)
//   --scientific        use shaders/geodesic_scientific.comp (planar RK4, rs = 1 units)
//   --relativistic      scientific mode + Doppler/gravitational-redshift disk shading
// ---------------------------------------------------------------------------
struct RuntimeConfig {
    bool scientific = false;
    bool relativistic = false;
    bool blackbody = false;       // --blackbody: Page–Thorne colours (scientific mode)
    float exposure = 2.0f;        // --exposure
    float spinSign = 1.0f;        // --spin-sign ±1
    int maxSteps = 4000;          // --max-steps
    double mdotEdd = 0.01;        // --mdot-edd (fraction of Eddington, blackbody)
    bool sceneLoaded = false;
    std::string capturePath;  // --capture: render one frame, save the compute texture, exit
    bh::SceneParams scene = bh::make_default_scene_params();
};
RuntimeConfig g_config;
std::string g_exePath;  // argv[0], used to find shaders next to the executable

// The scientific shader ships next to the binary (CMake copies it) and under
// shaders/ in the repository. Try cwd, then shaders/, then the executable's
// directory, so `build/gl/BlackHole3D --scientific` also works from the repo root.
static std::string resolveScientificShader() {
    const std::string name = "geodesic_scientific.comp";
    std::vector<std::string> candidates = {name, "shaders/" + name};
    const std::size_t slash = g_exePath.find_last_of("/\\");
    if (slash != std::string::npos) candidates.push_back(g_exePath.substr(0, slash + 1) + name);
    for (const std::string& c : candidates) {
        if (std::ifstream(c).good()) return c;
    }
    return name;  // CreateComputeProgram reports the failure
}

struct alignas(16) SciParamsUBOData {
    std::int32_t colorMode;
    std::int32_t maxSteps;
    float stepK;
    float stepMin;
    float stepMax;
    float sceneBound;
    float exposure;
    float spinSign;
    float tempScale;
    float peakTemperature;
    float rOverMPerRs;
    float _pad5;
};
static_assert(sizeof(SciParamsUBOData) == 48, "SciParams UBO layout must match geodesic_scientific.comp.");

struct Camera {
    // Center the camera orbit on the black hole at (0, 0, 0)
    vec3 target = vec3(0.0f, 0.0f, 0.0f); // Always look at the black hole center
    // Scene-file overrides (--scene). Defaults keep the historical baseline:
    // look at the origin with a 60° vertical FOV.
    vec3 sceneTarget = vec3(0.0f, 0.0f, 0.0f);
    float fovYDeg = 60.0f;
    float radius = 6.34194e10f;
    float minRadius = 1e10f, maxRadius = 1e12f;

    float azimuth = 0.0f;
    float elevation = M_PI / 2.0f;

    float orbitSpeed = 0.01f;
    float panSpeed = 0.01f;
    double zoomSpeed = 25e9f;

    bool dragging = false;
    bool panning = false;
    bool moving = false; // For compute shader optimization
    double lastX = 0.0, lastY = 0.0;

    // Calculate camera position in world space
    vec3 position() const {
        float clampedElevation = glm::clamp(elevation, 0.01f, float(M_PI) - 0.01f);
        // Orbit around (0,0,0) always
        return vec3(
            radius * sin(clampedElevation) * cos(azimuth),
            radius * cos(clampedElevation),
            radius * sin(clampedElevation) * sin(azimuth)
        );
    }
    void update() {
        // Always keep target at black hole center
        target = sceneTarget;
        if(dragging || panning) {
            moving = true;
        } else {
            moving = false;
        }
    }

    void processMouseMove(double x, double y) {
        float dx = float(x - lastX);
        float dy = float(y - lastY);

        if (dragging && panning) {
            // Pan: Shift + Left or Middle Mouse
            // Disable panning to keep camera centered on black hole
        }
        else if (dragging && !panning) {
            // Orbit: Left mouse only
            azimuth   += dx * orbitSpeed;
            elevation -= dy * orbitSpeed;
            elevation = glm::clamp(elevation, 0.01f, float(M_PI) - 0.01f);
        }

        lastX = x;
        lastY = y;
        update();
    }
    void processMouseButton(int button, int action, int /*mods*/, GLFWwindow* win) {
        if (button == GLFW_MOUSE_BUTTON_LEFT || button == GLFW_MOUSE_BUTTON_MIDDLE) {
            if (action == GLFW_PRESS) {
                dragging = true;
                // Disable panning so camera always orbits center
                panning = false;
                glfwGetCursorPos(win, &lastX, &lastY);
            } else if (action == GLFW_RELEASE) {
                dragging = false;
                panning = false;
            }
        }
        if (button == GLFW_MOUSE_BUTTON_RIGHT) {
            if (action == GLFW_PRESS) {
                Gravity = true;
            } else if (action == GLFW_RELEASE) {
                Gravity = false;
            }
        }
    }
    void processScroll(double /*xoffset*/, double yoffset) {
        radius -= yoffset * zoomSpeed;
        radius = glm::clamp(radius, minRadius, maxRadius);
        update();
    }
    void processKey(int key, int /*scancode*/, int action, int /*mods*/) {
        if (action == GLFW_PRESS && key == GLFW_KEY_G) {
            Gravity = !Gravity;
            cout << "[INFO] Gravity turned " << (Gravity ? "ON" : "OFF") << endl;
        }
    }
};
Camera camera;

struct BlackHole {
    vec3 position;
    double mass;
    double radius;
    double r_s;

    BlackHole(vec3 pos, float m) : position(pos), mass(m) {r_s = 2.0 * G * mass / (c*c);}
    bool Intercept(float px, float py, float pz) const {
        double dx = double(px) - double(position.x);
        double dy = double(py) - double(position.y);
        double dz = double(pz) - double(position.z);
        double dist2 = dx * dx + dy * dy + dz * dz;
        return dist2 < r_s * r_s;
    }
};
BlackHole SagA(vec3(0.0f, 0.0f, 0.0f), 8.54e36); // Sagittarius A black hole
struct ObjectData {
    vec4 posRadius; // xyz = position, w = radius
    vec4 color;     // rgb = color, a = unused
    float  mass;
    vec3 velocity = vec3(0.0f, 0.0f, 0.0f); // Initial velocity
};

constexpr std::size_t MAX_SCENE_OBJECTS = 16;

// Explicit std140-compatible host layouts. Keeping these layouts centralized
// prevents accidental CPU/GLSL drift when the shader evolves.
struct alignas(16) CameraUBOData {
    vec4 pos;
    vec4 right;
    vec4 up;
    vec4 forward;
    float tanHalfFov;
    float aspect;
    std::int32_t moving;
    std::int32_t _pad4;
};

struct alignas(16) DiskUBOData {
    float disk_r1;
    float disk_r2;
    float disk_num;
    float thickness;
};

struct alignas(16) ObjectsUBOData {
    std::int32_t numObjects;
    float _pad0;
    float _pad1;
    float _pad2;
    vec4 posRadius[MAX_SCENE_OBJECTS];
    vec4 color[MAX_SCENE_OBJECTS];
    // std140 arrays have a 16-byte stride. vec4 makes that rule explicit.
    vec4 mass[MAX_SCENE_OBJECTS];
};

static_assert(sizeof(vec4) == 16, "Unexpected GLM vec4 size; std140 layout would be invalid.");
static_assert(sizeof(CameraUBOData) == 80, "Camera UBO layout must match geodesic.comp.");
static_assert(sizeof(DiskUBOData) == 16, "Disk UBO layout must match geodesic.comp.");
static_assert(sizeof(ObjectsUBOData) == 784, "Objects UBO layout must match geodesic.comp.");
vector<ObjectData> objects = {
    { vec4(4e11f, 0.0f, 0.0f, 4e10f)   , vec4(1,1,0,1), 1.98892e30 },
    { vec4(0.0f, 0.0f, 4e11f, 4e10f)   , vec4(1,0,0,1), 1.98892e30 },
    { vec4(0.0f, 0.0f, 0.0f, SagA.r_s) , vec4(0,0,0,1), static_cast<float>(SagA.mass)  },
    //{ vec4(6e10f, 0.0f, 0.0f, 5e10f), vec4(0,1,0,1) }
};

struct Engine {
    GLuint gridShaderProgram;
    // -- Quad & Texture render -- //
    GLFWwindow* window;
    GLuint quadVAO;
    GLuint texture;
    GLuint shaderProgram;
    GLuint computeProgram = 0;
    // -- UBOs -- //
    GLuint cameraUBO = 0;
    GLuint diskUBO = 0;
    GLuint objectsUBO = 0;
    GLuint sciUBO = 0;
    // -- grid mess vars -- //
    GLuint gridVAO = 0;
    GLuint gridVBO = 0;
    GLuint gridEBO = 0;
    int gridIndexCount = 0;
    bool gridTopologyInitialized = false;

    GLint gridViewProjLocation = -1;
    GLint screenTextureLocation = -1;
    int textureWidth = 0;
    int textureHeight = 0;
    bool objectsUBODirty = true;
    bool diskUBODirty = true;

    int WIDTH = 800;  // Window width
    int HEIGHT = 600; // Window height
    int COMPUTE_WIDTH  = 200;   // Compute resolution width
    int COMPUTE_HEIGHT = 150;  // Compute resolution height
    float width = 100000000000.0f; // Width of the viewport in meters
    float height = 75000000000.0f; // Height of the viewport in meters
    
    Engine() {
        if (!glfwInit()) {
            cerr << "GLFW init failed\n";
            exit(EXIT_FAILURE);
        }
        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
        glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
        window = glfwCreateWindow(WIDTH, HEIGHT, "Black Hole", nullptr, nullptr);
        if (!window) {
            cerr << "Failed to create GLFW window\n";
            glfwTerminate();
            exit(EXIT_FAILURE);
        }
        glfwMakeContextCurrent(window);
        glewExperimental = GL_TRUE;
        GLenum glewErr = glewInit();
        if (glewErr != GLEW_OK) {
            cerr << "Failed to initialize GLEW: "
                << (const char*)glewGetErrorString(glewErr)
                << "\n";
            glfwTerminate();
            exit(EXIT_FAILURE);
        }
        cout << "OpenGL " << glGetString(GL_VERSION) << "\n";
        cout << "Renderer " << glGetString(GL_RENDERER) << "\n";
        this->shaderProgram = CreateShaderProgram();
        gridShaderProgram = CreateShaderProgram("grid.vert", "grid.frag");

        computeProgram = CreateComputeProgram(g_config.scientific ? resolveScientificShader().c_str() : "geodesic.comp");
        if (g_config.scientific) {
            cout << "[INFO] scientific mode: geodesic_scientific.comp (planar RK4, rs=1 units)"
                 << (g_config.blackbody ? ", Page-Thorne blackbody disk"
                                        : (g_config.relativistic ? ", relativistic disk shading" : ""))
                 << "\n";
        }
        screenTextureLocation = glGetUniformLocation(shaderProgram, "screenTexture");
        gridViewProjLocation = glGetUniformLocation(gridShaderProgram, "viewProj");

        glGenBuffers(1, &cameraUBO);
        glBindBuffer(GL_UNIFORM_BUFFER, cameraUBO);
        glBufferData(GL_UNIFORM_BUFFER, sizeof(CameraUBOData), nullptr, GL_DYNAMIC_DRAW);
        glBindBufferBase(GL_UNIFORM_BUFFER, 1, cameraUBO); // binding = 1 matches shader

        glGenBuffers(1, &diskUBO);
        glBindBuffer(GL_UNIFORM_BUFFER, diskUBO);
        glBufferData(GL_UNIFORM_BUFFER, sizeof(DiskUBOData), nullptr, GL_DYNAMIC_DRAW);
        glBindBufferBase(GL_UNIFORM_BUFFER, 2, diskUBO); // binding = 2 matches compute shader

        glGenBuffers(1, &objectsUBO);
        glBindBuffer(GL_UNIFORM_BUFFER, objectsUBO);
        glBufferData(GL_UNIFORM_BUFFER, sizeof(ObjectsUBOData), nullptr, GL_DYNAMIC_DRAW);
        glBindBufferBase(GL_UNIFORM_BUFFER, 3, objectsUBO);  // binding = 3 matches shader

        glGenBuffers(1, &sciUBO);
        glBindBuffer(GL_UNIFORM_BUFFER, sciUBO);
        glBufferData(GL_UNIFORM_BUFFER, sizeof(SciParamsUBOData), nullptr, GL_DYNAMIC_DRAW);
        glBindBufferBase(GL_UNIFORM_BUFFER, 4, sciUBO);  // binding = 4 (scientific shader only)

        auto result = QuadVAO();
        this->quadVAO = result[0];
        this->texture = result[1];
        textureWidth = COMPUTE_WIDTH;
        textureHeight = COMPUTE_HEIGHT;
    }

    void markObjectsDirty() {
        objectsUBODirty = true;
    }

    void ensureComputeTextureSize(int width, int height) {
        if (width == textureWidth && height == textureHeight) {
            return;
        }

        glBindTexture(GL_TEXTURE_2D, texture);
        glTexImage2D(GL_TEXTURE_2D,
                    0,
                    GL_RGBA8,
                    width,
                    height,
                    0,
                    GL_RGBA,
                    GL_UNSIGNED_BYTE,
                    nullptr);
        textureWidth = width;
        textureHeight = height;
    }

    void generateGrid(const vector<ObjectData>& objects) {
        const int gridSize = 25;
        const float spacing = 1e10f;  // tweak this

        vector<vec3> vertices;
        vertices.reserve((gridSize + 1) * (gridSize + 1));

        for (int z = 0; z <= gridSize; ++z) {
            for (int x = 0; x <= gridSize; ++x) {
                float worldX = (x - gridSize / 2) * spacing;
                float worldZ = (z - gridSize / 2) * spacing;

                float y = 0.0f;

                // ✅ Warp grid using Schwarzschild geometry
                for (const auto& obj : objects) {
                    vec3 objPos = vec3(obj.posRadius);
                    double mass = obj.mass;
                    double r_s = 2.0 * G * mass / (c * c);
                    double dx = worldX - objPos.x;
                    double dz = worldZ - objPos.z;
                    double dist = sqrt(dx * dx + dz * dz);

                    // prevent sqrt of negative or divide-by-zero (inside or at the black hole center)
                    if (dist > r_s) {
                        double deltaY = 2.0 * sqrt(r_s * (dist - r_s));
                        y += static_cast<float>(deltaY) - 3e10f;
                    } else {
                        // 🔴 For points inside or at r_s: make it dip down sharply
                        y += 2.0f * static_cast<float>(sqrt(r_s * r_s)) - 3e10f;  // or add a deep pit
                    }
                }

                vertices.emplace_back(worldX, y, worldZ);
            }
        }

        if (!gridTopologyInitialized) {
            vector<GLuint> indices;
            indices.reserve(gridSize * gridSize * 4);

            // 🧩 Add indices for GL_LINE rendering once: topology never changes.
            for (int z = 0; z < gridSize; ++z) {
                for (int x = 0; x < gridSize; ++x) {
                    int i = z * (gridSize + 1) + x;
                    indices.push_back(i);
                    indices.push_back(i + 1);

                    indices.push_back(i);
                    indices.push_back(i + gridSize + 1);
                }
            }

            glGenVertexArrays(1, &gridVAO);
            glGenBuffers(1, &gridVBO);
            glGenBuffers(1, &gridEBO);

            glBindVertexArray(gridVAO);

            glBindBuffer(GL_ARRAY_BUFFER, gridVBO);
            glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(vec3), vertices.data(), GL_DYNAMIC_DRAW);

            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, gridEBO);
            glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(GLuint), indices.data(), GL_STATIC_DRAW);

            glEnableVertexAttribArray(0); // location = 0
            glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(vec3), (void*)0);

            gridIndexCount = static_cast<int>(indices.size());
            gridTopologyInitialized = true;
            glBindVertexArray(0);
            return;
        }

        // Only vertex heights/positions can change after initialization.
        glBindBuffer(GL_ARRAY_BUFFER, gridVBO);
        glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(vec3), vertices.data(), GL_DYNAMIC_DRAW);
    }
    void drawGrid(const mat4& viewProj) {
        glUseProgram(gridShaderProgram);
        glUniformMatrix4fv(gridViewProjLocation, 1, GL_FALSE, glm::value_ptr(viewProj));
        glBindVertexArray(gridVAO);

        glDisable(GL_DEPTH_TEST);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        glDrawElements(GL_LINES, gridIndexCount, GL_UNSIGNED_INT, 0);

        glBindVertexArray(0);
        glEnable(GL_DEPTH_TEST);
    }
    void drawFullScreenQuad() {
        glUseProgram(shaderProgram); // fragment + vertex shader
        glBindVertexArray(quadVAO);

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, texture);
        glUniform1i(screenTextureLocation, 0);

        glDisable(GL_DEPTH_TEST);  // draw as background
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 6);  // 2 triangles
        glEnable(GL_DEPTH_TEST);
    }
    GLuint CreateShaderProgram(){
        const char* vertexShaderSource = R"(
        #version 330 core
        layout (location = 0) in vec2 aPos;  // Changed to vec2
        layout (location = 1) in vec2 aTexCoord;
        out vec2 TexCoord;
        void main() {
            gl_Position = vec4(aPos, 0.0, 1.0);  // Explicit z=0
            TexCoord = aTexCoord;
        })";

        const char* fragmentShaderSource = R"(
        #version 330 core
        in vec2 TexCoord;
        out vec4 FragColor;
        uniform sampler2D screenTexture;
        void main() {
            FragColor = texture(screenTexture, TexCoord);
        })";

        // vertex shader
        GLuint vertexShader = glCreateShader(GL_VERTEX_SHADER);
        glShaderSource(vertexShader, 1, &vertexShaderSource, nullptr);
        glCompileShader(vertexShader);

        // fragment shader
        GLuint fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
        glShaderSource(fragmentShader, 1, &fragmentShaderSource, nullptr);
        glCompileShader(fragmentShader);

        GLuint shaderProgram = glCreateProgram();
        glAttachShader(shaderProgram, vertexShader);
        glAttachShader(shaderProgram, fragmentShader);
        glLinkProgram(shaderProgram);

        glDeleteShader(vertexShader);
        glDeleteShader(fragmentShader);

        return shaderProgram;
    };
    GLuint CreateShaderProgram(const char* vertPath, const char* fragPath) {
        auto loadShader = [](const char* path, GLenum type) -> GLuint {
            std::ifstream in(path);
            if (!in.is_open()) {
                std::cerr << "Failed to open shader: " << path << "\n";
                exit(EXIT_FAILURE);
            }
            std::stringstream ss;
            ss << in.rdbuf();
            std::string srcStr = ss.str();
            const char* src = srcStr.c_str();

            GLuint shader = glCreateShader(type);
            glShaderSource(shader, 1, &src, nullptr);
            glCompileShader(shader);

            GLint success;
            glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
            if (!success) {
                GLint logLen;
                glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &logLen);
                std::vector<char> log(logLen);
                glGetShaderInfoLog(shader, logLen, nullptr, log.data());
                std::cerr << "Shader compile error (" << path << "):\n" << log.data() << "\n";
                exit(EXIT_FAILURE);
            }
            return shader;
        };

        GLuint vertShader = loadShader(vertPath, GL_VERTEX_SHADER);
        GLuint fragShader = loadShader(fragPath, GL_FRAGMENT_SHADER);

        GLuint program = glCreateProgram();
        glAttachShader(program, vertShader);
        glAttachShader(program, fragShader);
        glLinkProgram(program);

        GLint linkSuccess;
        glGetProgramiv(program, GL_LINK_STATUS, &linkSuccess);
        if (!linkSuccess) {
            GLint logLen;
            glGetProgramiv(program, GL_INFO_LOG_LENGTH, &logLen);
            std::vector<char> log(logLen);
            glGetProgramInfoLog(program, logLen, nullptr, log.data());
            std::cerr << "Shader link error:\n" << log.data() << "\n";
            exit(EXIT_FAILURE);
        }

        glDeleteShader(vertShader);
        glDeleteShader(fragShader);

        return program;
    }
    GLuint CreateComputeProgram(const char* path) {
        // 1) read GLSL source
        std::ifstream in(path);
        if(!in.is_open()) {
            std::cerr << "Failed to open compute shader: " << path << "\n";
            exit(EXIT_FAILURE);
        }
        std::stringstream ss;
        ss << in.rdbuf();
        std::string srcStr = ss.str();
        const char* src = srcStr.c_str();

        // 2) compile
        GLuint cs = glCreateShader(GL_COMPUTE_SHADER);
        glShaderSource(cs, 1, &src, nullptr);
        glCompileShader(cs);
        GLint ok; 
        glGetShaderiv(cs, GL_COMPILE_STATUS, &ok);
        if(!ok) {
            GLint logLen;
            glGetShaderiv(cs, GL_INFO_LOG_LENGTH, &logLen);
            std::vector<char> log(logLen);
            glGetShaderInfoLog(cs, logLen, nullptr, log.data());
            std::cerr << "Compute shader compile error:\n" << log.data() << "\n";
            exit(EXIT_FAILURE);
        }

        // 3) link
        GLuint prog = glCreateProgram();
        glAttachShader(prog, cs);
        glLinkProgram(prog);
        glGetProgramiv(prog, GL_LINK_STATUS, &ok);
        if(!ok) {
            GLint logLen;
            glGetProgramiv(prog, GL_INFO_LOG_LENGTH, &logLen);
            std::vector<char> log(logLen);
            glGetProgramInfoLog(prog, logLen, nullptr, log.data());
            std::cerr << "Compute shader link error:\n" << log.data() << "\n";
            exit(EXIT_FAILURE);
        }

        glDeleteShader(cs);
        return prog;
    }
    void dispatchCompute(const Camera& cam) {
        // determine target compute‐res
        int cw = cam.moving ? COMPUTE_WIDTH  : 200;
        int ch = cam.moving ? COMPUTE_HEIGHT : 150;

        // 1) Preserve the texture allocation while resolution is unchanged.
        ensureComputeTextureSize(cw, ch);

        // 2) bind compute program & UBOs
        glUseProgram(computeProgram);
        uploadCameraUBO(cam);
        if (diskUBODirty) {
            uploadDiskUBO();
            diskUBODirty = false;
        }
        if (objectsUBODirty) {
            uploadObjectsUBO(objects);
            objectsUBODirty = false;
            if (g_config.scientific) {
                uploadSciUBO();
            }
        }

        // 3) bind it as image unit 0
        glBindImageTexture(0, texture, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA8);

        // 4) dispatch grid
        GLuint groupsX = (GLuint)std::ceil(cw / 16.0f);
        GLuint groupsY = (GLuint)std::ceil(ch / 16.0f);
        glDispatchCompute(groupsX, groupsY, 1);

        // 5) sync
        // The compute shader writes through imageStore and the following draw
        // consumes the same texture through texture(). The barrier therefore
        // must cover texture fetch visibility as well as image access.
        glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT | GL_TEXTURE_FETCH_BARRIER_BIT);
    }
    // Scientific shader works in rs = 1 units; legacy shader in metres.
    // Uses SceneParams::r_s_m (default: the legacy shader literal 1.269e10 m),
    // exactly like bh_render_cpu, so CPU and GPU scientific renders share units.
    float unitScale() const {
        return g_config.scientific ? static_cast<float>(1.0 / g_config.scene.r_s_m) : 1.0f;
    }
    void uploadSciUBO() {
        SciParamsUBOData d{};
        d.colorMode = g_config.blackbody ? 2 : (g_config.relativistic ? 1 : 0);
        d.maxSteps = g_config.maxSteps;
        d.stepK = 0.02f;
        d.stepMin = 0.005f;
        d.stepMax = 2.0f;
        float bound = static_cast<float>(g_config.scene.disk_outer_factor_rs);
        for (const auto& o : objects) {
            const float c = glm::length(vec3(o.posRadius)) + o.posRadius.w;
            bound = std::max(bound, c * unitScale());
        }
        d.sceneBound = bound * 1.05f + 0.5f;
        d.exposure = g_config.exposure;
        d.spinSign = g_config.spinSign;
        // Page–Thorne normalisation shared with bh_render_cpu (same scene, Ṁ, rs).
        bh::render::Options bb;
        bb.scene = g_config.scene;
        bb.mdot_edd_fraction = g_config.mdotEdd;
        const double G = bh::units::G_SI, c = bh::units::C_SI, M = g_config.scene.mass_kg;
        const double rs_m = g_config.scene.r_s_m;
        const double mdot = g_config.mdotEdd *
            bh::disk_emission::eddington_accretion_rate_si(M, G, c, bh::orbits::thin_disk_efficiency());
        const double pi = 3.14159265358979323846;
        d.tempScale = static_cast<float>(std::pow(3.0 * G * M * mdot /
            (8.0 * pi * bh::disk_emission::SIGMA_SB_SI * rs_m * rs_m * rs_m), 0.25));
        d.peakTemperature = static_cast<float>(bh::render::peak_effective_temperature(bb));
        d.rOverMPerRs = static_cast<float>(rs_m * c * c / (G * M));
        glBindBuffer(GL_UNIFORM_BUFFER, sciUBO);
        glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(d), &d);
    }
    void uploadCameraUBO(const Camera& cam) {
        CameraUBOData data{};
        vec3 fwd = normalize(cam.target - cam.position());
        vec3 up = vec3(0, 1, 0); // y axis is up, so disk is in x-z plane
        vec3 right = normalize(cross(fwd, up));
        up = cross(right, fwd);

        data.pos = vec4(cam.position() * unitScale(), 0.0f);
        data.right = vec4(right, 0.0f);
        data.up = vec4(up, 0.0f);
        data.forward = vec4(fwd, 0.0f);
        // Computed in double then rounded once: identical to the historical
        // compile-time constant tan(radians(60.0f * 0.5f)) (tan 30° is a
        // hard-to-round case where runtime tanf is 1 ulp off).
        data.tanHalfFov = static_cast<float>(std::tan(static_cast<double>(radians(cam.fovYDeg * 0.5f))));
        data.aspect = float(WIDTH) / float(HEIGHT);
        data.moving = (cam.dragging || cam.panning) ? 1 : 0;

        glBindBuffer(GL_UNIFORM_BUFFER, cameraUBO);
        glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(data), &data);
    }
    void uploadObjectsUBO(const vector<ObjectData>& objs) {
        ObjectsUBOData data{};

        // Legacy mode uploads every object (the black marker sphere included).
        // Scientific mode skips the black-hole marker: the shader's exact horizon
        // test draws the hole, using the SAME predicate as bh_render_cpu.
        std::size_t count = 0;
        for (const ObjectData& od : objs) {
            if (count >= MAX_SCENE_OBJECTS) break;
            if (g_config.scientific) {
                bh::SceneObject so;
                so.pos_m = {{od.posRadius.x, od.posRadius.y, od.posRadius.z}};
                so.radius_m = od.posRadius.w;
                so.mass_kg = od.mass;
                if (bh::is_black_hole_marker(so, g_config.scene)) continue;
            }
            data.posRadius[count] = od.posRadius * unitScale();
            data.color[count] = od.color;
            data.mass[count] = vec4(od.mass, 0.0f, 0.0f, 0.0f);
            ++count;
        }
        data.numObjects = static_cast<std::int32_t>(count);

        // Upload
        glBindBuffer(GL_UNIFORM_BUFFER, objectsUBO);
        glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(data), &data);
    }
    void uploadDiskUBO() {
        DiskUBOData diskData{};
        diskData.disk_r1 = SagA.r_s * 2.2f;  // inner radius just outside the event horizon
        diskData.disk_r2 = SagA.r_s * 5.2f;  // outer radius of the disk
        diskData.disk_num = 2.0f;
        diskData.thickness = 1e9f;
        if (g_config.sceneLoaded) {
            // JSON overrides (legacy literals above stay the documented defaults).
            diskData.disk_r1 = SagA.r_s * static_cast<float>(g_config.scene.disk_inner_factor_rs);
            diskData.disk_r2 = SagA.r_s * static_cast<float>(g_config.scene.disk_outer_factor_rs);
            diskData.disk_num = static_cast<float>(g_config.scene.disk_num);
            diskData.thickness = static_cast<float>(g_config.scene.disk_thickness_m);
        }
        if (g_config.scientific) {
            // rs = 1 units: the annulus is exactly the configured factors.
            diskData.disk_r1 = static_cast<float>(g_config.scene.disk_inner_factor_rs);
            diskData.disk_r2 = static_cast<float>(g_config.scene.disk_outer_factor_rs);
            diskData.thickness *= unitScale();
        }

        glBindBuffer(GL_UNIFORM_BUFFER, diskUBO);
        glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(diskData), &diskData);
    }
    
    vector<GLuint> QuadVAO(){
        float quadVertices[] = {
            // positions   // texCoords
            -1.0f,  1.0f,  0.0f, 1.0f,  // top left
            -1.0f, -1.0f,  0.0f, 0.0f,  // bottom left
            1.0f, -1.0f,  1.0f, 0.0f,  // bottom right

            -1.0f,  1.0f,  0.0f, 1.0f,  // top left
            1.0f, -1.0f,  1.0f, 0.0f,  // bottom right
            1.0f,  1.0f,  1.0f, 1.0f   // top right
        };
        
        GLuint VAO, VBO;
        glGenVertexArrays(1, &VAO);
        glGenBuffers(1, &VBO);

        glBindVertexArray(VAO);
        glBindBuffer(GL_ARRAY_BUFFER, VBO);
        glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), quadVertices, GL_STATIC_DRAW);

        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
        glEnableVertexAttribArray(1);

        GLuint texture;
        glGenTextures(1, &texture);
        glBindTexture(GL_TEXTURE_2D, texture);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glBindTexture(GL_TEXTURE_2D, texture);
        glTexImage2D(GL_TEXTURE_2D,
                    0,             // mip
                    GL_RGBA8,      // internal format
                    COMPUTE_WIDTH,
                    COMPUTE_HEIGHT,
                    0,
                    GL_RGBA,
                    GL_UNSIGNED_BYTE,
                    nullptr);
        vector<GLuint> VAOtexture = {VAO, texture};
        return VAOtexture;
    }
    void renderScene() {
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glUseProgram(shaderProgram);
        glBindVertexArray(quadVAO);
        // make sure your fragment shader samples from texture unit 0:
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, texture);
        glDrawArrays(GL_TRIANGLES, 0, 6);
        glfwSwapBuffers(window);
        glfwPollEvents();
    };
};
static void printUsage() {
    cout << "BlackHole3D [--scene file.json] [--scientific] [--relativistic] [--blackbody]\n"
            "            [--exposure E] [--spin-sign +1|-1] [--max-steps N] [--mdot-edd F]\n"
            "            [--capture out.png] [--help]\n"
            "  --scene        load black_hole.scene_params/v1 (camera, disk, objects, Gravity)\n"
            "  --scientific   planar RK4 compute shader (geodesic_scientific.comp), rs=1 units\n"
            "  --relativistic scientific + Doppler/gravitational redshift disk shading\n"
            "  --blackbody    scientific + Page-Thorne blackbody disk (g-shifted, dark inside the ISCO)\n"
            "  --exposure E   Reinhard exposure for --relativistic/--blackbody (default 2)\n"
            "  --spin-sign S  disk rotation about +Y: +1 (default) or -1\n"
            "  --max-steps N  RK4 step budget per ray in scientific mode (default 4000)\n"
            "  --mdot-edd F   accretion rate as a fraction of Eddington for --blackbody (default 0.01)\n"
            "  --capture      render one frame, save the 200x150 compute image (.png/.bmp/.ppm), exit\n"
            "  (no flags)     historical baseline: geodesic.comp legacyEulerStep\n";
}

enum class ParseResult { Ok, Help, Error };

static std::string lowerExtension(const std::string& path) {
    const std::size_t dot = path.find_last_of('.');
    std::string ext = (dot == std::string::npos) ? "" : path.substr(dot);
    std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return ext;
}

static ParseResult parseArgs(int argc, char** argv) {
    // --help anywhere wins (exit 0), before any other validation.
    for (int i = 1; i < argc; ++i) {
        const std::string a = argv[i];
        if (a == "--help" || a == "-h") {
            printUsage();
            return ParseResult::Help;
        }
    }
    for (int i = 1; i < argc; ++i) {
        const std::string a = argv[i];
        const bool takesValue = (a == "--capture" || a == "--scene" || a == "--exposure" ||
                                 a == "--spin-sign" || a == "--max-steps" || a == "--mdot-edd");
        if (takesValue && i + 1 >= argc) {
            cerr << "[ERROR] missing value for " << a << "\n";
            return ParseResult::Error;
        }
        if (a == "--scientific") {
            g_config.scientific = true;
        } else if (a == "--relativistic") {
            g_config.scientific = true;
            g_config.relativistic = true;
        } else if (a == "--blackbody") {
            g_config.scientific = true;
            g_config.blackbody = true;
        } else if (a == "--exposure" || a == "--spin-sign" || a == "--max-steps" || a == "--mdot-edd") {
            const std::string v = argv[++i];
            char* end = nullptr;
            const double x = std::strtod(v.c_str(), &end);
            const bool parsed = end && *end == '\0' && !v.empty() && std::isfinite(x);
            bool valid = parsed;
            if (a == "--exposure") valid = valid && x >= 0.0;
            if (a == "--spin-sign") valid = valid && (x == 1.0 || x == -1.0);
            if (a == "--max-steps") valid = valid && x >= 1.0 && x <= 1e7 && x == std::floor(x);
            if (a == "--mdot-edd") valid = valid && x > 0.0;
            if (!valid) {
                cerr << "[ERROR] bad value for " << a << ": '" << v << "'\n";
                return ParseResult::Error;
            }
            if (a == "--exposure") g_config.exposure = static_cast<float>(x);
            if (a == "--spin-sign") g_config.spinSign = static_cast<float>(x);
            if (a == "--max-steps") g_config.maxSteps = static_cast<int>(x);
            if (a == "--mdot-edd") g_config.mdotEdd = x;
        } else if (a == "--capture") {
            g_config.capturePath = argv[++i];
            const std::string ext = lowerExtension(g_config.capturePath);
            if (ext != ".png" && ext != ".bmp" && ext != ".ppm") {
                // Fail before creating a window / rendering a frame.
                cerr << "[ERROR] unsupported capture extension (use .png, .bmp or .ppm): " << g_config.capturePath << "\n";
                return ParseResult::Error;
            }
        } else if (a == "--scene") {
            std::string err;
            if (!bh::load_scene_params_json(argv[++i], g_config.scene, err)) {
                cerr << "[ERROR] scene load failed: " << err << "\n";
                return ParseResult::Error;
            }
            g_config.sceneLoaded = true;
        } else {
            cerr << "[ERROR] unknown argument: " << a << "\n";
            printUsage();
            return ParseResult::Error;
        }
    }
    return ParseResult::Ok;
}

// Apply a loaded scene to the global baseline state (camera, black hole, objects, Gravity).
static void applySceneToGlobals() {
    const bh::SceneParams& sc = g_config.scene;
    SagA = BlackHole(vec3(sc.bh_position_m[0], sc.bh_position_m[1], sc.bh_position_m[2]),
                     static_cast<float>(sc.mass_kg));
    camera.radius = static_cast<float>(sc.camera.radius_m);
    camera.azimuth = static_cast<float>(sc.camera.azimuth_rad);
    camera.elevation = static_cast<float>(sc.camera.elevation_rad);
    camera.fovYDeg = static_cast<float>(sc.camera.fov_y_deg);
    camera.sceneTarget = vec3(sc.camera.target_x, sc.camera.target_y, sc.camera.target_z);
    camera.update();
    Gravity = sc.gravity;
    // The loader already supplies the three default objects (marker resized to
    // the loaded hole) when the JSON has no "objects" key, so an explicit
    // "objects": [] means "no objects" — exactly like bh_render_cpu.
    objects.clear();
    for (const bh::SceneObject& o : sc.objects) {
        ObjectData od;
        od.posRadius = vec4(o.pos_m[0], o.pos_m[1], o.pos_m[2], o.radius_m);
        od.color = vec4(o.color_rgba[0], o.color_rgba[1], o.color_rgba[2], o.color_rgba[3]);
        od.mass = static_cast<float>(o.mass_kg);
        objects.push_back(od);
    }
    cout << "[INFO] scene loaded: " << sc.bh_name << " rs=" << SagA.r_s << " m, camera R=" << camera.radius
         << " az=" << camera.azimuth << " el=" << camera.elevation << ", " << objects.size()
         << " objects, Gravity " << (Gravity ? "ON" : "OFF") << "\n";
}

void setupCameraCallbacks(GLFWwindow* window) {
    glfwSetWindowUserPointer(window, &camera);

    glfwSetMouseButtonCallback(window, [](GLFWwindow* win, int button, int action, int mods) {
        Camera* cam = (Camera*)glfwGetWindowUserPointer(win);
        cam->processMouseButton(button, action, mods, win);
    });

    glfwSetCursorPosCallback(window, [](GLFWwindow* win, double x, double y) {
        Camera* cam = (Camera*)glfwGetWindowUserPointer(win);
        cam->processMouseMove(x, y);
    });

    glfwSetScrollCallback(window, [](GLFWwindow* win, double xoffset, double yoffset) {
        Camera* cam = (Camera*)glfwGetWindowUserPointer(win);
        cam->processScroll(xoffset, yoffset);
    });

    glfwSetKeyCallback(window, [](GLFWwindow* win, int key, int scancode, int action, int mods) {
        Camera* cam = (Camera*)glfwGetWindowUserPointer(win);
        cam->processKey(key, scancode, action, mods);
    });
}


// -- MAIN -- //
int main(int argc, char** argv) {
    g_exePath = (argc > 0 && argv[0]) ? argv[0] : "";
    const ParseResult parsed = parseArgs(argc, argv);
    if (parsed != ParseResult::Ok) {
        return parsed == ParseResult::Help ? 0 : 2;
    }
    if (g_config.sceneLoaded) {
        applySceneToGlobals();
    }
    Engine engine;  // constructed after the CLI so the compute program matches the mode
    setupCameraCallbacks(engine.window);

    auto t0 = Clock::now();
    lastPrintTime = chrono::duration<double>(t0.time_since_epoch()).count();

    // The grid is static while scene objects are static. Build it once and
    // regenerate only after the legacy gravity mode actually moves objects.
    engine.generateGrid(objects);

    while (!glfwWindowShouldClose(engine.window)) {
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);  // optional, but good practice
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // Gravity
        bool objectsChanged = false;
        for (auto& obj : objects) {
            for (auto& obj2 : objects) {
                if (&obj == &obj2) continue; // skip self-interaction
                 float dx  = obj2.posRadius.x - obj.posRadius.x;
                 float dy = obj2.posRadius.y - obj.posRadius.y;
                 float dz = obj2.posRadius.z - obj.posRadius.z;
                 float distance = sqrt(dx * dx + dy * dy + dz * dz);
                 if (distance > 0) {
                        vector<double> direction = {dx / distance, dy / distance, dz / distance};
                        //distance *= 1000;
                        double Gforce = (G * obj.mass * obj2.mass) / (distance * distance);

                        double acc1 = Gforce / obj.mass;
                        std::vector<double> acc = {direction[0] * acc1, direction[1] * acc1, direction[2] * acc1};
                        if (Gravity) {
                            obj.velocity.x += acc[0];
                            obj.velocity.y += acc[1];
                            obj.velocity.z += acc[2];

                            obj.posRadius.x += obj.velocity.x;
                            obj.posRadius.y += obj.velocity.y;
                            obj.posRadius.z += obj.velocity.z;
                            objectsChanged = true;
                            cout << "velocity: " <<obj.velocity.x<<", " <<obj.velocity.y<<", " <<obj.velocity.z<<endl;
                        }
                    }
            }
        }



        // ---------- GRID ------------- //
        // 2) rebuild grid mesh only when the legacy gravity path moved objects.
        if (objectsChanged) {
            engine.generateGrid(objects);
            engine.markObjectsDirty();
        }
        // 5) overlay the bent grid
        mat4 view = lookAt(camera.position(), camera.target, vec3(0,1,0));
        // Keep the historical literal on the default path (byte-identical grid).
        const float gridAspect = float(engine.COMPUTE_WIDTH) / engine.COMPUTE_HEIGHT;
        mat4 proj = (camera.fovYDeg == 60.0f) ? perspective(radians(60.0f), gridAspect, 1e9f, 1e14f)
                                              : perspective(radians(camera.fovYDeg), gridAspect, 1e9f, 1e14f);
        mat4 viewProj = proj * view;
        engine.drawGrid(viewProj);

        // ---------- RUN RAYTRACER ------------- //
        glViewport(0, 0, engine.WIDTH, engine.HEIGHT);
        engine.dispatchCompute(camera);
        engine.drawFullScreenQuad();

        if (!g_config.capturePath.empty()) {
            // Golden-image capture: read back the raw compute texture. The fullscreen
            // quad maps texture row 0 to the BOTTOM of the window (texcoord v = 0), so
            // rows are flipped to produce the on-screen orientation (row 0 = top),
            // which is also the bh_render_cpu convention → 1:1 GPU/CPU comparison.
            glFinish();
            const int w = engine.textureWidth, h = engine.textureHeight;
            std::vector<unsigned char> rgba(static_cast<std::size_t>(w) * h * 4);
            glBindTexture(GL_TEXTURE_2D, engine.texture);
            glPixelStorei(GL_PACK_ALIGNMENT, 1);
            // imageStore → GetTexImage visibility requires TEXTURE_UPDATE (GL 4.3 §7.12.2).
            glMemoryBarrier(GL_TEXTURE_UPDATE_BARRIER_BIT);
            glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, rgba.data());
            // Compose like the fullscreen pass: RGB weighted by alpha over the black clear colour.
            std::vector<std::uint8_t> rgb8(static_cast<std::size_t>(w) * h * 3);
            for (int y = 0; y < h; ++y) {
                for (int x = 0; x < w; ++x) {
                    const std::size_t src = (static_cast<std::size_t>(h - 1 - y) * w + x) * 4;
                    const std::size_t dst = (static_cast<std::size_t>(y) * w + x) * 3;
                    const unsigned a = rgba[src + 3];
                    for (int c = 0; c < 3; ++c) {
                        rgb8[dst + c] = static_cast<std::uint8_t>((rgba[src + c] * a + 127u) / 255u);
                    }
                }
            }
            std::string err;
            const std::string& out = g_config.capturePath;
            const std::string ext = lowerExtension(out);
            bool ok = false;
            if (ext == ".png") ok = bh::image_io::write_png(out, w, h, rgb8, err);
            else if (ext == ".bmp") ok = bh::image_io::write_bmp(out, w, h, rgb8, err);
            else if (ext == ".ppm") ok = bh::image_io::write_ppm(out, w, h, rgb8, err);
            else err = "unsupported capture extension (use .png, .bmp or .ppm)";
            if (!ok) {
                cerr << "[ERROR] capture failed: " << err << "\n";
                glfwDestroyWindow(engine.window);
                glfwTerminate();
                return 1;
            }
            cout << "[INFO] captured " << w << "x" << h << " -> " << out << "\n";
            break;
        }

        // 6) present to screen
        glfwSwapBuffers(engine.window);
        glfwPollEvents();
    }

    glfwDestroyWindow(engine.window);
    glfwTerminate();
    return 0;
}
