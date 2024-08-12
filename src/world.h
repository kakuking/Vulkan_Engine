#pragma once

#include "types.h"
#include "structs.h"

struct Particle {
    glm::vec3 center;
    float radius;
    std::vector<glm::vec3> vertices;
    std::vector<uint32_t> indices;

    int level = 1;

    Particle() {}
    Particle(glm::vec3 newCenter, float newRadius){
        center = newCenter;
        radius = newRadius;
    }

    virtual void createVertices(){};
    virtual void updateLevel(int newLevel){};

    virtual void translate(glm::vec3 t) {
        center += t;
        for(auto& vert: vertices){
            vert += t;
        }
    }
};

struct Circle: public Particle {
    Circle(glm::vec3 newCenter, float newRadius){
        center = newCenter;
        radius = newRadius;
    }

    void createVertices(){
        float angle = glm::two_pi<float>() / (level + 2.f);

        float prevAngle = 0.0f;
        float curAngle = 0.0f;

        vertices.push_back(center); // center is 0th
        for (size_t i = 0; i < level + 2; i++)
        {   
            curAngle = angle * (i + 1.f);

            glm::vec3 first = glm::vec3(glm::cos(curAngle), glm::sin(curAngle), 0.f);
            glm::vec3 third = glm::vec3(glm::cos(prevAngle), glm::sin(prevAngle), 0.f);

            prevAngle = curAngle;

            vertices.push_back(first + center);
            vertices.push_back(third + center);

            indices.push_back(vertices.size() - 2);
            indices.push_back(0);
            indices.push_back(vertices.size() - 1);
        }
    }
};

class World{
    std::vector<Particle> particles;
    
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;

    int level = 1;    

public:
    std::vector<Vertex> getVertices(){
        return vertices;
    }

    std::vector<uint32_t> getIndices(){
        return indices;
    }

    int getParticleCount(){
        return particles.size();
    }

    void setLevel(int newLevel){
        level = newLevel;
    }

    void addCircle(glm::vec3 center, float radius, glm::vec4 color){
        addCircle(center, radius, color, level);
    }

    void addCircle(glm::vec3 center, float radius, glm::vec4 color, int level){
        Circle circle(center, radius);

        circle.level = level;

        circle.createVertices();

        for(auto& vertex: circle.vertices){
            Vertex v;
            v.position = vertex;
            v.color = color;

            vertices.push_back(v);
        }

        int currentOffset = indices.size();

        for (auto& index: circle.indices)
        {
            indices.push_back(index + currentOffset);
        }

        particles.push_back(circle);
    }

    void update(float timeDelta){
        if(particles.size() < 2)
            return;

        
    }
};