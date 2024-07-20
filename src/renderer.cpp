#include "types.h"

class Renderer{
public:
    GLFWwindow* window;

    void init(){
        createWindow();
    }

    void run(){
        double totalFrameTime = 0.0f;
        int frameCount = 0;
        auto startTime = std::chrono::high_resolution_clock::now();

        while(!glfwWindowShouldClose(window)){
            auto frameStartTime = std::chrono::high_resolution_clock::now();

            glfwPollEvents();

            draw();

            auto frameEndTime = std::chrono::high_resolution_clock::now();
            std::chrono::duration<double, std::milli> frameDuration = frameEndTime - frameStartTime;

            totalFrameTime += frameDuration.count();
            frameCount++;
        }

        auto endTime = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> elapsedSeconds = endTime - startTime;
        double avgFrameTime = elapsedSeconds.count() / frameCount;
        double fps = 1.0 / avgFrameTime;

        fmt::println("Total elapsed time: {}s", elapsedSeconds.count());
        fmt::println("Total frames: {}", frameCount);
        fmt::println("Average frame time: {}ms", avgFrameTime*1000.0f);
        fmt::println("Average FPS: {}", fps);
    }

    void cleanup(){
        destroyWindow();
    }

private:
    void draw(){

    }

    void createWindow(){
        glfwInit();

        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

        window = glfwCreateWindow(WIDTH, HEIGHT, "Vulkan Renderer", nullptr, nullptr);
    }

    void destroyWindow(){
        glfwDestroyWindow(window);
        glfwTerminate();
    }
};