#include <GLFW/glfw3.h>
#include <iostream>

GLFWwindow* StartGlu();

int main(){
    GLFWwindow* window = StartGlu();
    while (!glfwWindowShouldClose(window))
    {

    }
}


GLFWwindow* StartGlu (){
    if (!glfwInit()){
        std::cout << "Failed to initialize GLFW." << std::endl;
        return nullptr;
    }

    GLFWwindow* window = glfwCreateWindow(800, 600, "3d_test", NULL, NULL);

}