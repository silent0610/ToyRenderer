module;
// GLM includes for transformation functions
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_FORCE_RADIANS
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/random.hpp>

export module Transform;
import Vector;
import Matrix;
import Quaternion;

// Export transformation utilities using GLM
export namespace Math
{
    // Common transformation functions
    using ::glm::clamp;
    using ::glm::degrees;
    using ::glm::radians;
    using ::glm::min;
    using ::glm::max;
    using ::glm::sign;
    using ::glm::tan;
    using ::glm::linearRand;
    
    // Operators
    using ::glm::operator+;
    using ::glm::operator-;
    using ::glm::operator*;
    using ::glm::operator/;
    using ::glm::operator!=;
}