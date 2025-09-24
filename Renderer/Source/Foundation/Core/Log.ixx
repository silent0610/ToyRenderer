module;

export module Log;
import std;

// Simple logging system as per simplified migration plan
export namespace Core
{
    // Simple log macros - using iostream for now, can upgrade to spdlog later
    #define LOG_INFO(msg) std::cout << "[INFO] " << msg << std::endl
    #define LOG_ERROR(msg) std::cerr << "[ERROR] " << msg << std::endl
    #define LOG_WARN(msg) std::cout << "[WARN] " << msg << std::endl
    #define LOG_DEBUG(msg) std::cout << "[DEBUG] " << msg << std::endl
}