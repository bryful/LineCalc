#pragma once
#include "framework.h"
#include "../vendor/quickjs/quickjs.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <memory>
#include <optional>
#include <stdexcept>

struct WindowState
{
    int x = 0;
    int y = 0;
    int width = 540;
    int height = 80;
};

inline WindowState fitWindowState(WindowState state, const RECT& work)
{
    state.width = (std::clamp)(state.width, 1, static_cast<int>((std::max)(1L, work.right - work.left)));
    state.height = (std::clamp)(state.height, 1, static_cast<int>((std::max)(1L, work.bottom - work.top)));
    state.x = (std::clamp)(state.x, static_cast<int>(work.left), static_cast<int>(work.right) - state.width);
    state.y = (std::clamp)(state.y, static_cast<int>(work.top), static_cast<int>(work.bottom) - state.height);
    return state;
}

inline std::optional<WindowState> loadWindowState(const std::filesystem::path& path)
{
    if (!std::filesystem::exists(path)) return std::nullopt;
    std::ifstream input(path, std::ios::binary);
    if (!input) throw std::runtime_error("Cannot open window settings");
    std::array<char, 4097> buffer{};
    input.read(buffer.data(), buffer.size());
    const auto size = input.gcount();
    if (input.bad() || size > 4096) throw std::runtime_error("Invalid window settings size");
    std::unique_ptr<JSRuntime, decltype(&JS_FreeRuntime)> runtime(JS_NewRuntime(), JS_FreeRuntime);
    if (!runtime) throw std::runtime_error("Cannot allocate JSON runtime");
    std::unique_ptr<JSContext, decltype(&JS_FreeContext)> context(JS_NewContext(runtime.get()), JS_FreeContext);
    if (!context) throw std::runtime_error("Cannot allocate JSON context");
    const bool bom = size >= 3 && static_cast<unsigned char>(buffer[0]) == 0xef &&
        static_cast<unsigned char>(buffer[1]) == 0xbb && static_cast<unsigned char>(buffer[2]) == 0xbf;
    const int offset = bom ? 3 : 0;
    const auto object = JS_ParseJSON(context.get(), buffer.data() + offset, static_cast<size_t>(size - offset), "LineCalc.json");
    WindowState state;
    bool valid = JS_IsObject(object) && !JS_IsArray(object);
    const std::array<const char*, 4> names{"x", "y", "width", "height"};
    const std::array<int*, 4> values{&state.x, &state.y, &state.width, &state.height};
    for (size_t i = 0; valid && i < names.size(); ++i)
    {
        const auto property = JS_GetPropertyStr(context.get(), object, names[i]);
        double value = 0;
        valid = JS_IsNumber(property) && JS_ToFloat64(context.get(), &value, property) == 0 &&
            std::isfinite(value) && std::floor(value) == value && value >= -1000000 && value <= 1000000;
        JS_FreeValue(context.get(), property);
        if (valid) *values[i] = static_cast<int>(value);
    }
    JS_FreeValue(context.get(), object);
    if (!valid || state.width <= 0 || state.height <= 0) throw std::runtime_error("Invalid window settings JSON");
    return state;
}

inline void saveWindowState(const std::filesystem::path& path, const WindowState& state)
{
    std::filesystem::create_directories(path.parent_path());
    auto temporary = path;
    temporary += L"." + std::to_wstring(GetCurrentProcessId()) + L".tmp";
    try
    {
        std::ofstream output(temporary, std::ios::binary | std::ios::trunc);
        if (!output) throw std::runtime_error("Cannot open temporary window settings");
        output << "{\n  \"x\": " << state.x << ",\n  \"y\": " << state.y <<
            ",\n  \"width\": " << state.width << ",\n  \"height\": " << state.height << "\n}\n";
        output.close();
        if (!output) throw std::runtime_error("Cannot write window settings");
        if (!MoveFileExW(temporary.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
            throw std::system_error(static_cast<int>(GetLastError()), std::system_category(), "Cannot replace window settings");
    }
    catch (...)
    {
        std::error_code error;
        std::filesystem::remove(temporary, error);
        throw;
    }
}
