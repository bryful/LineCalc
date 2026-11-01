# LineCalc for VS Code

LineCalcの関数定義ファイルをVS Codeで編集・検証・実行する拡張機能です。LineCalc本体と同じアイコンを使用します。

## インストール

Windows版VS Code 1.90以降と、LineCalc.exeが必要です。

1. VS Codeのコマンドパレットから **Extensions: Install from VSIX...** を選び、`linecalc-0.1.1.vsix` を指定します。
2. **LineCalc::RegisterExecutable** を実行し、使用するLineCalc.exeを選びます。
3. **LineCalc::OpenIncludeDir** で定義ファイルのフォルダを開きます。

実行ファイルが未登録の状態で検証・実行コマンドを使った場合も、登録画面が開きます。実行ファイルを移動した場合は、再登録してください。

## コマンド

コマンドパレットで `LineCalc` を検索してください。

| コマンド | 動作 |
| --- | --- |
| `LineCalc::Open` | 既定の定義ファイルでLineCalcを通常起動。編集中のファイルは保存・実行しない |
| `LineCalc::OpenIncludeDir` | `%LOCALAPPDATA%\LineCalc` を新しいVS Codeウィンドウで開く。フォルダがなければ作成 |
| `LineCalc::RegisterExecutable` | LineCalc.exeを選んでユーザー設定に登録 |
| `LineCalc::debug` | アクティブな.jsを保存して `-debug` で検証 |
| `LineCalc::include` | アクティブな.jsを保存して `-include` で起動 |
| `LineCalc::OpenDefinition` | 既定の定義ファイルを開く。存在しなければ雛形作成を選択可能 |
| `LineCalc::CreateJS` | 保存先を選び、`-createjs` で雛形を作成して開く。既存ファイルは上書きしない |
| `LineCalc::Start` | `LineCalc::Open`と同じ通常起動（従来のコマンド名も利用可能） |
| `LineCalc::Reference` | 組み込み関数とメモリ変数のリファレンスを開く |

ローカルの.jsを開くと、エディター右上と右クリックメニューにもdebug／includeが表示されます。

他の言語のコードを編集中や、エディターでファイルを開いていない場合も、`LineCalc::Open`で計算機だけを起動できます。

`debug`／`include`は未保存の変更を保存してから実行します。新規JavaScript文書は、先に.jsとして保存してください。ファイルを保存できない場合や保存をキャンセルした場合は実行しません。

検証結果はLineCalcの確認画面に表示されます。問題なければOK、問題があればファイル名・位置・内容を確認できます。確認画面を閉じると検証が終了します。同じファイルの検証中は重複した検証を起動しません。

`debug` はファイルを読み込み、トップレベルの文も実行します。関数本体の実行時エラーは、その関数をファイル内から呼び出した場合に検出できます。`include` は指定ファイルの定義で計算機を起動します。どちらも、その.jsがあるフォルダをカレントディレクトリにします。

既定の定義ファイル名は、登録した実行ファイルの拡張子を.jsに変えた名前です。たとえば `LineCalc.exe` なら `%LOCALAPPDATA%\LineCalc\LineCalc.js`、`MyCalc.exe` なら `MyCalc.js` です。

## 編集支援

JavaScriptファイルでは、LineCalcの全組み込み関数、`ans`、`M1`〜`M12`を補完します。関数名にマウスを合わせると引数と説明を表示します。

```javascript
function triple(value) {
    return value * 3;
}

setTopmost(true);
setDisplayFont("Consolas", 14);
```

`resetJS()` と `chooseDisplayFont()` は定義ファイル内で実行できないことも説明に記載しています。LineCalcの変数は数値専用です。三角関数は `Math.sin()`／`Math.cos()` などを使用し、角度はラジアンです。

| スニペット | 内容 |
| --- | --- |
| `lc-function` | ユーザー定義関数 |
| `lc-display` | 最前面表示とフォント設定 |
| `lc-sum` | 数値変数を使うループ関数 |

## 設定

| 設定名 | 内容 |
| --- | --- |
| `linecalc.executablePath` | このPCで使用するLineCalc.exeの絶対パス |
| `linecalc.enableCompletions` | LineCalcの入力補完を有効にする（初期値true） |
| `linecalc.enableHover` | マウスを合わせたときの説明を有効にする（初期値true） |

## 動作環境とトラブルシューティング

Windowsのローカルファイルを対象とします。WSL・SSHなどのリモート文書は、ローカルの.jsとして保存してから実行してください。ワークスペースが制限モードの場合、編集支援は利用でき、コード実行を伴う操作はワークスペースを信頼した後に利用できます。

起動した実行ファイルと引数、終了コードは **表示 → 出力 → LineCalc** で確認できます。起動できない場合は実行ファイルの登録先を確認してください。

拡張機能が保存する設定は実行ファイルのパスと編集支援の設定です。定義ファイルは選択した保存先に置きます。ネットワーク通信やテレメトリーは行いません。

## ライセンス

LineCalc拡張機能は **MIT License** で公開します。著作権表示は `Copyright (c) 2026 bryful` です。[LICENSE](LICENSE) を参照してください。
