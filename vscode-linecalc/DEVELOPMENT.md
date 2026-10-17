# 開発

JavaScriptとNode.js標準モジュール、VS Code APIを使用します。実行時のnpm依存はありません。C++アプリのソリューション・プロジェクト・標準出力先は変更しません。

```text
npm ci --ignore-scripts
npm run check
npm test
npm run test:integration
npm run package
```

`@vscode/vsce` は公式のVSIX作成ツールで、開発時だけ使用します。VSIXにnode_modulesやテスト、開発文書を含めません。`package-lock.json` で開発依存を固定しています。

VS Codeでこのフォルダを開き、F5の **LineCalc Extension** で拡張機能開発ホストを起動できます。配布用VSIXはこのフォルダに生成します。

## 実装と検証

`src/commands.js` はVS Code APIとプロセス起動処理を注入できる構造にし、保存キャンセル・実行ファイルの不在・パスの扱い・信頼状態・重複検証・雛形の上書き防止をテストします。実行は `spawn(exe, args, {shell:false})` を使用し、引数をシェルの文字列に連結しません。

`src/catalog.js` の公開API一覧を入力補完・ホバー・リファレンスで共有します。本体のNATIVE_NAMESと一致することもテストします。`src/editor.js` はJavaScriptのグローバル名だけを補完し、メンバーアクセス中の候補を抑制します。

`test/vscode.integration.js` は実際のVS Code拡張機能ホストでコマンド登録、入力補完、ホバー、リファレンス、未保存文書の保存とLineCalc実行を確認します。VS Codeのユーザーデータ・拡張フォルダはテスト専用に分離し、普段使っている設定とインストール済み拡張を変更しません。

検証時はLineCalcのデバッグ結果画面を対象プロセスIDで特定して閉じ、includeで開いたテスト用アプリも同様に終了します。

## 公開

ローカルVSIXの作成のみを行います。Marketplaceへの公開は別途明示的に実施してください。`publisher` は `bryful` です。公開する場合は対応するMarketplaceのpublisherを所有していることを確認してください。
