#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <vector>
#include <iostream>
#include <cmath>
using namespace glm;

// vars
const int WIDTH = 800;
const int HEIGHT = 600;
glm::vec3 cameraPos   = glm::vec3(0.0f, 0.0f,  1.0f);
glm::vec3 cameraFront = glm::vec3(0.0f, 0.0f, -1.0f);
glm::vec3 cameraUp    = glm::vec3(0.0f, 1.0f,  0.0f);
float lastX = 400.0, lastY = 300.0;
float cameraYaw = -90;
float cameraPitch = 0.0;
glm::vec3 target = glm::vec3(0.0f, 0.0f, 0.0f); // the pivot point for orbiting
float radius = 10.0f;                         // the distance from the pivot point
float deltaTime = 0.0;
float lastFrame = 0.0;

// functions
GLFWwindow* StartGLU();
GLuint CreateShaderProgram();
GLuint setupQuad();
void renderScene(GLFWwindow* &window, GLuint &quadVAO, GLuint &texture, GLuint &shaderProgram, std::vector<unsigned char> &pixels);
GLuint loadTexture();

void UpdateCam(GLuint shaderProgram, glm::vec3 cameraPos);
void keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods);
void mouse_callback(GLFWwindow* window, double xpos, double ypos);
void scroll_callback(GLFWwindow* window, double xoffset, double yoffset);

// structures and classes :D
struct Ray{
    vec3 direction;
    vec3 origin;
    Ray(vec3 direction, vec3 origin) : direction(direction), origin(origin){}
};
struct Material{
    vec3 color;
    float specular;
    vec3 emission = vec3(0.0f, 0.0f, 0.0f);
    Material(glm::vec3 c, float s, vec3 e) : color(c), specular(s), emission(e) {}
};
struct Object{
    vec3 centre;
    float radius;
    Material material;

    Object(vec3 centre, float radius, Material m) : centre(centre), radius(radius), material(m) {}
    bool intersect(const Ray &ray, float& t){
        vec3 oc = ray.origin - centre;
        double a = dot(ray.direction, ray.direction);
        double b = 2.0 * dot(oc, ray.direction);
        double c = dot(oc, oc) - radius * radius;

        double discriminant = b*b - 4 * a * c;
        if (discriminant < 0) return false;
        
        float temp = (-b - sqrt(discriminant)) / (2.0f*a);
        if (temp < 0) {
            temp = (-b + sqrt(discriminant)) / (2.0f*a);
            if (temp < 0) return false;
        }
        t = temp;
        return true;
    }
    
    vec3 getNormal(const glm::vec3& point) const {
        return normalize(point - centre); // direction point to centre
    }
};
class Scene{
public:
    std::vector<Object> objs;

    Scene() {}

    // given light-ray, find color by tracing what it contacts
    vec3 trace(const Ray& ray) const{
        float closest = INFINITY;             // depth var
        const Object* hitObject = nullptr;
        // identify object hit by ray
        for(const auto& obj : objs){
            float t;                          // distance to intersection
            Object mutableObj = obj;           // non-const copy to call intersect
            if(mutableObj.intersect(ray, t)) {
                if(t < closest) {           // if obj intersects, and is closest in depth
                    closest = t;
                    hitObject = &obj;
                }
            }
        };
        
        // derive color
        if (hitObject) {
            vec3 hitPoint = ray.origin + ray.direction * closest;    // point ray hit the sphere
            vec3 normal = hitObject->getNormal(hitPoint);

            // base ambient lighting
            float ambient = 0.1f;
            vec3 finalColor = hitObject->material.color * ambient;
            finalColor += hitObject->material.emission;

            for (const auto& obj : objs) {
                if (length(obj.material.emission) < 0.001f) continue; // black light: no emmision
                
                }
            }
            
            if (inShadow) {
                return color * ambient;
            }
            
            return color * (ambient + diff * 0.9f);
        }
        return vec3(0.0f);             // defult black background
    }
};

