module;
#include <stdint.h>

export module ConfigMod;
import GlmMod;
import std;

export struct Config
{
	Config(const std::string& configPath);
	bool enableValidation;
	std::string modelPath;
	std::vector<std::string> shadersPath;
	struct CameraConfig
	{
		glm::vec3 pos;
		glm::vec3 Rotation{};
		std::string type;
		std::string flipY;
		float fov;
		float znear;
		float zfar;
		float movementSpeed;
	} camera;

	struct LightConfig
	{
		glm::vec3 pos;
		glm::vec3 color;
	} light;
	struct HBAOBase
	{
		float Radius;
		float StepNum;
		float DirNum;
	} HBAO;

	enum class SdfKind : uint32_t
    {
        BruteForce = 0,
        Analytical = 1,
        MultiView = 2,
        JFA = 3,
		Ngp = 4,
		Heat = 5,
		Bvh = 6
    };
	struct SdfConfig
	{
		float WorldSize{2.0f};
		uint32_t VoxelResolution{32};
		uint32_t OctreeResolution{32};
		uint32_t MinSelectionResolution{4};
		uint32_t MaxSelectionResolution{16};
		uint32_t SdfMode{1}; // Analytical:0, Multiview:1
		uint32_t SampledLevel{1};
		uint32_t MeshToSdfMode{1}; // floodfill:0, jump:1
		uint32_t AnalyticalUsedPointNum{512};
		uint32_t MultiViewUsedCameraNum{10};
        uint32_t MaxCameraNum{30};
		uint32_t MultiViewDepthResolution{128};
		uint32_t MeshToSdfIteration{10};
		uint32_t MeshToSdfDistanceMode{1}; // unsigned:0, signed:1
		uint32_t MeshToSdfQuality{0};	   // normal:0, ultra:1
        SdfKind SdfAoUseSdfKind{SdfKind::MultiView};       //
        uint32_t UseRandomSelection{1};
        uint32_t EnableCountSort{1};
        uint32_t EnableCameraOverlay{1};
        uint32_t EnableMultiviewIsoSurface{0};
        uint32_t EnableSelectionScoreOctreePass{0};
        // 1: 粗→细按父节点配额挑选；0: 旧复杂度门控逻辑
        uint32_t EnableHierarchicalParentQuota{0};
        // 同一父节点下最多选几个子相机，范围 [1, 8]
        uint32_t MaxChildrenPerParent{1};
        // 1: 层级挑选确定性排序+时序稳定；0: 旧并行原子（不稳定但更快）
        uint32_t EnableStableCameraSelection{1};
        uint32_t SdfResolution{64};
	};
	SdfConfig Sdf;

	struct DynamicGeometryConfig
	{
		bool enable{false};
		uint32_t animationIndex{0};
		float speed{1.0f};
		bool loop{true};
	};
	DynamicGeometryConfig Dynamic;
};
