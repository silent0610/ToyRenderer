module;

export module String;
import std;

// String utilities as per simplified migration plan
export namespace Core
{
    // Use standard string types
    using String = std::string;
    using StringView = std::string_view;
    using WString = std::wstring;
    
    // Use std::format from C++20 as mentioned in migration plan
    using std::format;
}