// --------- main --------- //
int main() {
    // setup
    GLFWwindow* window = StartGLU();
    Scene scene;
    GLuint shaderProgram = CreateShaderProgram(); // compile shader program
    GLuint quadVAO = setupQuad(); // create quad background
    GLuint texture = loadTexture();
    glfwSetKeyCallback(window, keyCallback);
    glfwSetCursorPosCallback(window, mouse_callback);
    glfwSetScrollCallback(window, scroll_callback);
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
    float radYaw = glm::radians(cameraYaw);
    float radPitch = glm::radians(cameraPitch);
    cameraPos.x = target.x + radius * cos(radPitch) * cos(radYaw);
    cameraPos.y = target.y + radius * sin(radPitch);
    cameraPos.z = target.z + radius * cos(radPitch) * sin(radYaw);
    cameraFront = glm::normalize(target - cameraPos);

    std::vector<unsigned char> pixels(WIDTH * HEIGHT * 3);
    scene.objs = {
        Object(glm::vec3(0.0f, 0.0f, -6.0f), 1.0f, Material(vec3(1.0f, 0.2f, 0.2f), 0.9)),
        Object(glm::vec3(2.0f, 0.0f, -6.0f), 1.0f, Material(vec3(0.2f, 0.2f, 1.0f), 0.9))
    };
    
    while(!glfwWindowShouldClose(window)){
        glClear(GL_COLOR_BUFFER_BIT);
        UpdateCam(shaderProgram, cameraPos);

        // ray tracing
        vec3 cameraRight = normalize(cross(cameraFront, cameraUp));
        float aspectRatio = float(WIDTH) / float(HEIGHT);
        float fov = 45.0f;
        float halfHeight = tan(radians(fov / 2.0f));
        float halfWidth = aspectRatio * halfHeight;

        for(int y = 0; y < HEIGHT; ++y){
            for(int x = 0; x < WIDTH; ++x){
                float u = float(x) / float(WIDTH); // normalize 0-1
                float v = float(y) / float(HEIGHT);  // normalize 0-1
                
                vec3 direction = normalize(
                    cameraFront +
                    (2.0f * u - 1.0f) * halfWidth * cameraRight +
                    (1.0f - 2.0f * v) * halfHeight * cameraUp
                );
                
                Ray ray(glm::normalize(direction), cameraPos);
                vec3 color = scene.trace(ray);
            
                int index = (y * WIDTH + x) * 3;
                pixels[index + 0] = static_cast<unsigned char>(color.r * 255);
                pixels[index + 1] = static_cast<unsigned char>(color.g * 255);
                pixels[index + 2] = static_cast<unsigned char>(color.b * 255);

            }   
        }

        // actualize scene
        renderScene(window, quadVAO, texture, shaderProgram, pixels);

    }
    glfwTerminate();
}

// function declarations
GLFWwindow* StartGLU() {
    if (!glfwInit()) {
        std::cerr << "Failed to initialize GLFW" << std::endl;
        return nullptr;
    }

    GLFWwindow* window = glfwCreateWindow(WIDTH, HEIGHT, "RAY_TRACING", nullptr, nullptr);
    if (!window) {
        std::cerr << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return nullptr;
    }
    glfwMakeContextCurrent(window);

    glewExperimental = GL_TRUE;
    if (glewInit() != GLEW_OK) {
        std::cerr << "Failed to initialize GLEW." << std::endl;
        glfwTerminate();
        return nullptr;
    }

    glViewport(0, 0, WIDTH, HEIGHT);
    return window;
}
GLuint CreateShaderProgram(){

    const char* vertexShaderSource = R"(
    #version 330 core
    layout (location = 0) in vec3 aPos;
    layout (location = 1) in vec2 aTexCoord;
    out vec2 TexCoord;
    void main() {
        gl_Position = vec4(aPos, 1.0);
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

    GLuint vertexShader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertexShader, 1, &vertexShaderSource, nullptr);
    glCompileShader(vertexShader);

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
}
GLuint setupQuad() {
    float quadVertices[] = {
        // positions   // texCoords
        -1.0f,  1.0f,  0.0f, 1.0f,
        -1.0f, -1.0f,  0.0f, 0.0f,
         1.0f, -1.0f,  1.0f, 0.0f,
        -1.0f,  1.0f,  0.0f, 1.0f,
         1.0f, -1.0f,  1.0f, 0.0f,
         1.0f,  1.0f,  1.0f, 1.0f
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

    return VAO;
}
GLuint loadTexture(){
    GLuint texture;
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    return texture;
};
void renderScene(GLFWwindow* &window, GLuint &quadVAO, GLuint &texture, GLuint &shaderProgram, std::vector<unsigned char> &pixels){
        // Update texture with ray traced result
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, WIDTH, HEIGHT, 0, GL_RGB, 
                 GL_UNSIGNED_BYTE, pixels.data());
    glUseProgram(shaderProgram);
    glBindVertexArray(quadVAO);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glfwSwapBuffers(window);
    glfwPollEvents();
}

