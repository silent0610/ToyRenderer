module;
#include <glm/glm.hpp>
module MeshOctreeMod;
import std;

Bounds::Bounds(const glm::vec3 &centre, const glm::vec3 &size) : mCentre{centre}, mSize{size}
{
	;
}
void Bounds::Encapsulate(const vec3 &point)
{
	vec3 currentMin = mCentre - (mSize * 0.5f);
	vec3 currentMax = mCentre + (mSize * 0.5f);

	currentMin = glm::min(currentMin, point);
	currentMax = glm::max(currentMax, point);

	mCentre = (currentMin + currentMax) * 0.5f;
	mSize = currentMax - currentMin;
}

void Bounds::SetMinMax(const vec3 &min, const vec3 &max)
{
	mSize = max - min;
	mCentre = min + mSize * 0.5f;
}
MeshOctreeNode::MeshOctreeNode(std::shared_ptr<Bounds> b, float minNodeSize, int depth, int maxNodeDepth) : mNodeBounds{b}, m_minSize{minNodeSize}, m_currentDepth(depth), m_maxDepth(maxNodeDepth)
{
	// 预计算子包围盒大小
	float quarter = mNodeBounds->mSize.x / 4.0f;
	float childLength = mNodeBounds->mSize.x / 2.0f;
	vec3 childSize{childLength, childLength, childLength};
	m_childBounds[0] = std::make_shared<Bounds>(mNodeBounds->mCentre + vec3(-quarter, quarter, -quarter), childSize);
	m_childBounds[1] = std::make_shared<Bounds>(mNodeBounds->mCentre + vec3(quarter, quarter, -quarter), childSize);
	m_childBounds[2] = std::make_shared<Bounds>(mNodeBounds->mCentre + vec3(-quarter, quarter, quarter), childSize);
	m_childBounds[3] = std::make_shared<Bounds>(mNodeBounds->mCentre + vec3(quarter, quarter, quarter), childSize);
	m_childBounds[4] = std::make_shared<Bounds>(mNodeBounds->mCentre + vec3(-quarter, -quarter, -quarter), childSize);
	m_childBounds[5] = std::make_shared<Bounds>(mNodeBounds->mCentre + vec3(quarter, -quarter, -quarter), childSize);
	m_childBounds[6] = std::make_shared<Bounds>(mNodeBounds->mCentre + vec3(-quarter, -quarter, quarter), childSize);
	m_childBounds[7] = std::make_shared<Bounds>(mNodeBounds->mCentre + vec3(quarter, -quarter, quarter), childSize);
}

void MeshOctreeNode::Init()
{
	if (mNodeBounds->mSize.x <= m_minSize || m_currentDepth >= m_maxDepth)
	{
		return;
	}
	if (mChildren[0] == nullptr)
	{
		for (int i = 0; i < 8; ++i)
		{
			if (mChildren[i] == nullptr)
			{
				mChildren[i] = std::make_shared<MeshOctreeNode>(m_childBounds[i], m_minSize, m_currentDepth + 1, m_maxDepth);
				mChildren[i]->Init();
			}
		}
	}
}
void MeshOctreeNode::AddTriangle(int triIndex, const vec3 &v0, const vec3 &v1, const vec3 &v2)
{
	if (mNodeBounds->mSize.x <= m_minSize || m_currentDepth >= m_maxDepth)
	{
		std::lock_guard<std::mutex> lock(m_mutex);
		mTriangleIndices.push_back(triIndex);
		return;
	}
	for (int i = 0; i < 8; i++) // 必须和所有进行判断
	{
		// Check if the triangle intersects the child's bounds.
		if (SATTriangleTest(m_childBounds[i], v0, v1, v2))
		{
			mChildren[i]->AddTriangle(triIndex, v0, v1, v2);
		}
	}
}

