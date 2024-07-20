#include "types.h"

class Renderer{
public:
    GLFWwindow* window;

    void init();

    void run();

    void cleanup();

private:
    void createWindow();
    void destroyWindow();
};