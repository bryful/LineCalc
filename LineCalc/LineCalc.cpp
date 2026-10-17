// LineCalc.cpp : アプリケーションのエントリ ポイントを定義します。
//
#include "framework.h"
#include "LineCalc.h"
#include "Calculator.h"
#include "History.h"
#include "LaunchOptions.h"
#include "DefinitionTemplate.h"
#include <commctrl.h>
#include <commdlg.h>
#include <dlgs.h>
#include <shellapi.h>
#include <shlobj.h>
#include <memory>
#include <stdexcept>
#include <algorithm>

#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "comdlg32.lib")

namespace
{
    constexpr UINT_PTR TOAST_TIMER = 1;
    constexpr DWORD MAIN_STYLE = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_THICKFRAME;

    std::wstring windowText(HWND window)
    {
        const int length = GetWindowTextLengthW(window);
        std::wstring result(static_cast<size_t>(length) + 1, L'\0');
        GetWindowTextW(window, result.data(), length + 1);
        result.resize(length);
        return result;
    }

    struct TextDialog
    {
        std::wstring text;
        HWND edit = nullptr;
        HWND button = nullptr;
        bool closed = false;

        static LRESULT CALLBACK procedure(HWND window, UINT message, WPARAM wParam, LPARAM lParam)
        {
            auto* self = reinterpret_cast<TextDialog*>(GetWindowLongPtrW(window, GWLP_USERDATA));
            if (message == WM_NCCREATE)
            {
                self = static_cast<TextDialog*>(reinterpret_cast<CREATESTRUCTW*>(lParam)->lpCreateParams);
                SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
            }
            if (!self) return DefWindowProcW(window, message, wParam, lParam);
            switch (message)
            {
            case WM_CREATE:
                self->edit = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", self->text.c_str(),
                    WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_VSCROLL | WS_HSCROLL | ES_MULTILINE | ES_READONLY | ES_AUTOHSCROLL | ES_AUTOVSCROLL,
                    12, 12, 500, 230, window, reinterpret_cast<HMENU>(101), nullptr, nullptr);
                self->button = CreateWindowExW(0, L"BUTTON", L"OK", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_DEFPUSHBUTTON,
                    420, 252, 92, 30, window, reinterpret_cast<HMENU>(IDOK), nullptr, nullptr);
                for (auto child : {self->edit, self->button})
                    SendMessageW(child, WM_SETFONT, reinterpret_cast<WPARAM>(GetStockObject(DEFAULT_GUI_FONT)), TRUE);
                SetFocus(self->button);
                return 0;
            case WM_SIZE:
            {
                const int width = LOWORD(lParam);
                const int height = HIWORD(lParam);
                MoveWindow(self->edit, 12, 12, width - 24, height - 66, TRUE);
                MoveWindow(self->button, width - 104, height - 42, 92, 30, TRUE);
                return 0;
            }
            case WM_COMMAND:
                if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL) DestroyWindow(window);
                return 0;
            case WM_CLOSE:
                DestroyWindow(window);
                return 0;
            case WM_DESTROY:
                self->closed = true;
                return 0;
            }
            return DefWindowProcW(window, message, wParam, lParam);
        }

        static void show(HWND owner, const std::wstring& title, const std::wstring& text)
        {
            TextDialog dialog{text};
            RECT parent{};
            if (owner) GetWindowRect(owner, &parent);
            else
            {
                RECT work{};
                SystemParametersInfoW(SPI_GETWORKAREA, 0, &work, 0);
                parent.left = work.left + (work.right - work.left - 600) / 2;
                parent.bottom = work.top + (work.bottom - work.top - 360) / 2;
            }
            HWND window = CreateWindowExW(WS_EX_DLGMODALFRAME, L"LineCalcText", title.c_str(),
                WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_THICKFRAME,
                parent.left, parent.bottom, 600, 360, owner, nullptr, GetModuleHandleW(nullptr), &dialog);
            if (!window)
            {
                MessageBoxW(owner, text.c_str(), title.c_str(), MB_OK | MB_ICONERROR);
                return;
            }
            EnableWindow(owner, FALSE);
            ShowWindow(window, SW_SHOW);
            MSG message{};
            while (!dialog.closed)
            {
                const auto status = GetMessageW(&message, nullptr, 0, 0);
                if (status <= 0)
                {
                    if (!status) PostQuitMessage(static_cast<int>(message.wParam));
                    break;
                }
                if (!IsDialogMessageW(window, &message))
                {
                    TranslateMessage(&message);
                    DispatchMessageW(&message);
                }
            }
            if (IsWindow(window)) DestroyWindow(window);
            EnableWindow(owner, TRUE);
            SetActiveWindow(owner);
        }
    };

    struct Application
    {
        HWND window = nullptr;
        HWND edit = nullptr;
        HWND toast = nullptr;
        HFONT font = nullptr;
        DisplayFontSettings displayFont;
        int inputHeight = 30;
        int outerHeight = 0;
        bool topmost = true;
        bool active = false;
        bool collapsed = false;
        bool showingResult = false;
        bool settingText = false;
        bool preserveInput = false;
        std::optional<double> displayedNumber;
        History history;
        std::unique_ptr<Calculator> calculator;
        std::filesystem::path definitions;

        ~Application() { if (font) DeleteObject(font); }
        void setText(const std::wstring& text, bool result = false, std::optional<double> number = {})
        {
            settingText = true;
            SetWindowTextW(edit, text.c_str());
            SendMessageW(edit, EM_SETSEL, text.size(), text.size());
            settingText = false;
            showingResult = result;
            displayedNumber = number;
        }
        void setTopmost(bool enabled)
        {
            topmost = enabled;
            SetWindowPos(window, enabled ? HWND_TOPMOST : HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
            updateCompactState();
        }
        int currentHeight() const
        {
            return collapsed ? outerHeight - inputHeight : outerHeight;
        }
        void resizeToCurrentHeight()
        {
            if (!window || !outerHeight) return;
            RECT rectangle{};
            GetWindowRect(window, &rectangle);
            SetWindowPos(window, nullptr, 0, 0, rectangle.right - rectangle.left, currentHeight(),
                SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
        }
        void updateCompactState()
        {
            const bool compact = topmost && !active;
            if (collapsed == compact) return;
            collapsed = compact;
            if (collapsed)
            {
                KillTimer(window, TOAST_TIMER);
                if (toast) DestroyWindow(toast);
                toast = nullptr;
            }
            if (edit) ShowWindow(edit, collapsed ? SW_HIDE : SW_SHOWNA);
            resizeToCurrentHeight();
        }
        void notifySuccess()
        {
            if (collapsed) return;
            if (toast) DestroyWindow(toast);
            RECT rectangle{};
            GetWindowRect(window, &rectangle);
            toast = CreateWindowExW(WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE | (topmost ? WS_EX_TOPMOST : 0),
                L"STATIC", L"  OK  ", WS_POPUP | WS_BORDER | SS_CENTER | SS_CENTERIMAGE,
                rectangle.right - 76, rectangle.bottom, 76, 30, window, nullptr, GetModuleHandleW(nullptr), nullptr);
            if (toast)
            {
                SendMessageW(toast, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
                ShowWindow(toast, SW_SHOWNOACTIVATE);
                SetTimer(window, TOAST_TIMER, 500, nullptr);
            }
        }
        void execute(int memory = -1)
        {
            const auto source = windowText(edit);
            if (source.find_first_not_of(L" \t\r\n") == std::wstring::npos) return;
            if (memory >= 0 && showingResult)
            {
                if (displayedNumber)
                {
                    calculator->storeMemory(memory, *displayedNumber);
                    notifySuccess();
                }
                return;
            }
            history.submit(source);
            preserveInput = false;
            const auto result = calculator->evaluate(source);
            if (!result.success)
            {
                TextDialog::show(window, L"LineCalc — ERROR", result.error);
                SetFocus(edit);
                return;
            }
            if (memory >= 0 && result.number) calculator->storeMemory(memory, *result.number);
            if (!preserveInput) setText(result.text, true, result.number);
            else
            {
                showingResult = true;
                displayedNumber.reset();
            }
            notifySuccess();
        }
        bool updateDpi(UINT dpi)
        {
            const auto newFont = CreateFontW(-static_cast<int>(std::lround(displayFont.size * dpi / 72)), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, displayFont.name.c_str());
            if (!newFont) return false;
            HDC dc = GetDC(window);
            if (!dc) { DeleteObject(newFont); return false; }
            auto oldFont = SelectObject(dc, newFont);
            TEXTMETRICW metrics{};
            const bool measured = GetTextMetricsW(dc, &metrics) != FALSE;
            SelectObject(dc, oldFont);
            ReleaseDC(window, dc);
            const int newHeight = metrics.tmHeight + MulDiv(10, static_cast<int>(dpi), 96);
            RECT rectangle{0, 0, 400, newHeight};
            if (!measured || !AdjustWindowRectExForDpi(&rectangle, MAIN_STYLE, FALSE, 0, dpi))
            {
                DeleteObject(newFont);
                return false;
            }
            const auto previousFont = font;
            font = newFont;
            inputHeight = newHeight;
            outerHeight = rectangle.bottom - rectangle.top;
            if (edit) SendMessageW(edit, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
            if (toast) SendMessageW(toast, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
            if (previousFont) DeleteObject(previousFont);
            return true;
        }
        void setDisplayFont(const DisplayFontSettings& settings)
        {
            const auto previous = displayFont;
            displayFont = settings;
            if (!updateDpi(GetDpiForWindow(window)))
            {
                displayFont = previous;
                throw std::runtime_error("Cannot create display font");
            }
            resizeToCurrentHeight();
        }
        static UINT_PTR CALLBACK fontDialogHook(HWND dialog, UINT message, WPARAM, LPARAM)
        {
            if (message == WM_INITDIALOG)
            {
                // 公開設定は書体名とサイズのみなので、スタイル選択を無効にする。
                EnableWindow(GetDlgItem(dialog, cmb2), FALSE);
                EnableWindow(GetDlgItem(dialog, stc2), FALSE);
            }
            return 0;
        }
        void chooseDisplayFont()
        {
            LOGFONTW selected{};
            selected.lfHeight = -static_cast<int>(std::lround(displayFont.size * GetDpiForWindow(window) / 72));
            selected.lfWeight = FW_NORMAL;
            selected.lfCharSet = DEFAULT_CHARSET;
            std::copy(displayFont.name.begin(), displayFont.name.end(), selected.lfFaceName);
            CHOOSEFONTW choice{sizeof(choice)};
            choice.hwndOwner = window;
            choice.lpLogFont = &selected;
            choice.iPointSize = static_cast<int>(std::lround(displayFont.size * 10));
            choice.Flags = CF_SCREENFONTS | CF_INITTOLOGFONTSTRUCT | CF_FORCEFONTEXIST | CF_NOVERTFONTS |
                CF_LIMITSIZE | CF_NOSCRIPTSEL | CF_ENABLEHOOK;
            choice.nSizeMin = 1;
            choice.nSizeMax = 200;
            choice.lpfnHook = fontDialogHook;
            if (ChooseFontW(&choice)) setDisplayFont(validateDisplayFont(selected.lfFaceName, choice.iPointSize / 10.0));
            else if (const auto error = CommDlgExtendedError())
                throw std::runtime_error("Cannot open font dialog: " + std::to_string(error));
        }
        static LRESULT CALLBACK editProcedure(HWND control, UINT message, WPARAM wParam, LPARAM lParam, UINT_PTR, DWORD_PTR data)
        {
            auto& self = *reinterpret_cast<Application*>(data);
            if (message == WM_KEYDOWN || message == WM_SYSKEYDOWN)
            {
                if (wParam == VK_RETURN) { self.execute(); return 0; }
                if ((wParam == VK_UP || wParam == VK_DOWN) && !(GetKeyState(VK_SHIFT) & 0x8000))
                {
                    self.setText(self.history.move(windowText(control), wParam == VK_UP, !self.showingResult));
                    return 0;
                }
                if (wParam >= VK_F1 && wParam <= VK_F12)
                {
                    const int index = static_cast<int>(wParam - VK_F1);
                    if (GetKeyState(VK_SHIFT) & 0x8000) self.execute(index);
                    else
                    {
                        const auto value = self.calculator->memoryText(index);
                        SendMessageW(control, EM_REPLACESEL, TRUE, reinterpret_cast<LPARAM>(value.c_str()));
                    }
                    return 0;
                }
                if (wParam == 'A' && (GetKeyState(VK_CONTROL) & 0x8000))
                {
                    SendMessageW(control, EM_SETSEL, 0, -1);
                    return 0;
                }
            }
            if (message == WM_CHAR && (wParam == L'\r' || wParam == L'\n')) return 0;
            if ((message == WM_KEYUP || message == WM_SYSKEYUP) && wParam >= VK_F1 && wParam <= VK_F12) return 0;
            return DefSubclassProc(control, message, wParam, lParam);
        }
        static LRESULT CALLBACK procedure(HWND window, UINT message, WPARAM wParam, LPARAM lParam)
        {
            auto* self = reinterpret_cast<Application*>(GetWindowLongPtrW(window, GWLP_USERDATA));
            if (message == WM_NCCREATE)
            {
                self = static_cast<Application*>(reinterpret_cast<CREATESTRUCTW*>(lParam)->lpCreateParams);
                self->window = window;
                SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
            }
            if (!self) return DefWindowProcW(window, message, wParam, lParam);
            switch (message)
            {
            case WM_CREATE:
                if (!self->updateDpi(GetDpiForWindow(window))) return -1;
                self->edit = CreateWindowExW(0, L"EDIT", L"", WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_AUTOHSCROLL,
                    0, 0, 400, self->inputHeight, window, reinterpret_cast<HMENU>(1), GetModuleHandleW(nullptr), nullptr);
                if (!self->edit) return -1;
                SendMessageW(self->edit, EM_SETLIMITTEXT, 1024 * 1024, 0);
                SendMessageW(self->edit, WM_SETFONT, reinterpret_cast<WPARAM>(self->font), TRUE);
                SetWindowSubclass(self->edit, editProcedure, 1, reinterpret_cast<DWORD_PTR>(self));
                return 0;
            case WM_SETFOCUS:
                if (!self->collapsed) SetFocus(self->edit);
                return 0;
            case WM_ACTIVATE:
                self->active = LOWORD(wParam) != WA_INACTIVE;
                self->updateCompactState();
                if (self->active && self->edit && IsWindowEnabled(window)) SetFocus(self->edit);
                return 0;
            case WM_COMMAND:
                if (HIWORD(wParam) == EN_CHANGE && !self->settingText)
                {
                    self->showingResult = false;
                    self->displayedNumber.reset();
                }
                return 0;
            case WM_GETMINMAXINFO:
                if (self->outerHeight)
                {
                    auto* limits = reinterpret_cast<MINMAXINFO*>(lParam);
                    limits->ptMinTrackSize.x = MulDiv(160, static_cast<int>(GetDpiForWindow(window)), 96);
                    limits->ptMinTrackSize.y = self->currentHeight();
                    limits->ptMaxTrackSize.y = self->currentHeight();
                }
                return 0;
            case WM_SIZING:
            {
                auto* rectangle = reinterpret_cast<RECT*>(lParam);
                if (wParam == WMSZ_TOP || wParam == WMSZ_TOPLEFT || wParam == WMSZ_TOPRIGHT)
                    rectangle->top = rectangle->bottom - self->currentHeight();
                else rectangle->bottom = rectangle->top + self->currentHeight();
                return TRUE;
            }
            case WM_WINDOWPOSCHANGING:
            {
                auto* position = reinterpret_cast<WINDOWPOS*>(lParam);
                if (self->outerHeight && !(position->flags & SWP_NOSIZE)) position->cy = self->currentHeight();
                break;
            }
            case WM_SIZE:
                MoveWindow(self->edit, 0, 0, LOWORD(lParam), self->inputHeight, TRUE);
                return 0;
            case WM_DPICHANGED:
            {
                if (!self->updateDpi(HIWORD(wParam))) return 0;
                const auto* rectangle = reinterpret_cast<RECT*>(lParam);
                SetWindowPos(window, nullptr, rectangle->left, rectangle->top, rectangle->right - rectangle->left,
                    self->currentHeight(), SWP_NOZORDER | SWP_NOACTIVATE);
                return 0;
            }
            case WM_SYSCOMMAND:
                if ((wParam & 0xfff0) == SC_MAXIMIZE || (wParam & 0xfff0) == SC_MINIMIZE) return 0;
                break;
            case WM_TIMER:
                if (wParam == TOAST_TIMER)
                {
                    KillTimer(window, TOAST_TIMER);
                    if (self->toast) DestroyWindow(self->toast);
                    self->toast = nullptr;
                }
                return 0;
            case WM_DESTROY:
                PostQuitMessage(0);
                return 0;
            }
            return DefWindowProcW(window, message, wParam, lParam);
        }
    };

    LaunchOptions selectLaunchOptions()
    {
        std::wstring executable(32768, L'\0');
        const auto length = GetModuleFileNameW(nullptr, executable.data(), static_cast<DWORD>(executable.size()));
        if (!length || length >= executable.size()) throw std::runtime_error("Cannot resolve executable path");
        executable.resize(length);
        auto filename = std::filesystem::path(executable).filename();
        filename.replace_extension(L".js");
        PWSTR local = nullptr;
        if (FAILED(SHGetKnownFolderPath(FOLDERID_LocalAppData, KF_FLAG_DEFAULT, nullptr, &local)))
            throw std::runtime_error("Cannot resolve LocalAppData");
        const auto directory = std::filesystem::path(local) / L"LineCalc";
        CoTaskMemFree(local);
        std::error_code error;
        int count = 0;
        auto arguments = CommandLineToArgvW(GetCommandLineW(), &count);
        if (!arguments) throw std::runtime_error("Cannot read command line");
        std::vector<std::wstring> values;
        for (int i = 1; i < count; ++i) values.emplace_back(arguments[i]);
        LocalFree(arguments);
        auto result = resolveLaunchOptions(values, std::filesystem::current_path(), directory, directory / filename);
        if (!result.debug && !result.createJs)
        {
            std::filesystem::create_directories(directory, error);
        }
        return result;
    }
}

int APIENTRY wWinMain(HINSTANCE instance, HINSTANCE, LPWSTR, int)
{
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    INITCOMMONCONTROLSEX controls{sizeof(controls), ICC_STANDARD_CLASSES};
    InitCommonControlsEx(&controls);
    WNDCLASSEXW windowClass{sizeof(windowClass)};
    windowClass.lpfnWndProc = Application::procedure;
    windowClass.hInstance = instance;
    windowClass.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    const auto systemDpi = GetDpiForSystem();
    windowClass.hIcon = static_cast<HICON>(LoadImageW(instance, MAKEINTRESOURCEW(IDI_LINECALC), IMAGE_ICON,
        GetSystemMetricsForDpi(SM_CXICON, systemDpi), GetSystemMetricsForDpi(SM_CYICON, systemDpi), LR_SHARED));
    windowClass.hIconSm = static_cast<HICON>(LoadImageW(instance, MAKEINTRESOURCEW(IDI_SMALL), IMAGE_ICON,
        GetSystemMetricsForDpi(SM_CXSMICON, systemDpi), GetSystemMetricsForDpi(SM_CYSMICON, systemDpi), LR_SHARED));
    windowClass.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
    windowClass.lpszClassName = L"LineCalcWindow";
    if (!RegisterClassExW(&windowClass)) return 1;
    windowClass.lpfnWndProc = TextDialog::procedure;
    windowClass.lpszClassName = L"LineCalcText";
    if (!RegisterClassExW(&windowClass)) return 1;
    Application application;
    try
    {
        const auto options = selectLaunchOptions();
        if (options.createJs)
        {
            const auto result = createDefinitionTemplate(options.definitions);
            TextDialog::show(nullptr, result.success ? L"LineCalc — OK" : L"LineCalc — ERROR",
                result.success ? options.definitions.wstring() + L"\r\n雛形を作成しました。" : result.error);
            return result.success ? 0 : 1;
        }
        if (options.debug)
        {
            Calculator calculator;
            calculator.initialize();
            const auto result = calculator.loadDefinitions(options.definitions);
            TextDialog::show(nullptr, result.success ? L"LineCalc — OK" : L"LineCalc — ERROR",
                result.success ? options.definitions.wstring() + L"\r\nOK" : result.error);
            return result.success ? 0 : 1;
        }
        application.definitions = options.definitions;
        const auto window = CreateWindowExW(0, L"LineCalcWindow", L"LineCalc", MAIN_STYLE,
            CW_USEDEFAULT, CW_USEDEFAULT, 540, 80, nullptr, nullptr, instance, &application);
        if (!window) throw std::runtime_error("Cannot create main window");
        CalculatorHost host;
        host.setTopmost = [&](bool value) { application.setTopmost(value); };
        host.isTopmost = [&] { return application.topmost; };
        host.clearHistory = [&] { application.history.clear(); application.preserveInput = true; };
        host.showText = [&](const auto& title, const auto& text) { TextDialog::show(window, title, text); };
        host.setDisplayFont = [&](const auto& settings) { application.setDisplayFont(settings); };
        host.getDisplayFont = [&] { return application.displayFont; };
        host.chooseDisplayFont = [&] { application.chooseDisplayFont(); };
        application.calculator = std::make_unique<Calculator>(std::move(host));
        application.calculator->initialize();
        application.setTopmost(true);
        application.calculator->loadDefinitions(application.definitions);
        SetWindowPos(window, nullptr, 0, 0, 540, application.outerHeight, SWP_NOMOVE | SWP_NOZORDER);
        ShowWindow(window, SW_SHOWNORMAL);
        if (!application.collapsed) SetFocus(application.edit);
        MSG message{};
        BOOL status;
        while ((status = GetMessageW(&message, nullptr, 0, 0)) > 0)
        {
            TranslateMessage(&message);
            DispatchMessageW(&message);
        }
        return status < 0 ? 1 : static_cast<int>(message.wParam);
    }
    catch (const std::exception& error)
    {
        MessageBoxW(application.window, Calculator::fromUtf8(error.what()).c_str(), L"LineCalc — ERROR", MB_OK | MB_ICONERROR);
        if (application.window) DestroyWindow(application.window);
        return 1;
    }
}
