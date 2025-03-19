#pragma once
#define GLFW_INCLUDE_VULKAN
#include "Lib.h"
struct Vertex
{
    glm::vec3 pos;
    glm::vec3 color;
    glm::vec2 uv;
    glm::vec3 tangent;
};
struct Model
{
    std::string name;
};
struct Models
{
    Model skyBox;
    std::vector<Model> transObjects;
    std::vector<Model> OpaqueObjects;
};