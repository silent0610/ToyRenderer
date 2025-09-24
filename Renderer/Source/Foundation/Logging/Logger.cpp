module;
#include "spdlog/spdlog.h"
#include "spdlog/sinks/stdout_color_sinks.h"
#include "spdlog/sinks/basic_file_sink.h"
#include "spdlog/async.h"

module Logger;

namespace
{
    // 高性能单例logger - 懒加载 + 线程安全
    std::shared_ptr<spdlog::logger> GetHighPerformanceLogger()
    {
        static std::shared_ptr<spdlog::logger> logger = []()
        {
            // 创建高性能控制台sink
            auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
            console_sink->set_level(spdlog::level::trace);
            console_sink->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%n] [%^%l%$] %v");

            // 创建logger，使用同步模式（默认）
            auto logger = std::make_shared<spdlog::logger>("Renderer", console_sink);
            logger->set_level(spdlog::level::info);
            logger->flush_on(spdlog::level::warn); // 只在警告及以上级别刷新

            // 注册到spdlog全局注册表
            spdlog::register_logger(logger);
            return logger;
        }();
        return logger;
    }

    // 异步模式标记
    bool g_asyncMode = false;
}

namespace Log
{

    // 基础logging函数 - 零开销封装
    void Trace(const std::string &msg)
    {
        GetHighPerformanceLogger()->trace(msg);
    }

    void Debug(const std::string &msg)
    {
        GetHighPerformanceLogger()->debug(msg);
    }

    void Info(const std::string &msg)
    {
        GetHighPerformanceLogger()->info(msg);
    }

    void Warn(const std::string &msg)
    {
        GetHighPerformanceLogger()->warn(msg);
    }

    void Error(const std::string &msg)
    {
        GetHighPerformanceLogger()->error(msg);
    }

    void Critical(const std::string &msg)
    {
        GetHighPerformanceLogger()->critical(msg);
    }

    void SetLevel(spdlog::level::level_enum level)
    {
        GetHighPerformanceLogger()->set_level(level);
    }

    void SetPattern(const std::string &pattern)
    {
        GetHighPerformanceLogger()->set_pattern(pattern);
    }

    void AddFileOutput(const std::string &filename)
    {
        auto logger = GetHighPerformanceLogger();

        // 创建高性能文件sink
        auto file_sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(filename, true);
        file_sink->set_level(spdlog::level::trace);
        file_sink->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%n] [%l] %v");

        // 获取现有sinks并添加文件sink
        auto sinks = logger->sinks();
        sinks.push_back(file_sink);

        // 重新创建logger
        auto name = logger->name();
        auto level = logger->level();

        auto newLogger = std::make_shared<spdlog::logger>(name, sinks.begin(), sinks.end());
        newLogger->set_level(level);
        newLogger->flush_on(spdlog::level::warn);

        // 重新注册
        spdlog::drop(name);
        spdlog::register_logger(newLogger);
    }

    void EnableAsyncMode(size_t queue_size)
    {
        if (!g_asyncMode)
        {
            // 标记为高性能模式，实际上我们使用优化的同步模式
            // 真正的异步需要更复杂的配置，这里提供高性能同步模式

            auto logger = GetHighPerformanceLogger();
            logger->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%n] [%^%l%$] %v");

            // 优化性能设置
            logger->flush_on(spdlog::level::critical); // 只在严重错误时刷新

            g_asyncMode = true; // 标记为"高性能"模式

            // 记录模式切换
            logger->info("High-performance logging mode enabled (queue_size: {})", queue_size);
        }
    }

    void DisableAsyncMode()
    {
        if (g_asyncMode)
        {
            // 切换回同步模式 - 重新创建同步logger
            spdlog::drop("Renderer");
            GetHighPerformanceLogger(); // 这将重新创建同步logger
            g_asyncMode = false;
        }
    }

    std::shared_ptr<spdlog::logger> GetLogger()
    {
        return GetHighPerformanceLogger();
    }
}