void UpdateCam(GLuint shaderProgram, glm::vec3 cameraPos) {
    // Calculate current frame time for smooth movement
    float currentFrame = glfwGetTime();
    deltaTime = currentFrame - lastFrame;
    lastFrame = currentFrame;
}
void keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods) {
    float cameraSpeed = 1.0f * deltaTime;
    
    if (key == GLFW_KEY_W && (action == GLFW_PRESS || action == GLFW_REPEAT))
        cameraPos += cameraSpeed * cameraFront;
    if (key == GLFW_KEY_S && (action == GLFW_PRESS || action == GLFW_REPEAT))
        cameraPos -= cameraSpeed * cameraFront;
    if (key == GLFW_KEY_A && (action == GLFW_PRESS || action == GLFW_REPEAT))
        cameraPos -= normalize(cross(cameraFront, cameraUp)) * cameraSpeed;
    if (key == GLFW_KEY_D && (action == GLFW_PRESS || action == GLFW_REPEAT))
        cameraPos += normalize(cross(cameraFront, cameraUp)) * cameraSpeed;
    if (key == GLFW_KEY_Q && action == GLFW_PRESS)
        glfwSetWindowShouldClose(window, true);
}
void mouse_callback(GLFWwindow* window, double xpos, double ypos) {
    static bool firstMouse = true;
    static float lastX = 400.0f, lastY = 300.0f;
    if (firstMouse) {
        lastX = xpos;
        lastY = ypos;
        firstMouse = false;
    }
    float xoffset = xpos - lastX;
    float yoffset = lastY - ypos; 
    lastX = xpos;
    lastY = ypos;

    float sensitivity = 0.2f;
    xoffset *= sensitivity;
    yoffset *= sensitivity;

    if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_MIDDLE) == GLFW_PRESS) {
        if (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS) {
            // PAN
            glm::vec3 right = glm::normalize(glm::cross(cameraFront, cameraUp));
            glm::vec3 up = glm::normalize(cameraUp);
            glm::vec3 panOffset = -right * xoffset * 0.05f + up * yoffset * 0.05f;
            target += panOffset;
            cameraPos += panOffset;
        } else {
            // ORBIT
            cameraYaw += xoffset;
            cameraPitch += yoffset;

            if (cameraPitch > 89.0f) cameraPitch = 89.0f;
            if (cameraPitch < -89.0f) cameraPitch = -89.0f;

            float radYaw = glm::radians(cameraYaw);
            float radPitch = glm::radians(cameraPitch);
            cameraPos.x = target.x + radius * cos(radPitch) * cos(radYaw);
            cameraPos.y = target.y + radius * sin(radPitch);
            cameraPos.z = target.z + radius * cos(radPitch) * sin(radYaw);
        }
        // Always look at the target
        cameraFront = glm::normalize(target - cameraPos);
    }
}
void scroll_callback(GLFWwindow* window, double xoffset, double yoffset) {
    float zoomSpeed = 1.0f;
    radius -= yoffset * zoomSpeed;
    if (radius < 1.0f) radius = 1.0f;
    if (radius > 50.0f) radius = 50.0f;

    float radYaw = glm::radians(cameraYaw);
    float radPitch = glm::radians(cameraPitch);
    cameraPos.x = target.x + radius * cos(radPitch) * cos(radYaw);
    cameraPos.y = target.y + radius * sin(radPitch);
    cameraPos.z = target.z + radius * cos(radPitch) * sin(radYaw);
    cameraFront = glm::normalize(target - cameraPos);
}