bool MeshOctreeNode::SATTriangleTest(std::shared_ptr<Bounds> bounds, const vec3 &v0, const vec3 &v1, const vec3 &v2)
{
	// 1. 添加快速AABB测试
	vec3 triMin = glm::min(glm::min(v0, v1), v2);
	vec3 triMax = glm::max(glm::max(v0, v1), v2);
	vec3 boxMin = bounds->GetMin();
	vec3 boxMax = bounds->GetMax();

	if (triMax.x < boxMin.x || triMin.x > boxMax.x ||
		triMax.y < boxMin.y || triMin.y > boxMax.y ||
		triMax.z < boxMin.z || triMin.z > boxMax.z)
	{
		return false;
	}
	// 1. AABB properties
	vec3 boxCenter = bounds->mCentre;
	vec3 boxHalfSize = bounds->mSize / 2.0f; // Assuming mSize is the full size

	// 2. Translate triangle vertices to AABB's local space (AABB center at origin)
	vec3 tv0 = v0 - boxCenter;
	vec3 tv1 = v1 - boxCenter;
	vec3 tv2 = v2 - boxCenter;

	// 3. Triangle edge vectors in local space
	vec3 e0 = tv1 - tv0;
	vec3 e1 = tv2 - tv1;
	vec3 e2 = tv0 - tv2;

	// 4. Define axes to test
	// We need to test 3 AABB face normals, 1 triangle normal, and 9 cross products (3 tri edges x 3 AABB axes)
	std::array<vec3, 13> axes_to_test;

	// a) AABB face normals (these are the world axes X, Y, Z for an axis-aligned box)
	// These are already stored in MeshOctreeNode::boxAxes
	axes_to_test[0] = MeshOctreeNode::boxAxes[0]; // Typically glm::vec3(1.0f, 0.0f, 0.0f)
	axes_to_test[1] = MeshOctreeNode::boxAxes[1]; // Typically glm::vec3(0.0f, 1.0f, 0.0f)
	axes_to_test[2] = MeshOctreeNode::boxAxes[2]; // Typically glm::vec3(0.0f, 0.0f, 1.0f)

	// b) Triangle normal
	// e0 and e1 are edges of the triangle in local space
	vec3 tri_normal = glm::cross(e0, e1);
	axes_to_test[3] = tri_normal;

	// c) Cross products of triangle edges and AABB face normals
	std::array<vec3, 3> tri_edges = {e0, e1, e2};
	int current_axis_idx = 4;
	for (const auto &edge : tri_edges)
	{
		for (const auto &box_axis : MeshOctreeNode::boxAxes)
		{
			// Ensure we don't write out of bounds, though it should be exactly 9 axes
			if (current_axis_idx < 13)
			{
				axes_to_test[current_axis_idx++] = glm::cross(edge, box_axis);
			}
		}
	}

	// 5. Perform SAT projection test for each axis
	for (const vec3 &axis : axes_to_test)
	{
		// Skip zero axes. This can happen if triangle is degenerate (tri_normal is zero)
		// or if a triangle edge is parallel to an AABB axis (cross product is zero).
		// Check squared length against a small epsilon to avoid sqrt.
		if (glm::dot(axis, axis) < 0.00001f)
		{ // Epsilon for zero vector check (squared)
			continue;
		}

		// Project the triangle onto the current axis.
		// The ProjectTriangle method is part of MeshOctreeNode.
		// tv0, tv1, tv2 are already in the AABB's local coordinate space.
		std::pair<float, float> tri_projection = ProjectTriangle(axis, tv0, tv1, tv2);
		float min_tri = tri_projection.first;
		float max_tri = tri_projection.second;

		// Project the AABB onto the current axis.
		// For an AABB centered at the origin, its projection is [-r, r] where
		// r = hx*|axis.x| + hy*|axis.y| + hz*|axis.z|.
		// This is because MeshOctreeNode::boxAxes[0] is (1,0,0), etc.
		// So glm::dot(axis, MeshOctreeNode::boxAxes[0]) is axis.x.
		float r = boxHalfSize.x * std::abs(axis.x) +
				  boxHalfSize.y * std::abs(axis.y) +
				  boxHalfSize.z * std::abs(axis.z);

		// Check for separation: if the intervals [min_tri, max_tri] and [-r, r] don't overlap.
		if (max_tri < -r || min_tri > r)
		{
			return false; // Found a separating axis, so no collision.
		}
	}

	return true; // No separating axis found after checking all 13 axes, so collision.
}

std::pair<float, float> MeshOctreeNode::ProjectTriangle(vec3 axis, const vec3 &v0, const vec3 &v1, const vec3 &v2)
{
	float p0 = glm::dot(axis, v0);
	float p1 = glm::dot(axis, v1);
	float p2 = glm::dot(axis, v2);

	float min = std::min(p0, std::min(p1, p2));
	float max = std::max(p0, std::max(p1, p2));

	return {min, max};
}
void MeshOctreeNode::Draw() const
{
	std::cout << "not support\n";
}

