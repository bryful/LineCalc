#include "framework.h"
#include "Calculator.h"
#include <cmath>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <utility>
#include <cerrno>
#include <system_error>
#include <charconv>
#include <string_view>

namespace
{
    enum NativeOperation { RESET, TOPMOST, CLEAR_HISTORY, HELP, VARIABLES, FUNCTIONS, MEMORY, CLEAR_MEMORY, IS_TOPMOST, HEX, BINARY, FONTS, SET_FONT, GET_FONT, CHOOSE_FONT, ANSWER };
    constexpr const char* NATIVE_NAMES[] = { "resetJS", "setTopmost", "clearHis", "help", "vars", "funcs", "mem", "clearMem", "isTopmost", "hex", "bin", "fonts", "setDisplayFont", "getDisplayFont", "chooseDisplayFont" };
    constexpr uint64_t MAX_SAFE_INTEGER = 9007199254740991ULL;
    const std::wstring HELP_TEXT =
        L"Enter: 評価 / ↑↓: 入力履歴 / Ctrl+C・X・V: コピー・切り取り・貼り付け\r\n"
        L"F1〜F12: メモリ挿入 / Shift+F1〜F12: 評価してメモリ保存\r\n\r\n"
        L"Math.sin(x), Math.cos(x), Math.sqrt(x), Math.pow(x,y), Math.PI（角度はラジアン）\r\n"
        L"変数: a=12 / var a / let a（未初期化宣言は0）\r\n"
        L"複数文: a=0; for(var i=0;i<10;i++) a+=i; a;\r\n"
        L"関数: function foo(v){return v*3;} / foo(5)\r\n"
        L"ans: 直前の数値結果（初期値null）\r\n"
        L"M1〜M12: メモリ（初期値null）/ M1=null: 消去\r\n\r\n"
        L"resetJS(): 現在の実行を終了し、JS環境・メモリ・ansを初期化して定義ファイルを再読み込み。定義ファイル内は禁止。\r\n"
        L"setTopmost(bool=false): 最前面設定。起動時true。\r\n"
        L"isTopmost(): 最前面設定を返す。\r\n"
        L"clearHis(): 履歴を消去。入力欄は維持。\r\n"
        L"clearMem(): 全メモリをnullにする。\r\n"
        L"hex(value): 安全な整数を0x付きの16進数文字列にする。例: hex(255) → 0xff\r\n"
        L"bin(value): 安全な整数を0b付きの2進数文字列にする。例: bin(10) → 0b1010\r\n"
        L"hex/binの結果は再評価・ans・メモリ保存に対応。負数は-0xffの形式。小数や安全範囲外はエラー。\r\n"
        L"fonts(): インストール済みフォント名の一覧（選択・コピー可能）。\r\n"
        L"setDisplayFont(name,size): フォント名とポイント数（1〜200）を設定。定義ファイルでも使用可能。\r\n"
        L"getDisplayFont(): 現在の設定を定義ファイルに貼り付けられる形で表示。\r\n"
        L"chooseDisplayFont(): フォント選択画面。定義ファイル内では使用不可。\r\n"
        L"help([name]): ヘルプ / vars(): 変数一覧 / funcs(): 関数一覧 / mem(): メモリ一覧\r\n\r\n"
        L"数値代入だけで終わり末尾に ; がある場合は空欄。宣言・関数定義も空欄。\r\n"
        L"履歴・変数・メモリは終了時に消えます。実行時間上限は2秒です。";

    std::optional<double> radixNumber(std::string_view text)
    {
        const bool negative = text.starts_with('-');
        if (negative) text.remove_prefix(1);
        if (text.size() < 3 || text[0] != '0') return {};
        const int base = text[1] == 'x' || text[1] == 'X' ? 16 : text[1] == 'b' || text[1] == 'B' ? 2 : 0;
        if (!base) return {};
        text.remove_prefix(2);
        uint64_t number = 0;
        const auto parsed = std::from_chars(text.data(), text.data() + text.size(), number, base);
        if (parsed.ec != std::errc{} || parsed.ptr != text.data() + text.size() || number > MAX_SAFE_INTEGER) return {};
        return negative ? -static_cast<double>(number) : static_cast<double>(number);
    }

