module;
#include "vulkan/vulkan.h"
export module MeshToSdf;
import std;
import GlmMod;
import DeviceMod;
import BufferMod;
import TextureMod;
import VkglTFModel;
import FrameBufferMod;
import InitMod;
import ToolMod;

export class MeshToSdf
{
public:
	enum class FloodMode
	{
		Linear = 0,
		Jump = 1
	};
	enum class DistanceMode
	{
		Signed = 0,
		Unsigned = 1
	};

	struct SdfParam
	{
		int voxelResolution{64};
		float size{5.0f};
		FloodMode floodMode{FloodMode::Jump};
		DistanceMode distanceMode{DistanceMode::Signed};
		int floodIterations{8};
		float offset{0.0f};
	};
	MeshToSdf();
	void Initialize(OldVulkanDevice *device, VkQueue queue, VkDescriptorPool descriptorPool);
	void GenerateSdf(VkCommandBuffer cmd, vkglTF::Model* mesh, const glm::mat4& worldToLocal = glm::mat4(1.0f));

	void SetSdfParams(const SdfParam& params) { sdfParam_ = params; }
	const SdfParam& GetSdfParams() const { return sdfParam_; }

	VkImageView GetSdfTextureView() const;
	void Cleanup();

private:
	void CreateSdfResource();

	void CreateComputePipeline();

	void CreateDescriptorSet(VkDescriptorPool descriptorPool);

	void CreateSingleComputePipeline(const std::string& shaderName, VkPipeline& pipeline);

	struct MeshToSDFConstant
	{
		alignas(16)glm::mat4 worldToLocal;
		alignas(16)glm::ivec4 voxelResolution;  // w = total voxel count
		float maxDistance;
		float initialDistance;
		float offset;
		float padding1;
		alignas(16)glm::vec4 origin;
		float cellSize;
		int numCellsX, numCellsY, numCellsZ;
		int indexFormat16bit;
		int vertexBufferStride;
		int vertexBufferPosOffset;
		int jumpOffset;
		alignas(16)glm::ivec4 jumpOffsetInterleaved;
		int dispatchSizeX;
	};

	struct ComputePipelines
	{
		VkPipeline initialize{};
		VkPipeline splateSigned{};
		VkPipeline splatUnsigned{};
		VkPipeline finalize{};
		VkPipeline linearFlood{};
		VkPipeline linearFloodUltra{};
		VkPipeline jumpFloodInit{};
		VkPipeline jumpFloodStep{};
		VkPipeline jumpFloodStepUltra{};
		VkPipeline jumpFloodFinalize{};
		VkPipeline bufferToTexture{};
	} pipelines_{};

	struct PipelineLayouts
	{
		VkPipelineLayout general{};
	} pipelinelayouts_{};

	struct DescriptorSets
	{
		VkDescriptorSetLayout layout{};
		VkDescriptorSet set{};
	} descriptor_{};

	SdfParam sdfParam_{};
	OldVulkanDevice *device_{};
	VkQueue queue_{};
	vkglTF::Model* currentMesh_{};
	Texture3D *sdfTexture_{};
	Buffer sdfBuffer_{};
	Buffer sdfBufferBis_{};
	Buffer jumpBuffer_{};
	Buffer jumpBufferBis_{};
};