MeshOctree::MeshOctree(const vkglTF::Model &mesh, float minNodeSize, int maxDepth) : m_mesh(mesh)
{
	auto &vertices{mesh.vertexBuffer};
	auto bounds{std::make_shared<Bounds>(vertices[0].pos, vec3(0.0f))};
	for (auto &vert : vertices)
	{
		bounds->Encapsulate(vert.pos);
	}
	float maxSize{std::max(std::max(bounds->mSize.x, bounds->mSize.y), bounds->mSize.z)};

	vec3 sizeVector{maxSize};
	sizeVector *= 0.5f;
	bounds->SetMinMax(bounds->mCentre - sizeVector, bounds->mCentre + sizeVector);

	m_rootNode = std::make_shared<MeshOctreeNode>(bounds, minNodeSize, 0, maxDepth);
	Voxelization();
	BuildFromMesh();
	CheckInsides();
}

void ::MeshOctree::Voxelization()
{
	m_rootNode->Init();
}
void ::MeshOctree::BuildFromMesh()
{
	auto &indices{m_mesh.indexBuffer};
	auto &vertices{m_mesh.vertexBuffer};

	unsigned int numThreads = std::thread::hardware_concurrency();
	std::vector<std::thread> threads(numThreads);
	unsigned int numTriangles = indices.size() / 3;
	unsigned int trianglesPerThread = numTriangles / numThreads;

	for (unsigned int i = 0; i < numThreads; ++i)
	{
		unsigned int start = i * trianglesPerThread;
		unsigned int end = (i == numThreads - 1) ? numTriangles : start + trianglesPerThread;

		threads[i] = std::thread([this, start, end, &indices, &vertices]()
								 {
				for (unsigned int j = start; j < end; ++j)
				{
					const vec3& v0 = vertices[indices[j * 3]].pos;
					const vec3& v1 = vertices[indices[j * 3 + 1]].pos;
					const vec3& v2 = vertices[indices[j * 3 + 2]].pos;
					m_rootNode->AddTriangle(j, v0, v1, v2);
				} });
	}

	for (auto &thread : threads)
	{
		thread.join();
	}
}
void MeshOctree::CheckInsides()
{
	if (m_rootNode == nullptr)
		return;

	std::vector<std::shared_ptr<MeshOctreeNode>> leafNodes;
	std::queue<std::shared_ptr<MeshOctreeNode>> queue;
	queue.push(m_rootNode);

	// 1. Get all leaf nodes
	while (!queue.empty())
	{
		auto node = queue.front();
		queue.pop();

		if (node->IsLeafNode())
		{
			leafNodes.push_back(node);
		}
		else
		{
			for (const auto &child : node->mChildren)
			{
				if (child)
				{
					queue.push(child);
				}
			}
		}
	}

	std::vector<std::shared_ptr<MeshOctreeNode>> surfaceNodes;
	std::vector<std::shared_ptr<MeshOctreeNode>> emptyNodes;

	for (const auto &node : leafNodes)
	{
		if (!node->mTriangleIndices.empty())
		{
			surfaceNodes.push_back(node);
		}
		else
		{
			emptyNodes.push_back(node);
		}
	}

	// 2. Spatial hashing for empty nodes
	float nodeSize = emptyNodes.empty() ? 0.0f : emptyNodes[0]->mNodeBounds->mSize.x;
	auto hash_func = [nodeSize](const glm::vec3 &p)
	{
		int x = static_cast<int>(std::floor(p.x / nodeSize));
		int y = static_cast<int>(std::floor(p.y / nodeSize));
		int z = static_cast<int>(std::floor(p.z / nodeSize));
		return std::hash<int>()(x) ^ (std::hash<int>()(y) << 1) ^ (std::hash<int>()(z) << 2);
	};

	std::unordered_map<glm::vec3, std::shared_ptr<MeshOctreeNode>, decltype(hash_func)> emptyNodeMap(10, hash_func);
	for (const auto &node : emptyNodes)
	{
		emptyNodeMap[node->mNodeBounds->mCentre] = node;
	}

	// 3. Flood fill
	for (auto &startNode : emptyNodes)
	{
		if (startNode->m_visited)
		{
			continue;
		}

		bool isInside = CheckInside(startNode->mNodeBounds->mCentre);
		std::queue<std::shared_ptr<MeshOctreeNode>> floodQueue;
		floodQueue.push(startNode);
		startNode->m_visited = true;

		while (!floodQueue.empty())
		{
			auto currentNode = floodQueue.front();
			floodQueue.pop();
			currentNode->mIsInside = isInside;

			// Check 6 neighbors
			const static glm::vec3 directions[] = {
				{1, 0, 0}, {-1, 0, 0}, {0, 1, 0}, {0, -1, 0}, {0, 0, 1}, {0, 0, -1}};

			for (const auto &dir : directions)
			{
				glm::vec3 neighborPos = currentNode->mNodeBounds->mCentre + dir * nodeSize;
				auto it = emptyNodeMap.find(neighborPos);
				if (it != emptyNodeMap.end() && !it->second->m_visited)
				{
					it->second->m_visited = true;
					floodQueue.push(it->second);
				}
			}
		}
	}
}

