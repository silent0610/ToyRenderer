module;
// 高性能spdlog封装 - 只包含必要头文件
#include "spdlog/spdlog.h"
#include "spdlog/sinks/stdout_color_sinks.h"
#include "spdlog/sinks/basic_file_sink.h"
#include "spdlog/async.h" // 异步logging
#include <memory>
#include <string>

export module Logger;

// 导出spdlog核心功能
export namespace spdlog
{
    using spdlog::info;
    // 日志级别
    namespace level
    {
        using ::spdlog::level::critical;
        using ::spdlog::level::debug;
        using ::spdlog::level::err;
        using ::spdlog::level::info;
        using ::spdlog::level::level_enum;
        using ::spdlog::level::off;
        using ::spdlog::level::trace;
        using ::spdlog::level::warn;
    }

    // 核心类型
    using ::spdlog::logger;
    using ::spdlog::sink_ptr;

    // 核心函数
    using ::spdlog::drop;
    using ::spdlog::get;
    using ::spdlog::register_logger;
    using ::spdlog::set_level;
    using ::spdlog::set_pattern;

    // Sink类型
    namespace sinks
    {
        using ::spdlog::sinks::basic_file_sink_mt;
        using ::spdlog::sinks::stdout_color_sink_mt;
    }
}

export namespace Log
{

    // 基础logging函数 - 性能最高
    void Trace(const std::string &msg);
    void Debug(const std::string &msg);
    void Info(const std::string &msg);
    void Warn(const std::string &msg);
    void Error(const std::string &msg);
    void Critical(const std::string &msg);

    // 配置函数
    void SetLevel(spdlog::level::level_enum level);
    void SetPattern(const std::string &pattern);
    void AddFileOutput(const std::string &filename);

    // 异步模式控制
    void EnableAsyncMode(size_t queue_size = 8192);
    void DisableAsyncMode();

    // 获取原始logger - 用于std::fmt格式化
    std::shared_ptr<spdlog::logger> GetLogger();
}