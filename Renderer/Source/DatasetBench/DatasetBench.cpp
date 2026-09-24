module;
#include <cstdint>

module DatasetBench;

namespace
{
std::optional<std::string> TakeValue(int argc, char **argv, int &index, const char *flag)
{
    if (index + 1 >= argc)
    {
        std::cerr << "missing value for " << flag << "\n";
        return std::nullopt;
    }
    ++index;
    return std::string(argv[index]);
}

std::optional<uint32_t> ParseU32(const std::string &text)
{
    try
    {
        std::size_t used = 0;
        const unsigned long value = std::stoul(text, &used, 10);
        if (used != text.size() || value > std::numeric_limits<uint32_t>::max())
        {
            return std::nullopt;
        }
        return static_cast<uint32_t>(value);
    }
    catch (const std::exception &)
    {
        return std::nullopt;
    }
}

std::string FormatOptional(const std::optional<double> &value)
{
    if (!value)
    {
        return {};
    }
    return std::format("{:.6f}", *value);
}

BenchRow MakeBaseRow(const BenchContext &context, const std::string &method, const std::string &status)
{
    BenchRow row;
    row.model = context.model;
    row.triangles = context.triangles;
    row.method = method;
    row.resolution = context.resolution;
    row.queries = context.queries;
    row.status = status;
    return row;
}
} // namespace

SdfMethod::~SdfMethod() = default;

std::string JfaSdfMethod::Name() const
{
    return "JFA";
}

BenchRow JfaSdfMethod::Run(const BenchContext &context)
{
    if (!context.hasPrecomp)
    {
        return MakeBaseRow(context, Name(), "error");
    }
    BenchRow row = MakeBaseRow(context, Name(), "precomp_only");
    row.precompMs = context.jfaPrecompMs;
    return row;
}

std::string MultiViewSdfMethod::Name() const
{
    return "MultiView";
}

BenchRow MultiViewSdfMethod::Run(const BenchContext &context)
{
    if (!context.hasPrecomp)
    {
        return MakeBaseRow(context, Name(), "error");
    }
    BenchRow row = MakeBaseRow(context, Name(), "precomp_only");
    row.precompMs = context.multiViewPrecompMs;
    return row;
}

std::string BvhSdfMethod::Name() const
{
    return "BVH";
}

BenchRow BvhSdfMethod::Run(const BenchContext &context)
{
    return MakeBaseRow(context, Name(), "skipped");
}

bool DatasetBench::RequestsBatch(int argc, char **argv)
{
    for (int i = 1; i < argc; ++i)
    {
        const std::string arg = argv[i];
        if (arg == "--batch" || arg == "--batch-list")
        {
            return true;
        }
    }
    return false;
}

std::optional<DatasetBenchOptions> DatasetBench::ParseArgs(int argc, char **argv)
{
    DatasetBenchOptions options;
    bool sawBatch = false;

    for (int i = 1; i < argc; ++i)
    {
        const std::string arg = argv[i];
        if (arg == "v")
        {
            continue;
        }
        if (arg == "--batch" || arg == "--batch-list")
        {
            const auto value = TakeValue(argc, argv, i, arg.c_str());
            if (!value)
            {
                std::cerr << kUsage << "\n";
                return std::nullopt;
            }
            if (arg == "--batch")
            {
                options.modelPath = *value;
            }
            else
            {
                options.listPath = *value;
            }
            sawBatch = true;
            continue;
        }
        if (arg == "--out")
        {
            const auto value = TakeValue(argc, argv, i, "--out");
            if (!value || value->empty())
            {
                std::cerr << kUsage << "\n";
                return std::nullopt;
            }
            options.outPath = *value;
            continue;
        }

        uint32_t *target = nullptr;
        if (arg == "--resolution")
        {
            target = &options.resolution;
        }
        else if (arg == "--queries")
        {
            target = &options.queries;
        }
        else if (arg == "--repeat")
        {
            target = &options.repeat;
        }
        else if (arg == "--warmup")
        {
            target = &options.warmup;
        }
        else if (arg == "--cameras")
        {
            target = &options.cameras;
        }
        else
        {
            std::cerr << "unknown arg: " << arg << "\n" << kUsage << "\n";
            return std::nullopt;
        }

        const auto value = TakeValue(argc, argv, i, arg.c_str());
        if (!value)
        {
            std::cerr << kUsage << "\n";
            return std::nullopt;
        }
        const auto parsed = ParseU32(*value);
        if (!parsed)
        {
            std::cerr << "invalid number for " << arg << ": " << *value << "\n";
            return std::nullopt;
        }
        *target = *parsed;
    }

    if (!sawBatch || (options.modelPath.empty() && options.listPath.empty()) || (!options.modelPath.empty() && !options.listPath.empty()))
    {
        std::cerr << kUsage << "\n";
        return std::nullopt;
    }
    if (options.resolution < 8 || options.queries == 0 || options.repeat == 0)
    {
        std::cerr << "resolution must be >= 8, queries and repeat must be > 0\n";
        return std::nullopt;
    }
    return options;
}