    std::string resourceText(int id)
    {
        const auto module = GetModuleHandleW(nullptr);
        const auto resource = FindResourceW(module, MAKEINTRESOURCEW(id), RT_RCDATA);
        if (!resource) throw std::runtime_error("JavaScript resource is missing");
        const auto data = LoadResource(module, resource);
        const auto bytes = static_cast<const char*>(LockResource(data));
        if (!bytes) throw std::runtime_error("JavaScript resource cannot be loaded");
        return std::string(bytes, SizeofResource(module, resource));
    }
}

Calculator::Calculator(CalculatorHost callbacks) : host(std::move(callbacks)) {}
Calculator::~Calculator() { destroy(); }

std::string Calculator::toUtf8(const std::wstring& value)
{
    if (value.empty()) return {};
    const int count = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, value.data(), static_cast<int>(value.size()), nullptr, 0, nullptr, nullptr);
    if (!count) throw std::runtime_error("Invalid Unicode text");
    std::string result(count, '\0');
    WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, value.data(), static_cast<int>(value.size()), result.data(), count, nullptr, nullptr);
    return result;
}

std::wstring Calculator::fromUtf8(const std::string& value)
{
    if (value.empty()) return {};
    const int count = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, value.data(), static_cast<int>(value.size()), nullptr, 0);
    if (!count) throw std::runtime_error("Invalid UTF-8 text");
    std::wstring result(count, L'\0');
    MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, value.data(), static_cast<int>(value.size()), result.data(), count);
    return result;
}

void Calculator::destroy()
{
    if (context) JS_FreeContext(context);
    if (runtime) JS_FreeRuntime(runtime);
    context = nullptr;
    runtime = nullptr;
}

void Calculator::initialize()
{
    destroy();
    names.clear();
    answer.reset();
    for (auto& memory : memories) memory.reset();
    resetRequested = false;
    runtime = JS_NewRuntime();
    if (!runtime) throw std::runtime_error("Cannot allocate JavaScript runtime");
    JS_SetMemoryLimit(runtime, 64 * 1024 * 1024);
    JS_SetMaxStackSize(runtime, 2 * 1024 * 1024);
    JS_SetInterruptHandler(runtime, interrupt, this);
    context = JS_NewContext(runtime);
    if (!context) throw std::runtime_error("Cannot allocate JavaScript context");
    JS_SetContextOpaque(context, this);
    auto global = JS_GetGlobalObject(context);
    for (int i = 0; i < static_cast<int>(std::size(NATIVE_NAMES)); ++i)
    {
        JS_DefinePropertyValueStr(context, global, NATIVE_NAMES[i],
            JS_NewCFunctionMagic(context, nativeCall, NATIVE_NAMES[i], 1, JS_CFUNC_generic_magic, i), JS_PROP_ENUMERABLE);
    }
    for (int i = 0; i < 12; ++i)
    {
        const auto name = "M" + std::to_string(i + 1);
        auto atom = JS_NewAtom(context, name.c_str());
        JS_DefinePropertyGetSet(context, global, atom,
            JS_NewCFunctionMagic(context, memoryGet, name.c_str(), 0, JS_CFUNC_generic_magic, i),
            JS_NewCFunctionMagic(context, memorySet, name.c_str(), 1, JS_CFUNC_generic_magic, i), JS_PROP_ENUMERABLE);
        JS_FreeAtom(context, atom);
    }
    auto atom = JS_NewAtom(context, "ans");
    JS_DefinePropertyGetSet(context, global, atom,
        JS_NewCFunctionMagic(context, nativeCall, "ans", 0, JS_CFUNC_generic_magic, ANSWER), JS_UNDEFINED, JS_PROP_ENUMERABLE);
    JS_FreeAtom(context, atom);
    JS_FreeValue(context, global);
    for (const int id : {201, 202})
    {
        auto value = run(resourceText(id), id == 201 ? "acorn.js" : "runtime.js");
        if (JS_IsException(value)) throw std::runtime_error(toUtf8(exceptionText()));
        JS_FreeValue(context, value);
    }
}

int Calculator::interrupt(JSRuntime*, void* opaque)
{
    auto& self = *static_cast<Calculator*>(opaque);
    if (self.resetRequested) return 1;
    if (std::chrono::steady_clock::now() > self.deadline)
    {
        self.interrupted = true;
        return 1;
    }
    return 0;
}

JSValue Calculator::run(const std::string& code, const char* filename)
{
    deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
    interrupted = false;
    return JS_Eval(context, code.data(), code.size(), filename, JS_EVAL_TYPE_GLOBAL);
}

std::wstring Calculator::valueText(JSValueConst value)
{
    const char* text = JS_ToCString(context, value);
    if (!text) return L"<表示できません>";
    auto result = fromUtf8(text);
    JS_FreeCString(context, text);
    return result;
}

