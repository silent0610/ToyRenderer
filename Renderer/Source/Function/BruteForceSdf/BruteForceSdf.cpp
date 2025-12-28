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
    modelToStandard_ = model_->GetModelToStandardTransform();
    if (model_)
    {
        Log::Info(std::format("BruteForceSdf: Model set with {} nodes", model_->nodes.size()));
    }
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
    float minDistance = std::numeric_limits<float>::max();

    // 直接使用model的全局顶点和索引缓冲区
    const auto& vertexBuffer = model_->vertexBuffer;
    const auto& indexBuffer = model_->indexBuffer;

    // 直接遍历所有三角形，每3个索引组成一个三角形
    for (size_t i = 0; i < indexBuffer.size(); i += 3)
    {
        // 获取三角形的三个顶点索引
        uint32_t idx0 = indexBuffer[i + 0];
        uint32_t idx1 = indexBuffer[i + 1];
        uint32_t idx2 = indexBuffer[i + 2];

        // 获取顶点数据（已经是世界坐标）
        const vkglTF::Vertex& vert0 = vertexBuffer[idx0];
        const vkglTF::Vertex& vert1 = vertexBuffer[idx1];
        const vkglTF::Vertex& vert2 = vertexBuffer[idx2];


        glm::vec3 v0 = glm::vec3(modelToStandard_ * glm::vec4(vert0.pos, 1.0f));
        glm::vec3 v1 = glm::vec3(modelToStandard_ * glm::vec4(vert1.pos, 1.0f));
        glm::vec3 v2 = glm::vec3(modelToStandard_ * glm::vec4(vert2.pos, 1.0f));
        // 计算三角形法线
        glm::vec3 normal = glm::normalize(glm::cross(v1 - v0, v2 - v0));

        // 计算距离
        float distance = SignedDistancePointToTriangle(point, v0, v1, v2, normal);

        if (params_.signedDistance)
        {
            // 有符号距离，保留符号
            if (std::abs(distance) < std::abs(minDistance))
            {
                minDistance = distance;
            }
        }
        else
        {
            // 无符号距离，只取绝对值
            float absDistance = std::abs(distance);
            if (absDistance < std::abs(minDistance))
            {
                minDistance = absDistance;
            }
        }
    }

    return minDistance;
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