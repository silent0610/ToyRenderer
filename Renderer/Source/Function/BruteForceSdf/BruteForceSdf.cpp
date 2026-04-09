module;
module BruteForceSdf;
import ThreadPool;
void BruteForceSdf::Initialize(const SdfParameters& params)
{
    params_ = params;
    Log::Info(std::format("BruteForceSdf initialized with resolution {}x{}x{}", params_.voxelResolution.x, params_.voxelResolution.y,
                          params_.voxelResolution.z));
}

void BruteForceSdf::SetModel(const vkglTF::Model* model)
{
    model_ = model;
    modelToStandard_ = glm::mat4(1.0f);
    triangles_.clear();
    triangleIndices_.clear();
    bvhNodes_.clear();
    rootNode_ = 0;
    accelerationReady_ = false;

    if (model_)
    {
        modelToStandard_ = model_->GetModelToStandardTransform();
        BuildAccelerationStructure();
        Log::Info(std::format("BruteForceSdf: Model set with {} nodes", model_->nodes.size()));
        Log::Info(std::format("BruteForceSdf: Built acceleration structure for {} triangles and {} BVH nodes",
                              triangles_.size(), bvhNodes_.size()));
    }
}

void BruteForceSdf::BuildAccelerationStructure()
{
    if (!model_)
    {
        return;
    }

    const auto &vertexBuffer = model_->vertexBuffer;
    const auto &indexBuffer = model_->indexBuffer;

    triangles_.reserve(indexBuffer.size() / 3);

    for (size_t i = 0; i + 2 < indexBuffer.size(); i += 3)
    {
        const uint32_t idx0 = indexBuffer[i + 0];
        const uint32_t idx1 = indexBuffer[i + 1];
        const uint32_t idx2 = indexBuffer[i + 2];

        if (idx0 >= vertexBuffer.size() || idx1 >= vertexBuffer.size() || idx2 >= vertexBuffer.size())
        {
            continue;
        }

        TriangleData triangle{};
        triangle.v0 = glm::vec3(modelToStandard_ * glm::vec4(vertexBuffer[idx0].pos, 1.0f));
        triangle.v1 = glm::vec3(modelToStandard_ * glm::vec4(vertexBuffer[idx1].pos, 1.0f));
        triangle.v2 = glm::vec3(modelToStandard_ * glm::vec4(vertexBuffer[idx2].pos, 1.0f));

        triangle.boundsMin = glm::min(triangle.v0, glm::min(triangle.v1, triangle.v2));
        triangle.boundsMax = glm::max(triangle.v0, glm::max(triangle.v1, triangle.v2));

        const glm::vec3 normal = glm::cross(triangle.v1 - triangle.v0, triangle.v2 - triangle.v0);
        const float normalLengthSq = glm::dot(normal, normal);
        triangle.normal = (normalLengthSq > 1e-20f) ? glm::normalize(normal) : glm::vec3(0.0f, 1.0f, 0.0f);

        triangles_.push_back(triangle);
    }

    triangleIndices_.resize(triangles_.size());
    std::iota(triangleIndices_.begin(), triangleIndices_.end(), 0u);

    if (triangles_.empty())
    {
        accelerationReady_ = false;
        return;
    }

    bvhNodes_.clear();
    bvhNodes_.reserve(triangles_.size() * 2);
    rootNode_ = BuildBvhRecursive(0, static_cast<uint32_t>(triangles_.size()));
    accelerationReady_ = true;
}

