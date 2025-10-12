module;
#include <stdint.h>

export module ConfigMod;
import GlmMod;
import std;

export struct Config
{
	Config(std::string configPath);
	bool enableValidation;
	std::string modelPath;
	std::vector<std::string> shadersPath;
	struct CameraConfig
	{
		glm::vec3 pos;
		std::string type;
		std::string flipY;
		float fov;
		float znear;
		float zfar;
		float movementSpeed;
	}camera;

	struct LightConfig
	{
		glm::vec3 pos;
		glm::vec3 color;
	}light;
	struct HBAOBase
	{
		float Radius;
		float StepNum;
		float DirNum;
	}HBAO;

	struct SdfConfig
    {
        float WorldSize{2.0f};
        uint32_t Resolution{64};
        uint32_t SdfMode{1}; //Analytical:0, Multiview:1
        uint32_t AnalyticalSampledLevel{1};
        uint32_t MeshToSdfMode{1}; // floodfill:0, jump:1
        uint32_t AnalyticalUsedPointNum{512};
        uint32_t MultiViewUsedCameraNum{10};
        uint32_t MultiViewDepthResolution{128};
        uint32_t MeshToSdfIteration{10};
        uint32_t MeshToSdfDistanceMode{1}; // unsigned:0, signed:1
        uint32_t MeshToSdfQuality{0};      // normal:0, ultra:1
        uint32_t UseBruteForce{1};         // 
    };
    SdfConfig Sdf;
};