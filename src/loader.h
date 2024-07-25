#include "types.h"
#include "structs.h"

#include <unordered_map>
#include <filesystem>

#include "stb_image.h"
#include <iostream>
#include "renderer.h"

struct GeoSurface{
    uint32_t startIndex;
    uint32_t count;
};

struct MeshAsset{
    std::string name;

    std::vector<GeoSurface> surfaces;
    MeshBuffers meshBuffers;
};

class loader{
    void setupDefaultRectangleData(){
        std::array<Vertex,4> verticesOpaque;
        std::array<Vertex,4> verticesAlpha;

        // Alpha rectagle, in front
        float opacity = 0.9f;
        verticesAlpha[0].position = {0.5,-0.5, 0};
        verticesAlpha[1].position = {0.5,0.5, 0};
        verticesAlpha[2].position = {-0.5,-0.5, 0};
        verticesAlpha[3].position = {-0.5,0.5, 0};
        verticesAlpha[0].color = {0,0, 0,opacity};
        verticesAlpha[1].color = {0.5,0.5,0.5,opacity};
        verticesAlpha[2].color = {1,0,0,opacity};
        verticesAlpha[3].color = {0,1,0,opacity};
        verticesAlpha[0].uv_x = 0;
        verticesAlpha[1].uv_x = 0;
        verticesAlpha[2].uv_x = 1;
        verticesAlpha[3].uv_x = 1;

        verticesOpaque[0].position = {0.7,-0.5,-0.5};
        verticesOpaque[1].position = {0.7,0.5,-0.5};
        verticesOpaque[2].position = {-0.3,-0.5,-0.5};
        verticesOpaque[3].position = {-0.3,0.5,-0.5};
        verticesOpaque[0].color = {1,0,1,1};
        verticesOpaque[1].color = {1,0,1,1};
        verticesOpaque[2].color = {1,0,1,1};
        verticesOpaque[3].color = {1,0,1,1};
        verticesOpaque[0].uv_x = 1;
        verticesOpaque[1].uv_x = 1;
        verticesOpaque[2].uv_x = 1;
        verticesOpaque[3].uv_x = 1;

        /*
        for(Vertex v: rect_vertices){
            glm::vec4 tempPos = glm::vec4(v.position, 1.0f);
            glm::vec4 viewPos = view * tempPos;
            glm::vec4 projPos = proj * viewPos;
            std::cout << "Vertex Position: " << glm::to_string(v.position) << std::endl;
            std::cout << "Position after View Matrix: " << glm::to_string(viewPos) << std::endl;
            std::cout << "Position after Projection Matrix: " << glm::to_string(projPos) << std::endl;
        }
        */

        std::array<uint32_t,6> indicesOpaque;
        std::array<uint32_t,6> indicesAlpha;

        // Alpha Rectangle
        indicesAlpha[0] = 0;
        indicesAlpha[1] = 1;
        indicesAlpha[2] = 2;

        indicesAlpha[3] = 2;
        indicesAlpha[4] = 1;
        indicesAlpha[5] = 3;

        // Opaque Rectangle
        indicesOpaque[0] = 0;
        indicesOpaque[1] = 1;
        indicesOpaque[2] = 2;

        indicesOpaque[3] = 2;
        indicesOpaque[4] = 1;
        indicesOpaque[5] = 3;

        _meshOpaque = uploadMesh(indicesOpaque, verticesOpaque);
        _meshAlpha = uploadMesh(indicesAlpha, verticesAlpha);

        //delete the rectangle data on engine shutdown
        _mainDeletionQueue.pushFunction([&](){
            destroyBuffer(_meshOpaque.indexBuffer);
            destroyBuffer(_meshOpaque.vertexBuffer);
            destroyBuffer(_meshAlpha.indexBuffer);
            destroyBuffer(_meshAlpha.vertexBuffer);
        });
    }
};