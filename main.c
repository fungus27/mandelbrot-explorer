#include <stdlib.h>
#include <stdio.h>
#include <GL/glew.h>
#include <GLFW/glfw3.h>

int main() {
    if (!glfwInit()) {
        const char *description;
        int code = glfwGetError(&description);
        fprintf(stderr, "glfw failed to initialize.\nerror (%d): %s\n", code, description);
    }
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
    GLFWwindow* window = glfwCreateWindow(640, 480, "dev", NULL, NULL);
    glfwMakeContextCurrent(window);

    GLenum err; 
    if ((err = glewInit()) != GLEW_OK)
        fprintf(stderr, "glew failed to initialize.\nerror (%u): %s\n", err, glewGetErrorString(err));

    while (!glfwWindowShouldClose(window)) {
        glfwSwapBuffers(window);
        glfwPollEvents();
    }
    
    glfwTerminate();
}
