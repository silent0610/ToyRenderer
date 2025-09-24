module;
#include <string>

export module RhiLogger;
import Logger;

// RHI-specific logging utilities
export namespace Rhi {
    
    class RhiLogger {
    private:
        static Logging::Logger& GetLogger() {
            static Logging::Logger rhiLogger("RHI");
            return rhiLogger;
        }
        
    public:
        template<typename... Args>
        static void Info(const std::string& format, Args&&... args) {
            GetLogger().Info(format, std::forward<Args>(args)...);
        }
        
        template<typename... Args>
        static void Warn(const std::string& format, Args&&... args) {
            GetLogger().Warn(format, std::forward<Args>(args)...);
        }
        
        template<typename... Args>
        static void Error(const std::string& format, Args&&... args) {
            GetLogger().Error(format, std::forward<Args>(args)...);
        }
        
        template<typename... Args>
        static void Debug(const std::string& format, Args&&... args) {
            GetLogger().Debug(format, std::forward<Args>(args)...);
        }
        
        template<typename... Args>
        static void Critical(const std::string& format, Args&&... args) {
            GetLogger().Critical(format, std::forward<Args>(args)...);
        }
        
        // Configure RHI logging
        static void SetLogLevel(Logging::LogLevel level) {
            GetLogger().SetLevel(level);
        }
        
        static void EnableFileLogging(const std::string& filename) {
            GetLogger().AddFileOutput(filename);
        }
    };
}

// Convenience functions for RHI logging (avoiding macros)
export namespace RhiLog {
    template<typename... Args>
    void Info(const std::string& format, Args&&... args) {
        Rhi::RhiLogger::Info(format, std::forward<Args>(args)...);
    }
    
    template<typename... Args>
    void Warn(const std::string& format, Args&&... args) {
        Rhi::RhiLogger::Warn(format, std::forward<Args>(args)...);
    }
    
    template<typename... Args>
    void Error(const std::string& format, Args&&... args) {
        Rhi::RhiLogger::Error(format, std::forward<Args>(args)...);
    }
    
    template<typename... Args>
    void Debug(const std::string& format, Args&&... args) {
        Rhi::RhiLogger::Debug(format, std::forward<Args>(args)...);
    }
    
    template<typename... Args>
    void Critical(const std::string& format, Args&&... args) {
        Rhi::RhiLogger::Critical(format, std::forward<Args>(args)...);
    }
}