std::vector<std::string> DatasetBench::ModelPaths(const DatasetBenchOptions &options)
{
    if (options.listPath.empty())
    {
        return {options.modelPath};
    }

    std::ifstream in(options.listPath);
    if (!in)
    {
        throw std::runtime_error("failed to open model list: " + options.listPath);
    }
    std::vector<std::string> paths;
    std::string line;
    while (std::getline(in, line))
    {
        const auto comment = line.find('#');
        if (comment != std::string::npos)
        {
            line.resize(comment);
        }
        while (!line.empty() && (line.back() == '\r' || line.back() == ' ' || line.back() == '\t'))
        {
            line.pop_back();
        }
        std::size_t start = 0;
        while (start < line.size() && (line[start] == ' ' || line[start] == '\t'))
        {
            ++start;
        }
        if (start > 0)
        {
            line.erase(0, start);
        }
        if (!line.empty())
        {
            paths.push_back(line);
        }
    }
    if (paths.empty())
    {
        throw std::runtime_error("model list is empty: " + options.listPath);
    }
    return paths;
}

void DatasetBench::ApplyToConfig(Config &config, const DatasetBenchOptions &options)
{
    config.modelPath = ModelPaths(options).front();
    config.Sdf.SdfResolution = options.resolution;
    if (options.cameras > 0)
    {
        config.Sdf.MaxCameraNum = options.cameras;
    }
    config.Dynamic.enable = false;
}

std::optional<double> DatasetBench::Mean(const std::vector<double> &samples)
{
    if (samples.empty())
    {
        return std::nullopt;
    }
    double sum = 0.0;
    for (const double sample : samples)
    {
        sum += sample;
    }
    return sum / static_cast<double>(samples.size());
}

std::vector<BenchRow> DatasetBench::MakeRows(const BenchContext &context)
{
    JfaSdfMethod jfa;
    MultiViewSdfMethod multiView;
    BvhSdfMethod bvh;
    return {jfa.Run(context), multiView.Run(context), bvh.Run(context)};
}

void DatasetBench::AppendRows(const std::string &path, const std::vector<BenchRow> &rows)
{
    const bool needsHeader = !std::filesystem::exists(path) || std::filesystem::file_size(path) == 0;
    std::ofstream out(path, std::ios::app);
    if (!out)
    {
        throw std::runtime_error("failed to open bench csv: " + path);
    }
    if (needsHeader)
    {
        out << "model,triangles,method,resolution,queries,precomp_ms,eval_ms,eval_per_query_us,status\n";
    }
    for (const BenchRow &row : rows)
    {
        out << row.model << "," << row.triangles << "," << row.method << "," << row.resolution << "," << row.queries << ","
            << FormatOptional(row.precompMs) << "," << FormatOptional(row.evalMs) << "," << FormatOptional(row.evalPerQueryUs) << ","
            << row.status << "\n";
    }
}