uint32_t BruteForceSdf::BuildBvhRecursive(uint32_t first, uint32_t count)
{
    const uint32_t nodeIndex = static_cast<uint32_t>(bvhNodes_.size());
    bvhNodes_.push_back({});

    const float maxFloat = std::numeric_limits<float>::max();
    glm::vec3 boundsMin(maxFloat);
    glm::vec3 boundsMax(-maxFloat);
    glm::vec3 centroidMin(maxFloat);
    glm::vec3 centroidMax(-maxFloat);

    for (uint32_t i = first; i < first + count; ++i)
    {
        const TriangleData &triangle = triangles_[triangleIndices_[i]];
        boundsMin = glm::min(boundsMin, triangle.boundsMin);
        boundsMax = glm::max(boundsMax, triangle.boundsMax);

        const glm::vec3 centroid = (triangle.boundsMin + triangle.boundsMax) * 0.5f;
        centroidMin = glm::min(centroidMin, centroid);
        centroidMax = glm::max(centroidMax, centroid);
    }

    bvhNodes_[nodeIndex].boundsMin = boundsMin;
    bvhNodes_[nodeIndex].boundsMax = boundsMax;

    constexpr uint32_t kLeafTriangleCount = 8;
    if (count <= kLeafTriangleCount)
    {
        bvhNodes_[nodeIndex].leftFirst = first;
        bvhNodes_[nodeIndex].triangleCount = count;
        return nodeIndex;
    }

    const glm::vec3 centroidExtent = centroidMax - centroidMin;
    uint32_t splitAxis = 0;
    if (centroidExtent.y > centroidExtent.x)
    {
        splitAxis = 1;
    }
    if ((splitAxis == 0 && centroidExtent.z > centroidExtent.x) ||
        (splitAxis == 1 && centroidExtent.z > centroidExtent.y))
    {
        splitAxis = 2;
    }

    const uint32_t mid = first + count / 2;
    auto begin = triangleIndices_.begin() + first;
    auto middle = triangleIndices_.begin() + mid;
    auto end = triangleIndices_.begin() + first + count;

    std::nth_element(begin, middle, end,
                     [this, splitAxis](uint32_t a, uint32_t b)
                     {
                         const TriangleData &triangleA = triangles_[a];
                         const TriangleData &triangleB = triangles_[b];
                         const float centroidA = ((triangleA.boundsMin + triangleA.boundsMax) * 0.5f)[splitAxis];
                         const float centroidB = ((triangleB.boundsMin + triangleB.boundsMax) * 0.5f)[splitAxis];
                         return centroidA < centroidB;
                     });

    const uint32_t leftChild = BuildBvhRecursive(first, mid - first);
    const uint32_t rightChild = BuildBvhRecursive(mid, first + count - mid);

    bvhNodes_[nodeIndex].leftFirst = leftChild;
    bvhNodes_[nodeIndex].rightChild = rightChild;
    bvhNodes_[nodeIndex].triangleCount = 0;
    return nodeIndex;
}

void BruteForceSdf::GenerateGroundTruth(std::vector<float> &sdfData)
{
    if (!model_)
    {
        Log::Error("BruteForceSdf: No model set");
        return ;
    }

    //glm::vec3 worldPos = GetVoxelWorldPosition(0, 0, 0);
    //float distance = FindClosestDistance(worldPos);

    Log::Info(std::format("BruteForceSdf: Generating ground truth for {} voxels...", sdfData.size()));

    ThreadPool pool;  // 使用默认线程数

    // 按z层分配任务
    for (int z = 0; z < params_.voxelResolution.z; ++z)
    {
        pool.Enqueue([this, &sdfData, z]() {
            for (int y = 0; y < params_.voxelResolution.y; ++y)
            {
                for (int x = 0; x < params_.voxelResolution.x; ++x)
                {
                    int sampleZ = params_.voxelResolution.z - 1 - z; // 翻转 Z (对应 Python flip_x)
                    int sampleY = params_.voxelResolution.y - 1 - y; // 翻转 Y (对应 Python flip_y)
                    int sampleX = x;                                 // X 保持不变 (对应 Python flip_z=False)
                                                                     // 使用翻转后的坐标获取世界位置进行计算
                    glm::vec3 worldPos = GetVoxelWorldPosition(sampleX, sampleY, sampleZ);
                    float distance = FindClosestDistance(worldPos);

                    // 写入索引保持原始顺序 (x, y, z)，确保内存是线性写入的
                    size_t index = GetVoxelIndex(x, y, z);
                    sdfData[index] = distance;
                }
            }

            // 线程安全的进度输出
            if ((z + 1) % 8 == 0)
            {
                float progress = static_cast<float>(z + 1) / params_.voxelResolution.z * 100.0f;
                Log::Info(std::format("Progress: {:.1f}%", progress));
            }
        });
    }

    // 等待所有任务完成
    pool.Wait();

    Log::Info("BruteForceSdf: Ground truth generation completed");

}

glm::vec3 BruteForceSdf::GetVoxelWorldPosition(int x, int y, int z) const
{
    return params_.origin + glm::vec3((x + 0.5f) * params_.cellSize, (y + 0.5f) * params_.cellSize, (z + 0.5f) * params_.cellSize);
}

size_t BruteForceSdf::GetVoxelIndex(int x, int y, int z) const
{
    return z * params_.voxelResolution.x * params_.voxelResolution.y + y * params_.voxelResolution.x + x;
}

