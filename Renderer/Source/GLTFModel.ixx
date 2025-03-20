module;

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <Glm/glm.hpp>
#include <Glm/gtc/matrix_transform.hpp>

export module GLTFModelMod;
import std;
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
    Model();
};
struct Models
{
    Model skyBox;
    std::vector<Model> transObjects;
    std::vector<Model> OpaqueObjects;
};