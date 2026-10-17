#pragma once
#include <filesystem>
#include <string>
#include <vector>
#include <stdexcept>

struct LaunchOptions
{
    bool debug = false;
    bool createJs = false;
    std::filesystem::path definitions;
    std::filesystem::path windowStateFile;
};

inline LaunchOptions resolveLaunchOptions(const std::vector<std::wstring>& arguments,
    const std::filesystem::path& currentDirectory, const std::filesystem::path& userDirectory,
    const std::filesystem::path& defaultFile)
{
    LaunchOptions result;
    std::filesystem::path debugPath;
    std::filesystem::path includePath;
    std::filesystem::path createPath;
    for (size_t i = 0; i < arguments.size(); ++i)
    {
        if (arguments[i] == L"-debug")
        {
            result.debug = true;
            if (i + 1 < arguments.size() && !arguments[i + 1].starts_with(L"-")) debugPath = arguments[++i];
        }
        else if (arguments[i] == L"-createjs")
        {
            result.createJs = true;
            if (i + 1 < arguments.size() && !arguments[i + 1].starts_with(L"-")) createPath = arguments[++i];
        }
        else if (arguments[i] == L"-include")
        {
            if (i + 1 == arguments.size() || arguments[i + 1].starts_with(L"-"))
                throw std::runtime_error("-include requires a filename");
            includePath = arguments[++i];
        }
        else throw std::runtime_error("Unknown command line argument");
    }
    if (result.createJs)
    {
        if (result.debug || !includePath.empty())
            throw std::runtime_error("-createjs cannot be combined with -debug or -include");
        result.definitions = createPath.empty() ? defaultFile :
            createPath.is_absolute() ? createPath :
            (createPath.has_parent_path() ? currentDirectory : userDirectory) / createPath;
        return result;
    }
    const auto specified = !debugPath.empty() ? debugPath : includePath;
    result.definitions = defaultFile;
    if (specified.empty()) return result;
    std::error_code error;
    auto candidate = specified.is_absolute() ? specified : currentDirectory / specified;
    if (!specified.has_parent_path() && !std::filesystem::exists(candidate, error))
        candidate = userDirectory / specified;
    // debugでは指定先の不在もエラーとして報告する。通常起動は既定ファイルへ戻す。
    if (result.debug || std::filesystem::exists(candidate, error)) result.definitions = candidate;
    return result;
}
