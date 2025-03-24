module;

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <Glm/glm.hpp>
#include <Glm/gtc/matrix_transform.hpp>


#include "tiny_gltf.h"
#include "vulkan/vulkan.h"
export module GLTFModelMod;
import std;
import TextureMod;
import DeviceMod;

export class GLTFModel
{
public:
	VulkanDevice* vulkanDevice{nullptr};
	VkQueue copyQueue{nullptr};

	// The vertex layout for the samples' model
	struct Vertex
	{
		glm::vec3 pos;
		glm::vec3 normal;
		glm::vec2 uv;
		glm::vec3 color;
	};

	// Single vertex buffer for all primitives
	struct VerticeBuffer
	{
		VkBuffer buffer{nullptr};
		VkDeviceMemory memory{nullptr};
	};

	VerticeBuffer vertices{};
	// Single index buffer for all primitives
	struct IndiceBuffer
	{
		int count;
		VkBuffer buffer{ nullptr };
		VkDeviceMemory memory{ nullptr };
	};
	IndiceBuffer indices{ };

	struct Primitive
	{
		uint32_t firstIndex;
		uint32_t indexCount;
		int32_t materialIndex;
	};

	// Contains the node's (optional) geometry and can be made up of an arbitrary number of primitives
	struct Mesh
	{
		std::vector<Primitive> primitives;
	};

	// A node represents an object in the glTF scene graph
	struct Node
	{
		Node* parent;
		std::vector<Node*> children;
		Mesh mesh;
		glm::mat4 matrix;
		~Node();
	};

	// A glTF material stores information in e.g. the texture that is attached to it and colors
	struct Material
	{
		glm::vec4 baseColorFactor = glm::vec4(1.0f);
		uint32_t baseColorTextureIndex;
	};



	// Contains the texture for a single glTF image
	// Images may be reused by texture objects and are as such separated
	struct Image
	{
		Texture2D texture;
		// We also store (and create) a descriptor set that's used to access this texture from the fragment shader
		VkDescriptorSet descriptorSet;
	};

	// A glTF texture stores a reference to the image and a sampler
	// In this sample, we are only interested in the image
	struct Texture
	{
		int32_t imageIndex;
	};

	/*
	Model data
	*/
	std::vector<Image> images;
	std::vector<Texture> textures;
	std::vector<Material> materials;
	std::vector<Node*> nodes;

	~GLTFModel();


	void LoadImages(tinygltf::Model& input);
	void LoadTextures(tinygltf::Model& input);
	void LoadMaterials(tinygltf::Model& input);
	void LoadNode(const tinygltf::Node& inputNode, const tinygltf::Model& input, GLTFModel::Node* parent, std::vector<uint32_t>& indexBuffer, std::vector<GLTFModel::Vertex>& vertexBuffer);

	// Draw a single node including child nodes (if present)
	void DrawNode(VkCommandBuffer commandBuffer, VkPipelineLayout pipelineLayout, GLTFModel::Node* node);

	void Draw(VkCommandBuffer commandBuffer, VkPipelineLayout pipelineLayout);

};




