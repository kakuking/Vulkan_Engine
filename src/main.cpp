#include "types.h"

#include "renderer.cpp"

int main(){
    Renderer app;

    app.init();

    app.run();

    app.cleanup();
}