float BruteForceSdf::FindClosestDistance(const glm::vec3& point) const
{
    if (!model_)
    {
        return std::numeric_limits<float>::max();
    }

    if (!accelerationReady_ || bvhNodes_.empty())
    {
        float fallbackDistance = std::numeric_limits<float>::max();
        for (const uint32_t triangleIndex : triangleIndices_)
        {
            const TriangleData &triangle = triangles_[triangleIndex];
            const float distance = SignedDistancePointToTriangle(point, triangle.v0, triangle.v1, triangle.v2, triangle.normal);
            if (distance < fallbackDistance)
            {
                fallbackDistance = distance;
            }
        }
        return fallbackDistance;
    }

    float minDistance = std::numeric_limits<float>::max();
    float minDistanceSq = std::numeric_limits<float>::max();

    std::vector<uint32_t> nodeStack;
    nodeStack.reserve(bvhNodes_.size());
    nodeStack.push_back(rootNode_);

    while (!nodeStack.empty())
    {
        const uint32_t nodeIndex = nodeStack.back();
        nodeStack.pop_back();

        const BvhNode &node = bvhNodes_[nodeIndex];
        const float nodeDistanceSq = DistanceSquaredToAabb(point, node.boundsMin, node.boundsMax);
        if (nodeDistanceSq > minDistanceSq)
        {
            continue;
        }

        if (node.triangleCount > 0)
        {
            for (uint32_t i = 0; i < node.triangleCount; ++i)
            {
                const TriangleData &triangle = triangles_[triangleIndices_[node.leftFirst + i]];
                const float distance = SignedDistancePointToTriangle(point, triangle.v0, triangle.v1, triangle.v2, triangle.normal);
                const float distanceSq = distance * distance;
                if (distanceSq < minDistanceSq)
                {
                    minDistanceSq = distanceSq;
                    minDistance = distance;
                }
            }
            continue;
        }

        const BvhNode &leftChild = bvhNodes_[node.leftFirst];
        const BvhNode &rightChild = bvhNodes_[node.rightChild];
        const float leftDistanceSq = DistanceSquaredToAabb(point, leftChild.boundsMin, leftChild.boundsMax);
        const float rightDistanceSq = DistanceSquaredToAabb(point, rightChild.boundsMin, rightChild.boundsMax);

        if (leftDistanceSq < rightDistanceSq)
        {
            if (rightDistanceSq <= minDistanceSq)
            {
                nodeStack.push_back(node.rightChild);
            }
            if (leftDistanceSq <= minDistanceSq)
            {
                nodeStack.push_back(node.leftFirst);
            }
        }
        else
        {
            if (leftDistanceSq <= minDistanceSq)
            {
                nodeStack.push_back(node.leftFirst);
            }
            if (rightDistanceSq <= minDistanceSq)
            {
                nodeStack.push_back(node.rightChild);
            }
        }
    }

    if (params_.signedDistance)
    {
        return IsPointInsideMesh(point) ? -minDistance : minDistance;
    }

    return minDistance;
}

float BruteForceSdf::DistanceSquaredToAabb(const glm::vec3& point, const glm::vec3& boundsMin, const glm::vec3& boundsMax) const
{
    const glm::vec3 clamped = glm::clamp(point, boundsMin, boundsMax);
    const glm::vec3 delta = point - clamped;
    return glm::dot(delta, delta);
}

bool BruteForceSdf::IntersectRayAabb(const glm::vec3& rayOrigin, const glm::vec3& rayDirInv, bool rayDirNegX, bool rayDirNegY, bool rayDirNegZ,
                                     const glm::vec3& boundsMin, const glm::vec3& boundsMax) const
{
    const float txMin = ((rayDirNegX ? boundsMax.x : boundsMin.x) - rayOrigin.x) * rayDirInv.x;
    const float txMax = ((rayDirNegX ? boundsMin.x : boundsMax.x) - rayOrigin.x) * rayDirInv.x;
    const float tyMin = ((rayDirNegY ? boundsMax.y : boundsMin.y) - rayOrigin.y) * rayDirInv.y;
    const float tyMax = ((rayDirNegY ? boundsMin.y : boundsMax.y) - rayOrigin.y) * rayDirInv.y;

    const float tMin = std::max(txMin, tyMin);
    const float tMax = std::min(txMax, tyMax);
    if (tMax < tMin)
    {
        return false;
    }

    const float tzMin = ((rayDirNegZ ? boundsMax.z : boundsMin.z) - rayOrigin.z) * rayDirInv.z;
    const float tzMax = ((rayDirNegZ ? boundsMin.z : boundsMax.z) - rayOrigin.z) * rayDirInv.z;

    const float tEnter = std::max(tMin, tzMin);
    const float tExit = std::min(tMax, tzMax);
    return tExit >= std::max(tEnter, 0.0f);
}

