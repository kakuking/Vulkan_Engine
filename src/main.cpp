#include "types.h"

#include "renderer.h"
#include "external_test.h"

int main(){
    Renderer app;

    RectangleMesh newMesh;
    app._meshes.push_back(&newMesh);

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