bool MeshOctree::CheckInside(const vec3 &point)
{
	const static vec3 bias{0.01f, 0.015f, 0.01f};
	const static vec3 dir{1, 0, 0};
	vec3 offset = point + bias * glm::vec3(glm::sign(point.x), glm::sign(point.y), glm::sign(point.z));

	auto nodes = QueryLeafNodesAlongDirection(offset, dir);
	int crossings = 0;

	std::unordered_set<int> allTriangleIndices;

	for (auto &node : nodes)
	{
		for (auto triIndex : node->mTriangleIndices)
		{
			allTriangleIndices.insert(triIndex);
		}
	}

	for (int triIndex : allTriangleIndices)
	{
		int i0 = m_mesh.indexBuffer[triIndex * 3];
		int i1 = m_mesh.indexBuffer[triIndex * 3 + 1];
		int i2 = m_mesh.indexBuffer[triIndex * 3 + 2];

		const vec3 &v0 = m_mesh.vertexBuffer[i0].pos;
		const vec3 &v1 = m_mesh.vertexBuffer[i1].pos;
		const vec3 &v2 = m_mesh.vertexBuffer[i2].pos;

		if (RayIntersectsTriangle(offset, dir, v0, v1, v2))
		{
			crossings++;
		}
	}
	return (crossings % 2) == 1;
}
bool MeshOctree::RayIntersectsTriangle(vec3 &origin, const vec3 &dir, const vec3 &v0, const vec3 &v1, const vec3 &v2)
{
	static const float epsilon = 1e-6f;
	vec3 edge1 = v1 - v0;
	vec3 edge2 = v2 - v0;
	vec3 h = glm::cross(dir, edge2);
	float a = glm::dot(edge1, h);
	if (a > -epsilon && a < epsilon)
		return false;

	float f = 1.0f / a;
	vec3 s = origin - v0;
	float u = f * glm::dot(s, h);
	if (u < 0.0f || u > 1.0f)
		return false;

	vec3 q = glm::cross(s, edge1);
	float v = f * glm::dot(dir, q);
	if (v < 0.0f || u + v > 1.0f)
		return false;

	float t = f * glm::dot(edge2, q);
	return t > epsilon;
}
std::vector<std::shared_ptr<MeshOctreeNode>> MeshOctree::QueryLeafNodesAlongDirection(vec3 &rayOrigin, const vec3 &rayDir)
{
	Ray ray{rayOrigin, const_cast<vec3 &>(rayDir)};
	std::vector<std::shared_ptr<MeshOctreeNode>> result;
	QueryLeafNodesRecursive(m_rootNode, ray, result);
	return result;
}
void MeshOctree::QueryLeafNodesRecursive(std::shared_ptr<MeshOctreeNode> node, const Ray &ray, std::vector<std::shared_ptr<MeshOctreeNode>> &result)
{
	float tMin = 0.0f;
	float tMax = 9999.0f;
	// 检测射线是否与当前节点的包围盒相交
	if (!IntersectRayBounds(ray, node->mNodeBounds, tMin, tMax))
	{
		return;
	}

	// 如果是叶节点，记录
	if (node->IsLeafNode())
	{
		result.push_back(node);
		return;
	}
	if (node->mChildren[0] != nullptr)
	{
		// 递归遍历子节点（按射线方向优化顺序）
		for (auto &child : node->mChildren)
		{
			QueryLeafNodesRecursive(child, ray, result);
		}
	}
}

bool MeshOctree::IntersectRayBounds(const Ray &ray, std::shared_ptr<Bounds> bounds, float &tMin, float &tMax)
{
	vec3 boundsMin = bounds->GetMin();
	vec3 boundsMax = bounds->GetMax();
	for (int i = 0; i < 3; i++)
	{
		float dir = ray.mDir[i];
		float invDir = std::abs(dir) > 1e-8f ? 1.0f / dir : 9999.0f;
		float t0 = (boundsMin[i] - ray.mOrigin[i]) * invDir;
		float t1 = (boundsMax[i] - ray.mOrigin[i]) * invDir;

		if (invDir < 0.0f)
		{
			float temp = t0;
			t0 = t1;
			t1 = temp;
		}

		tMin = std::max(tMin, t0);
		tMax = std::min(tMax, t1);

		if (tMax < tMin)
			return false;
	}

	return true;
}