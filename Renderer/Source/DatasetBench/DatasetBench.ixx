module;
#include <cstdint>

export module DatasetBench;

import std;
import ConfigMod;

// 一次批量运行的命令行参数。路径与 Config.json5 的 modelPath 相同，相对资源目录。
export struct DatasetBenchOptions
{
    std::string modelPath;
    std::string listPath;
    std::string outPath{"bench.csv"};
    uint32_t resolution{128};
    uint32_t queries{4096};
    uint32_t repeat{50};
    uint32_t warmup{50};
};

// 渲染器测完若干帧后交回的摘要。求值时间不在这里，采样 pass 还没有。
export struct BenchContext
{
    std::string model;
    uint32_t triangles{0};
    uint32_t resolution{128};
    uint32_t queries{4096};
    double jfaPrecompMs{0.0};
    double multiViewPrecompMs{0.0};
    bool hasPrecomp{false};
};

export struct BenchRow
{
    std::string model;
    uint32_t triangles{0};
    std::string method;
    uint32_t resolution{0};
    uint32_t queries{0};
    std::optional<double> precompMs;
    std::optional<double> evalMs;
    std::optional<double> evalPerQueryUs;
    std::string status;
};

// 三种方法共用的记录接口。这一轮只根据 BenchContext 填行，不自己提交 GPU 命令。
export class SdfMethod
{
public:
    virtual ~SdfMethod();
    virtual std::string Name() const = 0;
    virtual BenchRow Run(const BenchContext &context) = 0;
};

export class JfaSdfMethod : public SdfMethod
{
public:
    std::string Name() const override;
    BenchRow Run(const BenchContext &context) override;
};

export class MultiViewSdfMethod : public SdfMethod
{
public:
    std::string Name() const override;
    BenchRow Run(const BenchContext &context) override;
};

export class BvhSdfMethod : public SdfMethod
{
public:
    std::string Name() const override;
    BenchRow Run(const BenchContext &context) override;
};

export class DatasetBench
{
public:
    static bool RequestsBatch(int argc, char **argv);
    static std::optional<DatasetBenchOptions> ParseArgs(int argc, char **argv);
    static std::vector<std::string> ModelPaths(const DatasetBenchOptions &options);
    static void ApplyToConfig(Config &config, const DatasetBenchOptions &options);
    static std::optional<double> Mean(const std::vector<double> &samples);
    static std::vector<BenchRow> MakeRows(const BenchContext &context);
    static void AppendRows(const std::string &path, const std::vector<BenchRow> &rows);

    static constexpr const char *kUsage =
        "usage: MyToyRenderer --batch <model.gltf> | --batch-list <models.txt> [--out bench.csv] [--resolution 128] [--queries 4096] [--warmup 50] [--repeat 50]";
};
