#pragma once

#include <chrono>
#include <filesystem>
#include <functional>
#include <optional>
#include <set>
#include <string>
#include "../vendor/quickjs/quickjs.h"
#include "DisplayFont.h"

struct Evaluation
{
    bool success = false;
    bool reset = false;
    std::wstring text;
    std::optional<double> number;
    std::wstring error;
};

struct CalculatorHost
{
    std::function<void(bool)> setTopmost;
    std::function<bool()> isTopmost;
    std::function<void()> clearHistory;
    std::function<void(const std::wstring&, const std::wstring&)> showText;
    std::function<void(const DisplayFontSettings&)> setDisplayFont;
    std::function<DisplayFontSettings()> getDisplayFont;
    std::function<void()> chooseDisplayFont;
};

class Calculator
{
public:
    explicit Calculator(CalculatorHost host = {});
    ~Calculator();
    Calculator(const Calculator&) = delete;
    Calculator& operator=(const Calculator&) = delete;
    void initialize();
    Evaluation loadDefinitions(const std::filesystem::path& path);
    Evaluation evaluate(const std::wstring& source);
    void storeMemory(int index, double value);
    std::wstring memoryText(int index);
    bool memoryEmpty(int index) const;
    static std::string toUtf8(const std::wstring& value);
    static std::wstring fromUtf8(const std::string& value);

private:
    CalculatorHost host;
    JSRuntime* runtime = nullptr;
    JSContext* context = nullptr;
    std::optional<double> memories[12];
    std::optional<double> answer;
    DisplayFontSettings displayFont;
    std::set<std::string> names;
    std::filesystem::path definitionPath;
    bool loading = false;
    bool resetRequested = false;
    bool interrupted = false;
    std::chrono::steady_clock::time_point deadline;
    JSValue run(const std::string& code, const char* filename);
    std::wstring exceptionText();
    std::wstring valueText(JSValueConst value);
    std::wstring listBindings(bool functions);
    void destroy();
    static int interrupt(JSRuntime*, void* opaque);
    static JSValue nativeCall(JSContext*, JSValueConst, int, JSValueConst*, int);
    static JSValue memoryGet(JSContext*, JSValueConst, int, JSValueConst*, int);
    static JSValue memorySet(JSContext*, JSValueConst, int, JSValueConst*, int);
};
