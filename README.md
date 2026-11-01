# LineCalc

WindowsとAndroidで使える、1つの入力欄で計算するJavaScript計算機です。式を実行すると同じ欄に結果が表示され、そのまま次の計算へつなげられます。変数・条件分岐・ループ・ユーザー関数にも対応します。

## ダウンロードと対応環境

[配布用ZIPをダウンロード](_dl/LineCalc110.zip)し、展開して利用してください。Windows版、署名済みAndroid版、Windows用VS Code拡張機能、利用案内とライセンスを同梱しています。

| 版 | 対応環境 | 配布ファイル |
| --- | --- | --- |
| Windows | Windows 10／11、64ビット | [LineCalc.exe](_dl/LineCalc110/LineCalc.exe) |
| Android | Android 10以降、arm64／x86_64 | [LineCalc-Android.apk](_dl/LineCalc110/LineCalc-Android.apk) |
| VS Code拡張 | Windows上のVS Code | [linecalc-0.1.1.vsix](_dl/LineCalc110/vscode-linecalc/linecalc-0.1.1.vsix) |

## Windows版

`LineCalc.exe` を起動し、式を入力してEnterで実行します。結果はそのまま編集でき、↑／↓で入力履歴を呼び出せます。Ctrl+C／X／Vによるコピー・切り取り・貼り付けにも対応します。

横幅を変更できる1行のウィンドウです。起動時は最前面に表示し、非アクティブになると入力欄を隠してタイトルバーだけに縮みます。クリックやAlt+Tabで入力欄へ戻れます。`setTopmost(false)` で最前面表示を解除できます。

F1～F12でメモリの値を挿入し、Shift+F1～F12で数値結果を保存します。

フォント設定、定義ファイルの雛形作成・検証、起動オプションなどは [Windows版の詳しい使い方](docs/windows.md) を参照してください。

## Android版

APKを端末で開き、画面の案内に従ってインストールします。インストール元のアプリについて許可を求められた場合は、端末の設定で許可してください。

画面は「履歴・メモリ・関数・再読込などの機能ボタン」「入力行」「専用キーボード」で構成します。専用キーボードは数値・英字・JSの3面を切り替えられ、横画面では数値・JSキーを3行に配置します。

- **実行**：入力式を計算します。`=` キーは変数への代入用です。
- **BS／⌫**：選択範囲または直前の文字を削除します。
- **◀／▶**：カーソルを移動します。削除とカーソル移動は長押しで連続操作できます。
- **AC**：入力行だけを消去します。変数・メモリ・履歴は保持します。
- **再読込**：変数・関数・メモリ・ansを初期化し、初期JSを読み直します。履歴と入力行は保持します。

履歴・メモリ・関数は上部のボタンから利用できます。コピー・貼り付けは入力欄の長押しから操作します。日本語入力などが必要な場合は「⋮ → 標準キーボードに切替」を使用してください。

詳しくは [Android版の使い方](android/README.md) を参照してください。

## 計算例

```javascript
(15-2)*12
Math.sin(Math.PI/2)
0xff+0b1010
price=1200
price*1.1
ans/2
function triple(v){return v*3;}
triple(5)
a=0; for(var i=0;i<10;i++) a+=i; a;
```

三角関数の角度はラジアンです。変数は有限の数値専用で、初期値のない `var`／`let` 宣言は0になります。宣言・関数定義だけの実行は空欄になります。`a=1` は1を表示し、`a=1;` は空欄にします。

`ans` は直前の数値結果です。`M1`～`M12` はメモリで、初期値は `null` です。`M1=100` で保存し、`M1=null` で消去できます。

| 関数 | 用途 |
| --- | --- |
| `hex(value)`／`bin(value)` | 安全な整数を16進数／2進数で表示 |
| `vars()`／`funcs()`／`mem()` | 変数・ユーザー関数・メモリの一覧 |
| `clearHis()`／`clearMem()` | 履歴／全メモリの消去 |
| `help()` | ヘルプの表示 |
| `resetJS()` | 計算環境を初期化して定義ファイルを再読込 |

最前面表示・フォント関連の関数はWindows版専用です。

## 初期読込JSとデータの保存先

よく使う関数や定数をUTF-8の `LineCalc.js` に記述すると、起動時と `resetJS()` 実行時に読み込めます。複数行の関数定義も可能です。

| データ | Windows | Android |
| --- | --- | --- |
| 初期読込JS | `%LOCALAPPDATA%\LineCalc\LineCalc.js` | `Documents/LineCalc/LineCalc.js` |
| ウィンドウ位置・サイズ | `%LOCALAPPDATA%\LineCalc\LineCalc.json` | 対象外 |

Android版は初回起動時に初期JSの雛形を自動作成します。最初にフォルダーを選択する操作は不要です。外部エディターやPCで編集でき、読込先は「⋮ → 設定 → 読込フォルダーを変更」で変更できます。既存ファイルは上書きしません。

Windows版の雛形作成は [Windows版の詳しい使い方](docs/windows.md#定義ファイルの検証) を参照してください。履歴・変数・関数・メモリは終了時に消去します。Android版は画面回転や一時的なバックグラウンド移行では状態を保持し、プロセス終了時に消去します。

## 困ったときは

- **計算エラー**：ゼロ除算、NaN、無限大、不正な式はエラーになります。入力式は保持されます。実行時間の上限は2秒です。
- **WindowsでDLLが見つからない**：Microsoft公式の [Visual C++再頒布可能パッケージ（x64）](https://learn.microsoft.com/ja-jp/cpp/windows/latest-supported-vc-redist) をインストールしてください。
- **Androidで初期JSを読み込めない**：再インストール、外部エディターによるファイルの置き換え、アクセス権の失効後などは、設定から該当フォルダーを選択し直してください。共有フォルダーのJSはアプリを削除しても残ります。
- **AndroidのDebug版に上書きできない**：配布用Release版とは署名が異なります。必要な初期JSを保存してからDebug版を削除し、Release版をインストールしてください。

Windows版は起動時の定義ファイルの読込・実行エラーを通知しません。定義ファイルの検証機能で確認してください。Android版では読込エラーを画面に表示し、基本計算は引き続き利用できます。

## VS Code拡張機能

Windows版の定義ファイルの編集・補完・検証・実行には [LineCalc for VS Code](vscode-linecalc/README.md) を利用できます。VSIXをインストールし、`LineCalc::RegisterExecutable` で `LineCalc.exe` を登録してください。

## ライセンス

LineCalc本体は [MIT License](LICENSE) で公開します。`Copyright (c) 2026 bryful`。

同梱ライブラリのライセンスは [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md) を参照してください。
