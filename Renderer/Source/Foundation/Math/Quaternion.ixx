module;
// GLM includes for quaternion types
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_FORCE_RADIANS
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

export module Quaternion;
import Vector;
import Matrix;

// Export quaternion types and functions using GLM
export namespace Math
{
    // Quaternion type
    using Quaternion = glm::quat;

    // Quaternion functions
    using ::glm::angleAxis;
    using ::glm::slerp;
    using ::glm::mix;
}