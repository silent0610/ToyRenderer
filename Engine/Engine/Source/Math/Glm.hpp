#pragma once
#define GLM_FORCE_DEPTH_ZERO_TO_ONE // 适用于Vulkan的深度范围
#define GLM_FORCE_RADIANS
#include <glm/glm.hpp>                  // 核心功能
#include <glm/gtc/matrix_transform.hpp> // 变换矩阵，如 translate, rotate, scale, perspective
#include <glm/gtc/matrix_inverse.hpp>   // 矩阵求逆
#include <glm/gtc/type_ptr.hpp>         // 与OpenGL/Vulkan交互，获取指针
#include <glm/gtc/random.hpp>
#include <glm/gtc/packing.hpp>
#include <glm/gtc/quaternion.hpp>