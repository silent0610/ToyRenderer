module;
// 全局模块片段：在这里包含所有需要的 glm 头文件和宏定义
#define GLM_FORCE_DEPTH_ZERO_TO_ONE     // 适用于Vulkan的深度范围
#define GLM_FORCE_RADIANS
#include <glm/glm.hpp>                  // 核心功能
#include <glm/gtc/matrix_transform.hpp> // 变换矩阵，如 translate, rotate, scale, perspective
#include <glm/gtc/matrix_inverse.hpp>   // 矩阵求逆
#include <glm/gtc/type_ptr.hpp>         // 与OpenGL/Vulkan交互，获取指针
#include <glm/gtc/random.hpp>
#include <glm/gtc/packing.hpp>
#include <glm/gtc/quaternion.hpp>

export module GlmMod;

// 导出的命名空间
export namespace glm
{
	// --- 核心类型 ---
	// 向量 (Vectors)
	using ::glm::ivec2; // 整型向量
	using ::glm::ivec3;
	using ::glm::ivec4;
	using ::glm::uvec2; // 无符号整型向量
	using ::glm::uvec3;
	using ::glm::uvec4;
	using ::glm::vec2;
	using ::glm::vec3;
	using ::glm::vec4;

	// 矩阵 (Matrices)
	using ::glm::mat2;
	using ::glm::mat3;
	using ::glm::mat4;
	using ::glm::mat4x4;
	using ::glm::mat3x3;

	// 四元数 (Quaternions)
	using ::glm::quat;

	// --- 常用函数 ---
	// 核心数学函数
	using ::glm::clamp;
	using ::glm::cross;
	using ::glm::degrees;
	using ::glm::distance;
	using ::glm::dot;
	using ::glm::inverse;
	using ::glm::length;
	using ::glm::mix;
	using ::glm::normalize;
	using ::glm::radians;

	// 矩阵变换函数 (Matrix Transformations)
	using ::glm::rotate;
	using ::glm::scale;
	using ::glm::translate;

	// 投影矩阵函数 (Projection Matrices)
	using ::glm::ortho;
	using ::glm::perspective;

	// 视图矩阵函数 (View Matrix)
	using ::glm::lookAt;

	// 矩阵求逆/转置
	using ::glm::inverseTranspose;
	using ::glm::transpose;

	// 四元数相关函数
	using ::glm::angleAxis;
	using ::glm::slerp;

	// 与指针交互 (Pointer Interaction)
	using ::glm::value_ptr;

	using ::glm::operator+;
	using ::glm::operator-;
	using ::glm::operator*;
	using ::glm::operator/;
	using ::glm::operator!=;
	using ::glm::min;
	using ::glm::max;
	using ::glm::sign;
	using ::glm::tan;
	using ::glm::linearRand;
	using ::glm::packUnorm2x8;
	using ::glm::normalize;
	using ::glm::packUnorm4x8;


}