bool BruteForceSdf::IntersectRayTriangle(const glm::vec3& rayOrigin, const glm::vec3& rayDir,
                                         const TriangleData& triangle, float& t) const
{
    constexpr float epsilon = 1e-6f;

    const glm::vec3 edge1 = triangle.v1 - triangle.v0;
    const glm::vec3 edge2 = triangle.v2 - triangle.v0;
    const glm::vec3 pvec = glm::cross(rayDir, edge2);
    const float det = glm::dot(edge1, pvec);

    if (std::abs(det) < epsilon)
    {
        return false;
    }

    const float invDet = 1.0f / det;
    const glm::vec3 tvec = rayOrigin - triangle.v0;
    const float u = glm::dot(tvec, pvec) * invDet;
    if (u < 0.0f || u > 1.0f)
    {
        return false;
    }

    const glm::vec3 qvec = glm::cross(tvec, edge1);
    const float v = glm::dot(rayDir, qvec) * invDet;
    if (v < 0.0f || (u + v) > 1.0f)
    {
        return false;
    }

    t = glm::dot(edge2, qvec) * invDet;
    return t > epsilon;
}

float BruteForceSdf::SignedDistancePointToTriangle(const glm::vec3& point, const glm::vec3& v0, const glm::vec3& v1, const glm::vec3& v2,
                                                   const glm::vec3& normal) const
{
    // AMD TressFX算法的C++实现
    glm::vec3 edge0 = v1 - v0;
    glm::vec3 edge1 = v2 - v1;
    glm::vec3 edge2 = v0 - v2;

    glm::vec3 v0ToPoint = point - v0;
    glm::vec3 v1ToPoint = point - v1;
    glm::vec3 v2ToPoint = point - v2;

    // 计算到三个边的距离
    float dist0 = DistancePointToEdge(point, v0, v1);
    float dist1 = DistancePointToEdge(point, v1, v2);
    float dist2 = DistancePointToEdge(point, v2, v0);

    float minEdgeDistance = std::min({dist0, dist1, dist2});

    // 检查点是否在三角形平面上的投影在三角形内部
    glm::vec3 cross0 = glm::cross(edge0, v0ToPoint);
    glm::vec3 cross1 = glm::cross(edge1, v1ToPoint);
    glm::vec3 cross2 = glm::cross(edge2, v2ToPoint);

    bool inside = (glm::dot(cross0, normal) >= 0.0f) && (glm::dot(cross1, normal) >= 0.0f) && (glm::dot(cross2, normal) >= 0.0f);

    float distance;
    if (inside)
    {
        // 点在三角形内部，距离是到平面的距离
        distance = std::abs(glm::dot(v0ToPoint, normal));
    }
    else
    {
        // 点在三角形外部，距离是到最近边的距离
        distance = minEdgeDistance;
    }

    // 确定符号（简化版本，假设normal指向外部）
    // if (params_.signedDistance)
    //{
    //    float planeDist = glm::dot(v0ToPoint, normal);
    //    distance = planeDist >= 0.0f ? distance : -distance;
    //}

    return distance;
}

float BruteForceSdf::DistancePointToEdge(const glm::vec3& point, const glm::vec3& edgeStart, const glm::vec3& edgeEnd) const
{
    glm::vec3 edge = edgeEnd - edgeStart;
    glm::vec3 pointToStart = point - edgeStart;

    float edgeLength = glm::length(edge);
    if (edgeLength < 1e-6f)
    {
        // 退化边，返回到起点的距离
        return glm::length(pointToStart);
    }

    glm::vec3 edgeNormalized = edge / edgeLength;
    float projection = glm::dot(pointToStart, edgeNormalized);

    // 夹在边的范围内
    projection = glm::clamp(projection, 0.0f, edgeLength);

    glm::vec3 closestPoint = edgeStart + projection * edgeNormalized;
    return glm::length(point - closestPoint);
}

