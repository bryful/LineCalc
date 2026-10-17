#include "../LineCalc/Calculator.h"
#include "../LineCalc/History.h"
#include "../LineCalc/LaunchOptions.h"
#include "../LineCalc/DefinitionTemplate.h"
#include "../LineCalc/PopupPlacement.h"
#include "../LineCalc/WindowState.h"
#include <cmath>
#include <fstream>
#include <iostream>
#include <stdexcept>

namespace
{
    int checks = 0;
    void check(bool condition, const char* message)
    {
        ++checks;
        if (!condition) throw std::runtime_error(message);
    }
}

int main()
{
    try
    {
        bool topmost = true;
        bool cleared = false;
        std::wstring listing;
        DisplayFontSettings selectedFont;
        bool chooseCalled = false;
        bool failFontChange = false;
        CalculatorHost host;
        host.setTopmost = [&](bool value) { topmost = value; };
        host.isTopmost = [&] { return topmost; };
        host.clearHistory = [&] { cleared = true; };
        host.showText = [&](const auto&, const auto& text) { listing = text; };
        host.setDisplayFont = [&](const auto& settings)
        {
            if (failFontChange) throw std::runtime_error("Test font creation failure");
            selectedFont = settings;
        };
        host.getDisplayFont = [&] { return selectedFont; };
        host.chooseDisplayFont = [&] { chooseCalled = true; };
        Calculator calculator(host);
        calculator.initialize();
        auto value = [&](const wchar_t* source, const wchar_t* expected)
        {
            const auto result = calculator.evaluate(source);
            if (!result.success || result.text != expected)
            {
                std::cerr << "Input: " << Calculator::toUtf8(source) << " Expected: " << Calculator::toUtf8(expected)
                    << " Actual: " << Calculator::toUtf8(result.text) << " Error: " << Calculator::toUtf8(result.error) << '\n';
                throw std::runtime_error("Unexpected evaluation result");
            }
            ++checks;
        };
        auto error = [&](const wchar_t* source) { check(!calculator.evaluate(source).success, "Expected evaluation failure"); };
        value(L"1+2*3", L"7");
        value(L"(15-2)*12", L"156");
        value(L"Math.sin(Math.PI/2)", L"1");
        value(L"Math.cos(0)", L"1");
        value(L"0x001+0b00000001_11000001", L"450");
        value(L"foo=(15-2)*12", L"156");
        value(L"foo", L"156");
        value(L"var uninitialized", L"");
        value(L"uninitialized", L"0");
        value(L"let lexical", L"");
        value(L"lexical", L"0");
        value(L"a=1;", L"");
        value(L"a=1;b=12;a+b;", L"13");
        value(L"a=0;for(var i=0;i<10;i++) a+i;a;", L"0");
        value(L"a=0;for(var i=0;i<10;i++) a+=i;a;", L"45");
        value(L"a=1; a += (2); a", L"3");
        value(L"a=1; a /* += */ += (a=2); a", L"3");
        value(L"a=0; a+=true; a", L"1");
        error(L"a=1e308; a*=2;");
        value(L"a", L"1e+308");
        value(L"const fixed=2; fixed ||= 3; fixed", L"2");
        value(L"a=0;while(a<4){a++;}a", L"4");
        value(L"if(a===4){a=10;}a", L"10");
        value(L"function triple(v){return v*3;}", L"");
        value(L"triple(7)", L"21");
        value(L"function factorial(v){if(v<=1)return 1;return v*factorial(v-1);}", L"");
        value(L"factorial(6)", L"720");
        value(L"function locals(v){let x;x=v+2;return x;}", L"");
        value(L"locals(5)", L"7");
        value(L"ans/7", L"1");
        error(L"1/0");
        error(L"Math.sqrt(-1)");
        value(L"ans", L"1");
        error(L"1+");
        error(L"unknownName");
        error(L"bad='text'");
        error(L"var bad=Infinity");
        error(L"var M1=0");
        error(L"let M12=0");
        error(L"function M1(){return 0;}");
        error(L"delete M1");
        error(L"ans=4");
        error(L"help=4");
        value(L"M1", L"null");
        value(L"M1+1", L"1");
        value(L"M1=0", L"0");
        check(!calculator.memoryEmpty(0), "Zero memory must not be empty");
        value(L"M1=null", L"null");
        check(calculator.memoryEmpty(0), "Null memory must be empty");
        calculator.storeMemory(11, -3);
        check(calculator.memoryText(11) == L"(-3)", "Negative insertion needs parentheses");
        value(L"2*M12", L"-6");
        error(L"M1=Infinity");
        value(L"mem()", L"");
        check(listing.find(L"M12 = (-3)") != std::wstring::npos, "Memory listing");
        value(L"clearMem()", L"");
        value(L"M12", L"null");
        value(L"vars()", L"");
        check(listing.find(L"lexical = 0") != std::wstring::npos, "Lexical variable listing");
        value(L"funcs()", L"");
        check(listing.find(L"factorial()") != std::wstring::npos, "Function listing");
        value(L"help('resetJS')", L"");
        check(listing.find(L"resetJS") != std::wstring::npos, "Help lookup");
        value(L"hex(255)", L"0xff");
        value(L"ans", L"255");
        value(L"bin(10)", L"0b1010");
        value(L"ans", L"10");
        value(L"hex(0)", L"0x0");
        value(L"bin(-0)", L"0b0");
        value(L"hex(-255)", L"-0xff");
        value(L"bin(-10)", L"-0b1010");
        value(L"hex(9007199254740991)", L"0x1fffffffffffff");
        value(L"bin(9007199254740991)", L"0b11111111111111111111111111111111111111111111111111111");
        value(L"hex(-9007199254740991)", L"-0x1fffffffffffff");
        const auto formattedResult = calculator.evaluate(L"hex(-255)");
        check(formattedResult.success && formattedResult.number == -255, "Formatted result retains memory value");
        const auto reEvaluated = calculator.evaluate(formattedResult.text);
        check(reEvaluated.success && reEvaluated.number == -255, "Negative radix display can be re-evaluated");
        calculator.storeMemory(2, *formattedResult.number);
        value(L"M3", L"-255");
        value(L"typeof hex(255) === 'string'", L"true");
        value(L"Number(hex(255))+1", L"256");
        value(L"function formatted(v){return hex(v);} formatted(42)", L"0x2a");
        value(L"'0xff'", L"0xff");
        for (const wchar_t* invalid : {L"hex()", L"bin()", L"hex(1,2)", L"hex(1.5)", L"bin(-0.5)",
            L"hex(Infinity)", L"bin(NaN)", L"hex('255')", L"bin(null)", L"hex(true)",
            L"hex(9007199254740992)", L"bin(-9007199254740992)", L"a=hex(255)", L"let text=bin(10)",
            L"'hello'", L"'123'", L"'0x'", L"'0b102'", L"'0xffoops'", L"'0x20000000000000'",
            L"var hex=0", L"bin=0", L"delete hex"}) error(invalid);
        value(L"ans", L"255");
        value(L"help('hex')", L"");
        check(listing.find(L"hex(value)") != std::wstring::npos, "Hex help");
        value(L"help('bin')", L"");
        check(listing.find(L"bin(value)") != std::wstring::npos, "Binary help");
        const auto availableFonts = installedFontNames();
        check(!availableFonts.empty(), "Installed font enumeration");
        value(L"fonts()", L"");
        check(listing.find(availableFonts.front()) != std::wstring::npos, "Installed font list display");
        check(listing.find(L"\r\n@") == std::wstring::npos, "Vertical font aliases excluded");
        value(L"setDisplayFont('Segoe UI', 18.5)", L"");
        check(selectedFont.name == L"Segoe UI" && selectedFont.size == 18.5, "Set font callback");
        value(L"getDisplayFont()", L"");
        check(listing == L"setDisplayFont(\"Segoe UI\", 18.5);", "Copyable font configuration");
        value(L"setDisplayFont('segoe ui', 12)", L"");
        check(selectedFont.name == L"Segoe UI", "Case insensitive font name lookup");
        for (const wchar_t* invalid : {L"setDisplayFont()", L"setDisplayFont('Segoe UI')", L"setDisplayFont(1,12)",
            L"setDisplayFont('Segoe UI','12')", L"setDisplayFont('Segoe UI',0)", L"setDisplayFont('Segoe UI',201)",
            L"setDisplayFont('Segoe UI',NaN)", L"setDisplayFont('Segoe UI',Infinity)", L"setDisplayFont('',12)",
            L"setDisplayFont('LineCalc-nonexistent-font',12)", L"var fonts=0", L"setDisplayFont=1", L"delete getDisplayFont"}) error(invalid);
        check(selectedFont.name == L"Segoe UI" && selectedFont.size == 12, "Invalid font settings preserve current selection");
        failFontChange = true;
        error(L"setDisplayFont('Segoe UI',20)");
        failFontChange = false;
        check(selectedFont.size == 12, "Failed host font change preserves current selection");
        value(L"chooseDisplayFont()", L"");
        check(chooseCalled, "Font picker callback");
        value(L"help('fonts')", L"");
        check(listing.find(L"fonts()") != std::wstring::npos, "Font list help");
        value(L"setTopmost()", L"");
        check(!topmost, "Topmost default false");
        value(L"setTopmost(true)", L"");
        value(L"isTopmost()", L"true");
        error(L"setTopmost(1)");
        value(L"clearHis()", L"");
        check(cleared, "History callback");
        error(L"a=9;missing()");
        value(L"a", L"9");
        error(L"while(true){}");
        value(L"2+2", L"4");

        const auto file = std::filesystem::temp_directory_path() / L"LineCalc-tests-定義.js";
        struct RemoveFile
        {
            std::filesystem::path path;
            ~RemoveFile() { std::error_code ignored; std::filesystem::remove(path, ignored); }
        } cleanup{file};
        { std::ofstream out(file); out << "function loaded(v){return v*4;} setTopmost(false); resetJS(); afterReset=1;"; }
        calculator.loadDefinitions(file);
        check(!topmost, "Definition file topmost");
        value(L"loaded(5)", L"20");
        error(L"afterReset");
        calculator.storeMemory(0, 12);
        const auto reset = calculator.evaluate(L"resetJS(); afterReset=2;");
        check(reset.success && reset.reset, "Reset must succeed");
        value(L"loaded(6)", L"24");
        value(L"M1", L"null");
        error(L"foo");
        error(L"afterReset");
        calculator.loadDefinitions(file.parent_path() / L"not-existing-linecalc.js");
        value(L"3+4", L"7");
        check(!calculator.loadDefinitions(file.parent_path() / L"not-existing-linecalc.js").success, "Missing debug file must fail");
        { std::ofstream out(file); out << "function valid(v){return v+1;}\nlet broken = ;\n"; }
        const auto syntaxFailure = calculator.loadDefinitions(file);
        check(!syntaxFailure.success && syntaxFailure.error.find(L"行 2、列 14") != std::wstring::npos, "Syntax error location");
        check(syntaxFailure.error.find(file.wstring()) != std::wstring::npos, "Error filename");
        { std::ofstream out(file); out << "function badCall(){\n return missingCall();\n}\nbadCall();\n"; }
        const auto runtimeFailure = calculator.loadDefinitions(file);
        check(!runtimeFailure.success && runtimeFailure.error.find(L":2:") != std::wstring::npos, "Runtime error location");
        { std::ofstream out(file); out << "a = missingCall();\n"; }
        const auto mappedFailure = calculator.loadDefinitions(file);
        check(!mappedFailure.success && mappedFailure.error.find(L":1:5") != std::wstring::npos, "Runtime error maps to original column");
        { std::ofstream out(file); out << "function valid(v){return v+1;}\n"; }
        check(calculator.loadDefinitions(file).success, "Valid debug file");
        { std::ofstream out(file); out << "setDisplayFont('Segoe UI',15);\nchooseDisplayFont();\n"; }
        chooseCalled = false;
        const auto forbiddenPicker = calculator.loadDefinitions(file);
        check(!forbiddenPicker.success && !chooseCalled, "Definition file cannot open font picker");
        check(selectedFont.size == 15, "Definition file sets display font before subsequent error");

        const auto folder = std::filesystem::temp_directory_path() / L"LineCalc-options-tests";
        const auto currentFolder = folder / L"current";
        const auto userFolder = folder / L"user";
        std::filesystem::create_directories(currentFolder);
        std::filesystem::create_directories(userFolder);
        const auto currentFile = currentFolder / L"functions.js";
        const auto userFile = userFolder / L"functions.js";
        const auto defaultFile = userFolder / L"LineCalc.js";
        { std::ofstream out(currentFile); out << "//current"; }
        { std::ofstream out(userFile); out << "//user"; }
        auto options = [&](std::vector<std::wstring> args) { return resolveLaunchOptions(args, currentFolder, userFolder, defaultFile); };
        check(options({L"-debug"}).debug && options({L"-debug"}).definitions == defaultFile, "Default debug path");
        check(options({L"-debug",L"functions.js"}).definitions == currentFile, "Debug current directory precedence");
        check(options({L"-include",L"functions.js"}).definitions == currentFile, "Include current directory precedence");
        std::filesystem::remove(currentFile);
        check(options({L"-debug",L"functions.js"}).definitions == userFile, "Debug user directory fallback");
        check(options({L"-include",L"functions.js"}).definitions == userFile, "Include user directory fallback");
        check(options({L"-debug",L"-include",L"functions.js"}).definitions == userFile, "Combined debug include");
        check(options({L"-include",L"functions.js",L"-debug"}).debug, "Reversed options");
        check(options({L"-include",L"missing.js"}).definitions == defaultFile, "Normal missing include fallback");
        check(options({L"-debug",L"missing.js"}).definitions == userFolder / L"missing.js", "Missing debug target remains an error");
        check(options({L"-debug",L"sub/functions.js"}).definitions == currentFolder / L"sub/functions.js", "Explicit parent prevents user fallback");
        check(options({L"-debug",file.wstring()}).definitions == file, "Absolute debug target");
        check(options({L"-debug",file.wstring(),L"-include",L"functions.js"}).definitions == file, "Explicit debug path precedence");
        check(options({L"-createjs"}).createJs && options({L"-createjs"}).definitions == defaultFile, "Default template path");
        check(options({L"-createjs",L"functions.js"}).definitions == userFile, "Bare template filename uses user directory");
        check(options({L"-createjs",L"./functions.js"}).definitions == currentFolder / L"./functions.js", "Dot template path uses current directory");
        check(options({L"-createjs",L"sub/functions.js"}).definitions == currentFolder / L"sub/functions.js", "Relative template parent");
        check(options({L"-createjs",file.wstring()}).definitions == file, "Absolute template path");
        bool conflictingOptions = false;
        try { options({L"-createjs",L"-debug"}); }
        catch (const std::exception&) { conflictingOptions = true; }
        check(conflictingOptions, "Conflicting modes must fail");
        const auto templateFolder = currentFolder / L"新規";
        const auto templateFile = templateFolder / L"関数.js";
        check(createDefinitionTemplate(templateFile).success, "Create UTF-8 template and parent directory");
        std::ifstream templateInput(templateFile, std::ios::binary);
        std::string templateText(std::istreambuf_iterator<char>(templateInput), {});
        templateInput.close();
        for (const char* name : {"resetJS", "setTopmost", "isTopmost", "clearHis", "clearMem", "help", "vars", "funcs", "mem", "hex", "bin",
            "fonts", "setDisplayFont", "getDisplayFont", "chooseDisplayFont"})
            check(templateText.find(name) != std::string::npos, "Template includes all native functions");
        size_t lineStart = 0;
        while (lineStart < templateText.size())
        {
            check(templateText.compare(lineStart,2,"//") == 0, "Template lines must be comments");
            const auto lineEnd = templateText.find('\n', lineStart);
            if (lineEnd == std::string::npos) break;
            lineStart = lineEnd+1;
        }
        check(calculator.loadDefinitions(templateFile).success, "Generated template evaluates without errors");
        check(!createDefinitionTemplate(userFile).success, "Existing definition must not be overwritten");
        std::ifstream existingInput(userFile, std::ios::binary);
        const std::string existingText(std::istreambuf_iterator<char>(existingInput), {});
        existingInput.close();
        check(existingText == "//user", "Existing definition content preserved");
        check(!createDefinitionTemplate(userFile / L"child.js").success, "Parent file cannot be used as directory");
        std::filesystem::remove(templateFile);
        std::filesystem::remove(templateFolder);
        std::filesystem::remove(userFile);
        std::filesystem::remove(currentFolder);
        std::filesystem::remove(userFolder);
        std::filesystem::remove(folder);

        History history;
        history.submit(L"1+1");
        history.submit(L"invalid+");
        check(history.move(L"draft", true, true) == L"invalid+", "Up saves draft");
        check(history.move(L"invalid+", true, true) == L"1+1", "Up recalls first");
        check(history.move(L"edited", false, true) == L"invalid+", "Down preserves edited history");
        check(history.move(L"invalid+", false, true) == L"draft", "Down restores draft");
        check(history.move(L"draft", false, true) == L"edited", "Edited history retained");
        history.clear();
        check(history.size() == 0, "Clear history");
        check(history.move(L"", true, false).empty(), "Empty history");
        const RECT work{0, 0, 1920, 1040};
        const RECT bottomRight{1700, 980, 1920, 1040};
        const auto notification = popupBounds(work, &bottomRight, 76, 30, true);
        check(notification.right == 1920 && notification.bottom == 980, "Bottom edge toast appears above parent");
        const auto dialog = popupBounds(work, &bottomRight, 600, 360);
        check(dialog.right <= work.right && dialog.bottom <= work.bottom && dialog.left >= work.left && dialog.top >= work.top, "Bottom right dialog stays inside work area");
        const RECT normalParent{100, 100, 500, 160};
        const auto below = popupBounds(work, &normalParent, 600, 360);
        check(below.left == 100 && below.top == 160, "Popup uses space below when available");
        const RECT negativeWork{-1600, -200, 0, 700};
        const RECT negativeParent{-1550, 650, -1010, 700};
        const auto negative = popupBounds(negativeWork, &negativeParent, 600, 360);
        check(negative.left == -1550 && negative.bottom == 650, "Secondary monitor with negative coordinates");
        const RECT smallWork{0, 0, 320, 200};
        const auto small = popupBounds(smallWork, &normalParent, 600, 360);
        check(small.left == 0 && small.top == 0 && small.right == 320 && small.bottom == 200, "Oversized dialog shrinks to fit work area");
        const auto centered = popupBounds(negativeWork, nullptr, 600, 360);
        check(centered.left == -1100 && centered.top == 70, "Ownerless dialog centers on target monitor");
        const RECT outside{2000, 1200, 2400, 1260};
        const auto clamped = popupBounds(work, &outside, 600, 360);
        check(clamped.right == work.right && clamped.bottom == work.bottom, "Offscreen parent cannot place popup offscreen");
        const auto fitted = fitWindowState({2000, 1200, 540, 80}, work);
        check(fitted.x == 1380 && fitted.y == 960, "Window restored inside work area");
        const auto fittedNegative = fitWindowState({-1800, -300, 540, 80}, negativeWork);
        check(fittedNegative.x == -1600 && fittedNegative.y == -200, "Window supports negative monitor coordinates");
        const auto fittedLarge = fitWindowState({0, 0, 1000, 400}, smallWork);
        check(fittedLarge.width == 320 && fittedLarge.height == 200, "Large window fits smaller screen");
        const auto statePath = std::filesystem::temp_directory_path() /
            (L"LineCalc-window-test-" + std::to_wstring(GetCurrentProcessId()) + L".json");
        check(!loadWindowState(statePath), "Missing window settings use defaults");
        saveWindowState(statePath, {-1200, 200, 720, 80});
        const auto loaded = loadWindowState(statePath);
        check(loaded && loaded->x == -1200 && loaded->y == 200 && loaded->width == 720 && loaded->height == 80,
            "Window settings JSON round trip");
        {
            std::ofstream invalid(statePath);
            invalid << "{\"x\":0,\"y\":0,\"width\":-1,\"height\":80}";
        }
        bool rejected = false;
        try { loadWindowState(statePath); }
        catch (const std::exception&) { rejected = true; }
        check(rejected, "Invalid window dimensions rejected");
        std::filesystem::remove(statePath);
        std::cout << checks << " checks passed\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << "FAILED: " << error.what() << '\n';
        return 1;
    }
}