std::wstring Calculator::exceptionText()
{
    const auto exception = JS_GetException(context);
    auto result = valueText(exception);
    const auto stack = JS_GetPropertyStr(context, exception, "stack");
    if (JS_IsString(stack)) result += L"\r\n" + valueText(stack);
    JS_FreeValue(context, stack);
    JS_FreeValue(context, exception);
    if (interrupted) result = L"実行時間が2秒を超えました。\r\n" + result;
    return result;
}

Evaluation Calculator::loadDefinitions(const std::filesystem::path& path)
{
    definitionPath = path;
    loading = true;
    Evaluation result;
    try
    {
        std::ifstream file(path, std::ios::binary);
        if (file)
        {
            std::string source(std::istreambuf_iterator<char>(file), {});
            if (source.starts_with("\xEF\xBB\xBF")) source.erase(0, 3);
            if (file.bad()) throw std::runtime_error("Cannot read definition file");
            result = evaluate(fromUtf8(source));
        }
        else result.error = L"ファイルを開けません。\r\n" + fromUtf8(std::error_code(errno, std::generic_category()).message());
    }
    catch (const std::exception& error)
    {
        // 指定どおり、ユーザー定義ファイルの読み込み失敗は画面に表示しない。
        result.error = fromUtf8(error.what());
    }
    loading = false;
    if (!result.success) result.error = path.wstring() + L"\r\n" + result.error;
    return result;
}

Evaluation Calculator::evaluate(const std::wstring& source)
{
    Evaluation result;
    try
    {
        const auto utf8 = toUtf8(source);
        auto global = JS_GetGlobalObject(context);
        auto prepare = JS_GetPropertyStr(context, global, "__lcPrepare");
        auto argument = JS_NewStringLen(context, utf8.data(), utf8.size());
        deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
        interrupted = false;
        auto prepared = JS_Call(context, prepare, JS_UNDEFINED, 1, &argument);
        JS_FreeValue(context, argument);
        JS_FreeValue(context, prepare);
        JS_FreeValue(context, global);
        if (JS_IsException(prepared))
        {
            result.error = exceptionText();
            return result;
        }
        auto codeValue = JS_GetPropertyStr(context, prepared, "code");
        auto namesValue = JS_GetPropertyStr(context, prepared, "names");
        auto suppressValue = JS_GetPropertyStr(context, prepared, "suppress");
        auto formatter = JS_GetPropertyStr(context, prepared, "formatError");
        const auto code = toUtf8(valueText(codeValue));
        const bool suppress = JS_ToBool(context, suppressValue) != 0;
        auto lengthValue = JS_GetPropertyStr(context, namesValue, "length");
        uint32_t length = 0;
        JS_ToUint32(context, &length, lengthValue);
        for (uint32_t i = 0; i < length; ++i)
        {
            auto name = JS_GetPropertyUint32(context, namesValue, i);
            names.insert(toUtf8(valueText(name)));
            JS_FreeValue(context, name);
        }
        JS_FreeValue(context, lengthValue);
        JS_FreeValue(context, suppressValue);
        JS_FreeValue(context, namesValue);
        JS_FreeValue(context, codeValue);
        JS_FreeValue(context, prepared);
        const auto filename = loading ? toUtf8(definitionPath.wstring()) : std::string("<input>");
        auto value = run(code, filename.c_str());
        if (resetRequested)
        {
            JS_FreeValue(context, formatter);
            if (JS_IsException(value)) exceptionText();
            else JS_FreeValue(context, value);
            const auto path = definitionPath;
            initialize();
            loadDefinitions(path);
            result.success = true;
            result.reset = true;
            return result;
        }
        if (JS_IsException(value))
        {
            result.error = exceptionText();
            auto errorArgument = JS_NewString(context, toUtf8(result.error).c_str());
            auto filenameArgument = JS_NewString(context, filename.c_str());
            JSValue arguments[] = { errorArgument, filenameArgument };
            deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
            auto formatted = JS_Call(context, formatter, JS_UNDEFINED, 2, arguments);
            if (!JS_IsException(formatted)) result.error = valueText(formatted);
            else exceptionText();
            JS_FreeValue(context, errorArgument);
            JS_FreeValue(context, filenameArgument);
            JS_FreeValue(context, formatted);
            JS_FreeValue(context, formatter);
            return result;
        }
        JS_FreeValue(context, formatter);
        result.success = true;
        if (!loading && !suppress && !JS_IsUndefined(value))
        {
            if (JS_IsNumber(value))
            {
                double number = 0;
                JS_ToFloat64(context, &number, value);
                if (!std::isfinite(number))
                {
                    result.success = false;
                    result.error = L"評価結果が有限の数値ではありません。";
                }
                else
                {
                    result.number = number;
                    answer = number;
                }
            }
            else if (JS_IsString(value))
            {
                result.number = radixNumber(toUtf8(valueText(value)));
                if (result.number) answer = *result.number;
                else
                {
                    result.success = false;
                    result.error = L"文字列の結果は、安全な整数を表す0x/0b形式である必要があります。";
                }
            }
            else if (!JS_IsNull(value) && !JS_IsBool(value))
            {
                result.success = false;
                result.error = L"評価結果は数値・null・真偽値である必要があります。";
            }
            if (result.success) result.text = valueText(value);
        }
        JS_FreeValue(context, value);
    }
    catch (const std::exception& error)
    {
        result.success = false;
        result.error = fromUtf8(error.what());
    }
    return result;
}