bool BruteForceSdf::IsPointInsideMesh(const glm::vec3& point) const
{
    if (!accelerationReady_ || bvhNodes_.empty())
    {
        return false;
    }

    constexpr float epsilon = 1e-4f;
    const glm::vec3 rayDir = glm::normalize(glm::vec3(0.7548777f, 0.5698403f, 0.3216544f));
    const glm::vec3 rayOrigin = point + rayDir * epsilon;
    const glm::vec3 rayDirInv(1.0f / rayDir.x, 1.0f / rayDir.y, 1.0f / rayDir.z);
    const bool rayDirNegX = rayDir.x < 0.0f;
    const bool rayDirNegY = rayDir.y < 0.0f;
    const bool rayDirNegZ = rayDir.z < 0.0f;

    uint32_t hitCount = 0;
    std::vector<uint32_t> nodeStack;
    nodeStack.reserve(bvhNodes_.size());
    nodeStack.push_back(rootNode_);

    while (!nodeStack.empty())
    {
        const uint32_t nodeIndex = nodeStack.back();
        nodeStack.pop_back();

        const BvhNode &node = bvhNodes_[nodeIndex];
        if (!IntersectRayAabb(rayOrigin, rayDirInv, rayDirNegX, rayDirNegY, rayDirNegZ, node.boundsMin, node.boundsMax))
        {
            continue;
        }

        if (node.triangleCount > 0)
        {
            for (uint32_t i = 0; i < node.triangleCount; ++i)
            {
                const TriangleData &triangle = triangles_[triangleIndices_[node.leftFirst + i]];
                float t = 0.0f;
                if (IntersectRayTriangle(rayOrigin, rayDir, triangle, t))
                {
                    ++hitCount;
                }
            }
            continue;
        }

        nodeStack.push_back(node.leftFirst);
        nodeStack.push_back(node.rightChild);
    }

    return (hitCount % 2u) == 1u;
}

void BruteForceSdf::SaveToFile(const std::vector<float>& data, const std::string& filename)
{
    std::ofstream file(filename, std::ios::binary);
    if (!file)
    {
        Log::Error(std::format("Failed to open file for writing: {}", filename));
        return;
    }

    //// 写入头信息
    //file.write(reinterpret_cast<const char*>(&params_.voxelResolution), sizeof(params_.voxelResolution));
    //file.write(reinterpret_cast<const char*>(&params_.origin), sizeof(params_.origin));
    //file.write(reinterpret_cast<const char*>(&params_.cellSize), sizeof(params_.cellSize));

    // 写入数据
    file.write(reinterpret_cast<const char*>(data.data()), data.size() * sizeof(float));

    Log::Info(std::format("BruteForceSdf: Saved {} voxels to {}", data.size(), filename));
}

std::vector<float> BruteForceSdf::LoadFromFile(const std::string& filename)
{
    std::ifstream file(filename, std::ios::binary);
    if (!file)
    {
        Log::Error(std::format("Failed to open file for reading: {}", filename));
        return {};
    }

    // 读取头信息
    glm::ivec3 resolution;
    glm::vec3 origin;
    float cellSize;

    file.read(reinterpret_cast<char*>(&resolution), sizeof(resolution));
    file.read(reinterpret_cast<char*>(&origin), sizeof(origin));
    file.read(reinterpret_cast<char*>(&cellSize), sizeof(cellSize));

    size_t dataSize = resolution.x * resolution.y * resolution.z;
    std::vector<float> data(dataSize);

    file.read(reinterpret_cast<char*>(data.data()), dataSize * sizeof(float));

    Log::Info(std::format("BruteForceSdf: Loaded {} voxels from {}", dataSize, filename));
    return data;
}

BruteForceSdf::ComparisonResult BruteForceSdf::CompareResults(const std::vector<float>& groundTruth, const std::vector<float>& testData,
                                                              float errorThreshold)
{
    ComparisonResult result;

    if (groundTruth.size() != testData.size())
    {
        Log::Error("BruteForceSdf: Data size mismatch for comparison");
        return result;
    }

    result.totalVoxels = groundTruth.size();
    double sumError = 0.0;
    double sumSquaredError = 0.0;

    for (size_t i = 0; i < groundTruth.size(); ++i)
    {
        float error = std::abs(groundTruth[i] - testData[i]);

        result.maxError = std::max(result.maxError, error);
        sumError += error;
        sumSquaredError += error * error;

        if (error > errorThreshold)
        {
            result.errorVoxels++;
        }
    }

    result.avgError = static_cast<float>(sumError / result.totalVoxels);
    result.rmseError = static_cast<float>(std::sqrt(sumSquaredError / result.totalVoxels));

    Log::Info("Comparison Results:");
    Log::Info(std::format("  Max Error: {:.6f}", result.maxError));
    Log::Info(std::format("  Avg Error: {:.6f}", result.avgError));
    Log::Info(std::format("  RMSE Error: {:.6f}", result.rmseError));
    Log::Info(std::format("  Error Voxels: {} / {} ({:.2f}%)", result.errorVoxels, result.totalVoxels,
                          static_cast<float>(result.errorVoxels) / result.totalVoxels * 100.0f));

    return result;
}
