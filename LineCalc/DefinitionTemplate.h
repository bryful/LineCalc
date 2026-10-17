#pragma once
#include "framework.h"
#include <filesystem>
#include <memory>
#include <string>

struct TemplateResult
{
    bool success = false;
    std::wstring error;
};

inline TemplateResult createDefinitionTemplate(const std::filesystem::path& path)
{
    // 全行をコメントにして、雛形を読み込んでも環境に副作用を起こさない。
    constexpr char CONTENT[] = R"JS(// LineCalc ユーザー定義ファイル（UTF-8）
// 必要な行のコメントを外して編集してください。
// 起動時と resetJS() 実行時に読み込まれます。
//
// 最前面設定（起動時は true、引数省略は false）:
// setTopmost(true);
// setTopmost(false);
// setTopmost();
//
// 最前面設定の取得:
// isTopmost();
//
// 表示フォント（インストール済みフォント名、サイズは1〜200ポイント）:
// setDisplayFont("Consolas",14); // 定義ファイルからも指定可能
// fonts(); // 入力欄から実行するとフォント名一覧を表示
// getDisplayFont(); // 現在の設定行を表示。コピーしてこのファイルに保存可能
// chooseDisplayFont(); // 入力欄からのみ使用可能。定義ファイル内では実行禁止。
//
// 履歴の消去（現在の入力欄は維持）:
// clearHis();
//
// 全メモリの消去:
// clearMem();
//
// 整数の16進数・2進数表示（引数は±9007199254740991以内の整数）:
// hex(255); // 0xff
// bin(10); // 0b1010
// hex(-255); // -0xff（補数表現ではありません）
// 返り値は文字列ですが、表示時は数値結果としてans・メモリ保存にも使用できます。
//
// ヘルプ・一覧（入力欄から実行すると確認画面を表示）:
// help();
// help("resetJS");
// vars();
// funcs();
// mem();
//
// JS環境を初期化して、このファイルを再読み込み:
// resetJS(); // 入力欄からのみ使用可能。定義ファイル内では実行禁止。
//
// メモリ M1〜M12（初期値 null）:
// M1=0;
// M1=null; // 消去
//
// ans: 直前の数値結果（読み取り専用、初期値 null）
//
// JavaScript標準の数学関数（角度はラジアン）:
// Math.sin(Math.PI/2);
// Math.cos(0);
// Math.sqrt(9);
// Math.pow(2,3);
//
// ユーザー定義関数の例:
// function triple(v) {
//     return v*3;
// }
)JS";
    std::error_code directoryError;
    if (path.has_parent_path()) std::filesystem::create_directories(path.parent_path(), directoryError);
    if (directoryError) return {false, path.wstring() + L"\r\n親ディレクトリを作成できません。エラーコード: " + std::to_wstring(directoryError.value())};
    auto systemError = [&](DWORD code)
    {
        wchar_t message[1024]{};
        FormatMessageW(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, nullptr, code, 0, message, 1024, nullptr);
        return path.wstring() + L"\r\n" + message + L" (" + std::to_wstring(code) + L")";
    };
    const auto raw = CreateFileW(path.c_str(), GENERIC_WRITE | DELETE, 0, nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (raw == INVALID_HANDLE_VALUE) return {false, systemError(GetLastError())};
    std::unique_ptr<void, decltype(&CloseHandle)> file(raw, CloseHandle);
    DWORD written = 0;
    constexpr DWORD length = sizeof(CONTENT) - 1;
    const bool saved = WriteFile(raw, CONTENT, length, &written, nullptr) != FALSE;
    DWORD error = saved ? ERROR_WRITE_FAULT : GetLastError();
    if (saved && written == length)
    {
        if (FlushFileBuffers(raw)) return {true, {}};
        error = GetLastError();
    }
    // 失敗した新規ファイルだけを、ハンドルを保持したまま削除対象にする。
    FILE_DISPOSITION_INFO disposition{TRUE};
    const bool removed = SetFileInformationByHandle(raw, FileDispositionInfo, &disposition, sizeof(disposition)) != FALSE;
    return {false, systemError(error) + (removed ? L"" : L"\r\n不完全なファイルが残っている可能性があります。")};
}
