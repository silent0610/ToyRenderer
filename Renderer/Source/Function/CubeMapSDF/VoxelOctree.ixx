module;
export module VoxelOctreeMod;
import std;
import <vector>;
import <memory>;
import <cstdint>;
import <array>;
import <optional>;
import GlmMod;
using vec3 = glm::vec3;

export enum class VoxelState : uint8_t
{
	EMPTY = 0,
	SOLID = 1,
	MIXED = 2
};

export struct VoxelOctreeNode
{
	vec3 center;
	float halfSize;
	VoxelState state = VoxelState::EMPTY;
	std::array<std::shared_ptr<VoxelOctreeNode>, 8> children;

	VoxelOctreeNode(const vec3& c, float h)
		: center(c), halfSize(h), state(VoxelState::EMPTY)
	{
		children.fill(nullptr);
	}

	bool IsLeaf() const
	{
		for (const auto& child : children)
		{
			if (child != nullptr)
				return false;
		}
		return true;
	}
};

export class VoxelOctree
{
public:
	// Constructor takes a reference to the voxel state array and grid size (assumed cubic)
	VoxelOctree(const std::vector<uint8_t>& voxelStates, int gridSize);

	// Get root node pointer
	std::shared_ptr<VoxelOctreeNode> GetRoot() const { return root; }

private:
	std::shared_ptr<VoxelOctreeNode> root;
	const std::vector<uint8_t>& voxelStates;
	int gridSize; // e.g. 64
	int maxDepth; // calculated from gridSize (log2)

	// Recursive function to calculate node state and build tree
	VoxelState CalculateNodeState(std::shared_ptr<VoxelOctreeNode> node, int depth, int x, int y, int z, int size);

	// Helper to get voxel state at given coordinates
	VoxelState GetVoxelState(int x, int y, int z) const;
};