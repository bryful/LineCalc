# 開発・検証

## Android版

`android/` をAndroid Studioで開きます。JDK 17以降、Android SDK 35、NDK 30.0.16248370、CMake 4.1.2を使用します。Gradle Wrapperを同梱しています。SDKの場所は `ANDROID_HOME` または未追跡の `android/local.properties` の `sdk.dir` に指定します。

```powershell
cd android
./gradlew.bat assembleDebug assembleRelease lintDebug
# エミュレーターまたはテスト端末を接続して実行
./gradlew.bat connectedDebugAndroidTest
```

Gradleのテスト実行ツールを取得できない場合は、`assembleDebugAndroidTest` でテストAPKを作成して、`adb -s <テスト端末ID> install -r` で本体とテストAPKをインストールした後、`adb -s <テスト端末ID> shell am instrument -w io.github.bryful.linecalc.test/android.test.InstrumentationTestRunner` でも実行できます。

APKは標準の `android/app/build/outputs/apk/` 配下に生成します。Debug APKは開発用署名付き、Release APKはリリース用の鍵で署名します。arm64-v8aとx86_64、Android 10（API 29）以降に対応します。

リリースの秘密鍵とパスワードは、リポジトリ `LineCalc` の隣の `LineCalc_signing/` に集約します。Gradleは同フォルダーの `signing.properties` を読み込み、`storeFile`（同フォルダーを基準とする相対パス）、`storePassword`、`keyAlias`、`keyPassword` を使用します。署名設定がない場合、Releaseのビルドは失敗します。Debugのビルドには不要です。鍵と設定ファイルはリポジトリに含めず、更新版にも同じ鍵を使用します。Release APKは `android/app/build/outputs/apk/release/app-release.apk` に生成します。

画面はAndroid標準のJava APIで実装し、追加のUIライブラリは使いません。JNIから既存の `Calculator.cpp` と同梱QuickJS-NGを呼び出し、AcornとRuntime.jsもWindows版と同じファイルをビルド時に取り込みます。Android固有のリソース取得・Unicode変換・非対応関数の処理だけを条件分岐しています。

専用キーボードは `CalculatorKeyboard.java` で数値・QWERTY英字・JS記号の3面を構成します。入力先は既存のEditTextで、選択・コピー・貼り付けと物理キーボードの操作を維持します。専用キーボード使用時はIMEの自動表示を止め、標準キーボードへの切替設定はSharedPreferencesに保存します。面・大文字状態・入力選択範囲は画面回転時にセッションから復元します。削除と左右移動はUnicodeコードポイント単位で処理し、長押しリピートは指を離したとき・キャンセル・画面離脱・計算開始時に停止します。統合テストではこれらの編集操作、長押し停止、キーボード切替も確認します。

JavaScriptは専用の単一ワーカースレッドで実行します。画面回転時にはActivityから独立したセッションを引き継ぎます。初期JSはMediaStore.Filesで `Documents/LineCalc/LineCalc.js` に作成し、初回のフォルダー選択やストレージ全体のアクセス権を要求しません。設定変更時だけStorage Access Frameworkを使用し、フォルダーの永続URI権限を保持します。既存ファイルを参照できない場合に別名の雛形を作らないよう検査します。

ネイティブ側にはアプリ内部のキャッシュに保存したUTF-8のスナップショットを渡します。ユーザー定義の読込は起動・明示的な再読込・検証・resetJSのときに限り、通常の計算では共有ファイルを読みません。検証は別のJS環境で実行します。AndroidのresetJSは実行終了後にJava側で共有ファイルを読み直して適用します。

Androidの統合テストは、計算互換性、数値制約、メモリ、日本語識別子、DocumentsのJS編集と再読込、読込エラー、検証の独立性、タイムアウト後の復帰、画面の入力・実行操作を確認します。テストは定義ファイルを一時変更して最後に元に戻すため、専用のエミュレーターを使用してください。

VS Code拡張機能は `vscode-linecalc/` にあります。開発・テスト・VSIXの作成方法は [拡張機能の開発文書](vscode-linecalc/DEVELOPMENT.md) を参照してください。C++本体と独立したJavaScriptプロジェクトです。

## ビルド

Visual Studio 2026、MSVC v145、Windows SDK、C++20、Unicode、x64のDebug／Releaseを使用します。`LineCalc.slnx` をVisual Studioで開いてビルドできます。既存のWin32構成も維持しています。中間・出力ディレクトリはVisual Studioの既定値を使用し、OutDir／IntDirの独自指定はありません。

VS 2026のx64 Native Tools Command Promptで実行する場合:

```bat
MSBuild LineCalc.slnx /p:Configuration=Debug /p:Platform=x64
MSBuild LineCalc.slnx /p:Configuration=Release /p:Platform=x64
MSBuild Tests\Tests.vcxproj /p:Configuration=Debug /p:Platform=x64
Tests\x64\Debug\Tests.exe
```

テストプロジェクトを直接ビルドする場合、テストの実行ファイルは `Tests\x64\Debug\Tests.exe` に出力されます。

## 実装