void Calculator::storeMemory(int index, double value)
{
    if (index >= 0 && index < 12 && std::isfinite(value)) memories[index] = value;
}

bool Calculator::memoryEmpty(int index) const { return !memories[index].has_value(); }

std::wstring Calculator::memoryText(int index)
{
    if (!memories[index]) return L"null";
    auto value = JS_NewFloat64(context, *memories[index]);
    auto text = valueText(value);
    JS_FreeValue(context, value);
    if (std::signbit(*memories[index])) text = L"(" + text + L")";
    return text;
}

JSValue Calculator::memoryGet(JSContext* context, JSValueConst, int, JSValueConst*, int index)
{
    const auto& self = *static_cast<Calculator*>(JS_GetContextOpaque(context));
    return self.memories[index] ? JS_NewFloat64(context, *self.memories[index]) : JS_NULL;
}

JSValue Calculator::memorySet(JSContext* context, JSValueConst, int count, JSValueConst* arguments, int index)
{
    auto& self = *static_cast<Calculator*>(JS_GetContextOpaque(context));
    if (count && JS_IsNull(arguments[0])) self.memories[index].reset();
    else
    {
        double value = 0;
        if (!count || !JS_IsNumber(arguments[0]) || JS_ToFloat64(context, &value, arguments[0]) < 0 || !std::isfinite(value))
            return JS_ThrowTypeError(context, "メモリには有限の数値またはnullを指定してください。");
        self.memories[index] = value;
    }
    return JS_UNDEFINED;
}

std::wstring Calculator::listBindings(bool functions)
{
    const auto savedDeadline = deadline;
    std::wstring text;
    for (const auto& name : names)
    {
        if (name.starts_with("__lc") || name == "ans") continue;
        auto value = run(name, "<inspect>");
        if (JS_IsException(value)) { exceptionText(); continue; }
        if (JS_IsFunction(context, value) == functions)
            text += fromUtf8(name) + (functions ? L"()" : L" = " + valueText(value)) + L"\r\n";
        JS_FreeValue(context, value);
    }
    deadline = savedDeadline;
    return text.empty() ? L"定義されていません。" : text;
}

