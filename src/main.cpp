#include "types.h"

#include "renderer.h"

int main(){
    Renderer app;

    app.init();

    try {
        app.run();
    } catch (const std::exception &e) {
        std::cerr << e.what() << std::endl;
        return EXIT_FAILURE;
    }

    app.cleanup();

    return EXIT_SUCCESS;

}