OK通知とTextDialogは `PopupPlacement.h` で親ウィンドウのモニターの作業領域に収めます。通常は下側、収まらなければ上側に配置し、左右・上下を補正します。親がない場合はカーソルのあるモニターの中央に配置します。作業領域より大きい確認画面はサイズを縮め、負の座標を持つモニターも同じ座標系で扱います。

`WM_ACTIVATE` でアクティブ状態を追跡し、最前面かつ非アクティブのときだけEDITを隠して非クライアント領域のみの高さにします。通常時と縮小時の高さを分け、サイズ制約・フォント変更・DPI変更でも現在の表示状態に合う高さを使用します。復帰時は既存EDITを再表示するため入力と選択範囲を維持し、非アクティブのまま最前面を解除した場合も通常サイズへ戻します。

アプリアイコンの元画像は `assets/LineCalc.png` です。選定されたA案（ダークな背景にミント色の `>` と白い `=`）を維持しています。`Tools/generate_icons.py` をPillowが利用可能なPythonで実行すると、16／20／24／32／40／48／64／96／128／256ピクセルを含む `LineCalc.ico` と `small.ico` を再生成します。通常のビルドでは同梱ICOを使用するため、PythonやPillowは不要です。

表示フォントは `DisplayFont.h` でインストール済みの横書きフォント名を列挙・検証します。`setDisplayFont` は名前と1〜200ポイントのサイズを受け取り、DPIに応じてHFONTと1行分の高さを更新します。新しいフォントを作成・適用した後に以前のHFONTを解放し、失敗時は既存設定を維持します。`chooseDisplayFont` はWindows標準の選択画面を使い、書体名・サイズだけを扱うためスタイル選択を無効にしています。`getDisplayFont` の設定行はJSONの文字列エスケープを使用します。

Win32の単一行EDITコントロールを使用します。DPIに応じて高さを固定し、幅だけを変更できます。計算エラーと一覧表示は選択・コピー可能な読み取り専用EDITを持つ確認ウィンドウ、成功通知はフォーカスを奪わないポップアップです。

JavaScript実行環境はQuickJS-NG 0.17.0、構文解析はAcorn 8.15.0です。どちらもソースを同梱し、実行時の追加DLLやネットワーク取得を必要としません。QuickJSの結合版CソースはMSVC C17とC11 atomics対応オプションでビルドします。外部ソースだけ警告を抑制し、アプリ側は通常の警告設定を使います。

Acornの構文木から、未初期化宣言の0補完、予約名の検査、数値代入の検査、最後の文に応じた表示制御を行います。実行にはグローバル評価を用い、letを含む環境を評価間で維持します。変数・関数一覧は入力および定義ファイルの構文木から取得した名前を参照します。

実行時間は1回2秒、JSメモリは64MiB、JSスタック上限は2MiBです。ユーザーコードにファイル操作・プロセス起動などのネイティブAPIは公開しません。resetJSは実行の終了後に環境を破棄して再生成します。ユーザー定義ファイルのエラーは、仕様に従ってUI通知しません。

## テスト

`Tests.cpp` は四則演算、数値表記、宣言、複数文、ループ、再帰、履歴、予約メモリ、数値制約、ans、組み込み関数、定義ファイルの読み込みとreset、実行時間制限を検証します。

`-debug [path]` は定義ファイル評価後に結果をモーダル表示して終了します。通常起動の読み込みエラーは引き続き通知しません。`loadDefinitions` の戻り値でデバッグ時のみエラーを表示し、成功0／失敗1を返します。Acornの構文エラーには元の行・列を付加し、実行時スタックの列位置は挿入・置換情報から元のソース位置に戻します。パス解決は `LaunchOptions.h` に分離し、検索順とオプション併用を自動テストしています。

UIスモークテストは、ビルド済みのアプリを対象に `Tests\UiSmoke.ps1` で実行します。

ウィンドウ位置のJSON読み書きと作業領域への補正は `WindowState.h` に実装しています。JSONは既存のQuickJSで構文解析し、数値の整数座標と正のサイズを検証します。保存は同じディレクトリの一時ファイルから置き換えます。起動時の高さはフォントとDPIに応じた1行分を使用します。UIテストは既存の `LineCalc.json` を退避・復元し、画面外からの復元、幅の復元、終了時の保存、縮小中の通常高さの保存を確認します。

`Tests\DebugSmoke.ps1` は実際の `-debug` 起動について、成功・構文エラー・実行時エラー・ファイル不在・オプション併用・終了コードを確認します。各スクリプトの `-Executable` 引数でDebug／Releaseの対象を指定できます。

`-createjs [path]` は `DefinitionTemplate.h` のコメントだけのUTF-8雛形を新規作成し、結果表示後に終了します。ファイル名だけならLocalAppData、明示的な相対パスならカレントを基準にします。Win32 `CREATE_NEW` で既存ファイルの上書きを防ぎます。書き込み失敗時は、新規作成したハンドルだけを削除対象にします。自動テストでパス選択、全組み込み関数の記載、コメント化、生成物の評価、既存内容の保持を確認し、CLIスモークテストでも作成と再評価を確認します。
