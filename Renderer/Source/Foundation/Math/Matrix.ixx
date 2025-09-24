module;
// GLM includes for matrix types
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_FORCE_RADIANS
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/matrix_inverse.hpp>
#include <glm/gtc/type_ptr.hpp>

export module Matrix;
import Vector;

// Export matrix types and functions using GLM
export namespace Math
{
    // Matrix types
    using Matrix2 = glm::mat2;
    using Matrix3 = glm::mat3;
    using Matrix4 = glm::mat4;

    // Matrix transformation functions
    using ::glm::rotate;
    using ::glm::scale;
    using ::glm::translate;

    // Projection matrix functions
    using ::glm::ortho;
    using ::glm::perspective;

    // View matrix functions
    using ::glm::lookAt;

    // Matrix inverse/transpose
    using ::glm::inverse;
    using ::glm::inverseTranspose;
    using ::glm::transpose;

    // Pointer interaction
    using ::glm::value_ptr;

    // Convenience functions
    inline Matrix4 CreatePerspective(float fov, float aspect, float near, float far) {
        return glm::perspective(fov, aspect, near, far);
    }
    
    inline Matrix4 CreateLookAt(const Vector3& eye, const Vector3& target, const Vector3& up) {
        return glm::lookAt(eye, target, up);
    }

    inline Matrix4 CreateOrtho(float left, float right, float bottom, float top, float near, float far) {
        return glm::ortho(left, right, bottom, top, near, far);
    }
    
    // Matrix operations
    inline Matrix4 Multiply(const Matrix4& a, const Matrix4& b) {
        return glm::mat4(a) * glm::mat4(b);
    }
    
    inline Matrix4 MultiplyMVP(const Matrix4& proj, const Matrix4& view, const Matrix4& model) {
        return glm::mat4(proj) * glm::mat4(view) * glm::mat4(model);
    }
}