JSValue Calculator::nativeCall(JSContext* context, JSValueConst, int count, JSValueConst* arguments, int operation)
{
    auto& self = *static_cast<Calculator*>(JS_GetContextOpaque(context));
    const auto callbackStart = std::chrono::steady_clock::now();
    if (self.resetRequested) return JS_ThrowInternalError(context, "resetJS");
    try
    {
        switch (operation)
        {
        case RESET:
            if (self.loading) return JS_ThrowTypeError(context, "定義ファイルではresetJS()を実行できません。");
            self.resetRequested = true;
            return JS_ThrowInternalError(context, "resetJS");
        case TOPMOST:
            if (count && !JS_IsBool(arguments[0])) return JS_ThrowTypeError(context, "setTopmostには真偽値を指定してください。");
            if (self.host.setTopmost) self.host.setTopmost(count && JS_ToBool(context, arguments[0]));
            break;
        case IS_TOPMOST:
            return JS_NewBool(context, self.host.isTopmost && self.host.isTopmost());
        case HEX:
        case BINARY:
        {
            double number = 0;
            if (count != 1 || !JS_IsNumber(arguments[0]) || JS_ToFloat64(context, &number, arguments[0]) < 0 ||
                !std::isfinite(number) || std::trunc(number) != number || std::abs(number) > static_cast<double>(MAX_SAFE_INTEGER))
                return JS_ThrowTypeError(context, "hex/binには安全な整数を1つ指定してください（±9007199254740991以内）。");
            char digits[65]{};
            const auto converted = std::to_chars(digits, digits + sizeof(digits), static_cast<uint64_t>(std::abs(number)), operation == HEX ? 16 : 2);
            if (converted.ec != std::errc{}) return JS_ThrowInternalError(context, "数値の基数変換に失敗しました。");
            std::string text = number < 0 ? "-" : "";
            text += operation == HEX ? "0x" : "0b";
            text.append(digits, converted.ptr);
            return JS_NewStringLen(context, text.data(), text.size());
        }
        case ANSWER:
            return self.answer ? JS_NewFloat64(context, *self.answer) : JS_NULL;
        case FONTS:
            if (self.host.showText && !self.loading)
            {
                std::wstring text;
                for (const auto& name : installedFontNames()) text += name + L"\r\n";
                self.host.showText(L"インストール済みフォント", text);
            }
            break;
        case SET_FONT:
        {
            double size = 0;
            if (count != 2 || !JS_IsString(arguments[0]) || !JS_IsNumber(arguments[1]) || JS_ToFloat64(context, &size, arguments[1]) < 0)
                return JS_ThrowTypeError(context, "setDisplayFontにはフォント名とポイント数を指定してください。");
            const auto settings = validateDisplayFont(self.valueText(arguments[0]), size);
            if (self.host.setDisplayFont) self.host.setDisplayFont(settings);
            self.displayFont = settings;
            break;
        }
        case GET_FONT:
            if (self.host.showText && !self.loading)
            {
                const auto settings = self.host.getDisplayFont ? self.host.getDisplayFont() : self.displayFont;
                auto name = JS_NewString(context, toUtf8(settings.name).c_str());
                auto quoted = JS_JSONStringify(context, name, JS_UNDEFINED, JS_UNDEFINED);
                auto size = JS_NewFloat64(context, settings.size);
                const auto text = L"setDisplayFont(" + self.valueText(quoted) + L", " + self.valueText(size) + L");";
                JS_FreeValue(context, name);
                JS_FreeValue(context, quoted);
                JS_FreeValue(context, size);
                self.host.showText(L"表示フォント設定", text);
            }
            break;
        case CHOOSE_FONT:
            if (self.loading) return JS_ThrowTypeError(context, "定義ファイルではchooseDisplayFont()を実行できません。");
            if (self.host.chooseDisplayFont) self.host.chooseDisplayFont();
            break;
        case CLEAR_HISTORY:
            if (self.host.clearHistory) self.host.clearHistory();
            break;
        case CLEAR_MEMORY:
            for (auto& memory : self.memories) memory.reset();
            break;
        case HELP:
            if (self.host.showText && !self.loading)
            {
                std::wstring text = HELP_TEXT;
                if (count)
                {
                    const auto name = self.valueText(arguments[0]);
                    text.clear();
                    size_t begin = 0;
                    while (begin < HELP_TEXT.size())
                    {
                        const auto end = HELP_TEXT.find(L"\r\n", begin);
                        const auto line = HELP_TEXT.substr(begin, end == std::wstring::npos ? end : end - begin);
                        if (line.find(name) != std::wstring::npos) text += line + L"\r\n";
                        if (end == std::wstring::npos) break;
                        begin = end + 2;
                    }
                    if (text.empty()) text = L"該当する説明がありません。";
                }
                self.host.showText(L"LineCalc ヘルプ", text);
            }
            break;
        case VARIABLES:
        case FUNCTIONS:
            if (self.host.showText && !self.loading)
                self.host.showText(operation == VARIABLES ? L"変数一覧" : L"関数一覧", self.listBindings(operation == FUNCTIONS));
            break;
        case MEMORY:
            if (self.host.showText && !self.loading)
            {
                std::wstring text;
                for (int i = 0; i < 12; ++i) text += L"M" + std::to_wstring(i + 1) + L" = " + self.memoryText(i) + L"\r\n";
                self.host.showText(L"メモリ一覧", text);
            }
            break;
        }
    }
    catch (const std::exception& error)
    {
        return JS_ThrowInternalError(context, "%s", error.what());
    }
    self.deadline += std::chrono::steady_clock::now() - callbackStart;
    return JS_UNDEFINED;
}
