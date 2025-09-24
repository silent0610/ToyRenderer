module;

export module MeshOctreeMod;
import GlmMod;
import std;
import <mutex>;
import VkglTFModel;
using vec3 = glm::vec3;

class Ray
{
public:
	Ray(vec3& origin, vec3& dir) :mOrigin{ origin }, mDir(dir) {};
	vec3 mOrigin;
	vec3 mDir;

};
class Bounds
{
public:
	glm::vec3 mCentre;
	glm::vec3 mSize;

	Bounds(const glm::vec3& centre, const glm::vec3& size);
	void Encapsulate(const vec3& point);
	void SetMinMax(const vec3& min, const vec3& max);
	inline vec3 GetMin() { return mCentre - 0.5f * mSize; }
	inline vec3 GetMax() { return mCentre + 0.5f * mSize; }
};

class MeshOctreeNode
{
public:
	MeshOctreeNode(std::shared_ptr<Bounds>b, float minNodeSize, int depth, int maxNodeDepth);

	/// @brief 递归创建子节点
	void Init();
	void AddTriangle(int triIndex, const vec3& v0, const vec3& v1, const vec3& v2);
	void Draw()const;

	bool IsLeafNode() { return !mTriangleIndices.empty(); };
	bool mIsInside{ false };
	bool m_visited{ false };
	std::shared_ptr<Bounds> mNodeBounds;
	std::array<std::shared_ptr<MeshOctreeNode>, 8> mChildren;
	std::vector<int> mTriangleIndices;
	std::mutex m_mutex;
private:
	//void AddTriangle(int triIndex, vec3 v0, vec3 v1, vec3 v2);

	inline static std::array<vec3, 3> boxAxes{ vec3(1,0,0),vec3(0,1,0),vec3(0,0,1) };

	bool SATTriangleTest(std::shared_ptr<Bounds> bounds, const vec3& v0, const vec3& v1, const vec3& v2);

	std::pair<float, float> ProjectTriangle(vec3 axis, const vec3& v0, const vec3& v1, const vec3& v2);


	float m_minSize{ 0.1f };
	int m_currentDepth{ 0 };
	int m_maxDepth{ 4 };
	std::array<std::shared_ptr<Bounds>, 8> m_childBounds;

};

export class MeshOctree
{
public:
	MeshOctree(const vkglTF::Model& mesh, float minNodeSize, int maxDepth);

private:
	void Voxelization();
	void BuildFromMesh();
	void CheckInsides();
	bool CheckInside(const vec3& point);
	std::vector<std::shared_ptr<MeshOctreeNode>>QueryLeafNodesAlongDirection(vec3& rayOrigin, const vec3& rayDir);
	void QueryLeafNodesRecursive(std::shared_ptr<MeshOctreeNode> node, const Ray& ray, std::vector<std::shared_ptr<MeshOctreeNode>>& result);
	bool IntersectRayBounds(const Ray& ray, std::shared_ptr<Bounds> bounds, float& tMin, float& tMax);
	bool RayIntersectsTriangle(vec3& origin, const vec3& dir, const vec3& v0, const vec3& v1, const vec3& v2);

	std::shared_ptr<MeshOctreeNode> m_rootNode;
	const vkglTF::Model& m_mesh;
};

