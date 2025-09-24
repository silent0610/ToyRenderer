module;
// GLM includes for vector types
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_FORCE_RADIANS
#include <glm/glm.hpp>

export module Vector;

// Export vector types using GLM
export namespace Math
{
    // Vector types
    using Vector2 = glm::vec2;
    using Vector3 = glm::vec3;
    using Vector4 = glm::vec4;
    using IVector2 = glm::ivec2;
    using IVector3 = glm::ivec3;
    using IVector4 = glm::ivec4;
    using UVector2 = glm::uvec2;
    using UVector3 = glm::uvec3;
    using UVector4 = glm::uvec4;

    // Vector functions
    using ::glm::cross;
    using ::glm::distance;
    using ::glm::dot;
    using ::glm::length;
    using ::glm::normalize;
    
    // Vector operations with function names
    template<typename T>
    inline T Add(const T& a, const T& b) { return a + b; }
    
    template<typename T>
    inline T Sub(const T& a, const T& b) { return a - b; }
    
    template<typename T>
    inline T Mul(const T& a, float scalar) { return a * scalar; }
    
    template<typename T>
    inline T Div(const T& a, float scalar) { return a / scalar; }
}