#pragma once
#include "framework.h"
#include <cmath>
#include <memory>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>
#include <type_traits>

struct DisplayFontSettings
{
    std::wstring name = L"Segoe UI";
    double size = 12;
};

inline std::vector<std::wstring> installedFontNames()
{
    struct Enumeration
    {
        std::set<std::wstring> names;
        bool failed = false;
        static int CALLBACK collect(const LOGFONTW* font, const TEXTMETRICW*, DWORD, LPARAM data)
        {
            auto& result = *reinterpret_cast<Enumeration*>(data);
            try
            {
                if (font->lfFaceName[0] && font->lfFaceName[0] != L'@') result.names.insert(font->lfFaceName);
                return 1;
            }
            catch (...)
            {
                result.failed = true;
                return 0;
            }
        }
    } enumeration;
    const auto raw = CreateCompatibleDC(nullptr);
    if (!raw) throw std::runtime_error("Cannot enumerate installed fonts");
    std::unique_ptr<std::remove_pointer_t<HDC>, decltype(&DeleteDC)> dc(raw, DeleteDC);
    LOGFONTW query{};
    query.lfCharSet = DEFAULT_CHARSET;
    EnumFontFamiliesExW(raw, &query, Enumeration::collect, reinterpret_cast<LPARAM>(&enumeration), 0);
    if (enumeration.failed || enumeration.names.empty()) throw std::runtime_error("Cannot enumerate installed fonts");
    return {enumeration.names.begin(), enumeration.names.end()};
}

inline DisplayFontSettings validateDisplayFont(const std::wstring& name, double size)
{
    if (name.empty() || name.size() >= LF_FACESIZE || name.find(L'\0') != std::wstring::npos)
        throw std::invalid_argument("Invalid display font name");
    if (!std::isfinite(size) || size < 1 || size > 200)
        throw std::invalid_argument("Display font size must be between 1 and 200 points");
    for (const auto& candidate : installedFontNames())
    {
        if (CompareStringOrdinal(name.c_str(), -1, candidate.c_str(), -1, TRUE) == CSTR_EQUAL)
            return {candidate, size};
    }
    throw std::invalid_argument("Display font is not installed");
}
