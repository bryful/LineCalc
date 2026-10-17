'use strict';

// LineCalc本体の公開APIに合わせて、補完・説明・リファレンスを共有する。
const builtins = [
    { name: 'resetJS', signature: 'resetJS()', snippet: 'resetJS()', description: '現在の実行を終了し、変数・関数・メモリ・ansを初期化して定義ファイルを再読み込みします。履歴と表示設定は保持します。定義ファイル内では実行できません。' },
    { name: 'setTopmost', signature: 'setTopmost(bool = false)', snippet: 'setTopmost(${1:true})', description: '最前面表示を設定します。起動時はtrue、引数省略はfalseです。trueの間は非アクティブ時にタイトルバーのみになります。定義ファイルから使用できます。' },
    { name: 'isTopmost', signature: 'isTopmost()', snippet: 'isTopmost()', description: '最前面表示の設定を真偽値で返します。' },
    { name: 'clearHis', signature: 'clearHis()', snippet: 'clearHis()', description: '保存した入力履歴を消去します。現在の入力欄は保持します。' },
    { name: 'clearMem', signature: 'clearMem()', snippet: 'clearMem()', description: 'M1〜M12をすべてnullに戻します。' },
    { name: 'help', signature: 'help(name?)', snippet: 'help(${1})', description: '全体のヘルプ、または指定した関数名の説明を確認画面に表示します。定義ファイル内では画面を表示しません。' },
    { name: 'vars', signature: 'vars()', snippet: 'vars()', description: '定義済みの変数と値を一覧表示します。定義ファイル内では画面を表示しません。' },
    { name: 'funcs', signature: 'funcs()', snippet: 'funcs()', description: 'ユーザー定義関数を一覧表示します。定義ファイル内では画面を表示しません。' },
    { name: 'mem', signature: 'mem()', snippet: 'mem()', description: 'M1〜M12の値を一覧表示します。定義ファイル内では画面を表示しません。' },
    { name: 'hex', signature: 'hex(value)', snippet: 'hex(${1:value})', description: '安全な整数を0x付きの16進数文字列にします。例: hex(255) → "0xff"。引数は±9007199254740991以内の整数。負数は-0xffの形式です。表示時はansとメモリ保存にも対応します。' },
    { name: 'bin', signature: 'bin(value)', snippet: 'bin(${1:value})', description: '安全な整数を0b付きの2進数文字列にします。例: bin(10) → "0b1010"。引数は±9007199254740991以内の整数。補数表現にはしません。表示時はansとメモリ保存にも対応します。' },
    { name: 'fonts', signature: 'fonts()', snippet: 'fonts()', description: 'インストール済みの横書きフォント名を、コピー可能な一覧画面に表示します。定義ファイル内では画面を表示しません。' },
    { name: 'setDisplayFont', signature: 'setDisplayFont(name, size)', snippet: 'setDisplayFont("${1:Consolas}", ${2:14})', description: 'インストール済みフォント名と1〜200ポイントのサイズを設定します。定義ファイルから使用できます。例: setDisplayFont("Consolas", 14)。' },
    { name: 'getDisplayFont', signature: 'getDisplayFont()', snippet: 'getDisplayFont()', description: '現在の表示フォントを、定義ファイルへ貼り付けられる設定行として表示します。定義ファイル内では画面を表示しません。' },
    { name: 'chooseDisplayFont', signature: 'chooseDisplayFont()', snippet: 'chooseDisplayFont()', description: 'Windowsのフォント選択画面を開きます。定義ファイル内では実行できません。' }
];

const variables = [
    { name: 'ans', signature: 'ans', description: '直前の数値結果。読み取り専用、初期値null。エラーや結果なしの実行では更新しません。' },
    ...Array.from({ length: 12 }, (_, i) => ({
        name: `M${i + 1}`, signature: `M${i + 1}`,
        description: `メモリ${i + 1}。初期値null。有限の数値を代入でき、${`M${i + 1}`}=nullで消去します。再宣言とdeleteは禁止です。`
    }))
];

function referenceMarkdown() {
    return '# LineCalc 組み込み関数\n\n' +
        '定義ファイルはUTF-8のJavaScriptです。通常の変数は有限の数値のみ、未初期化のvar／letは0です。' +
        '三角関数はMath.sin／Math.cosなどを使い、角度はラジアンです。\n\n' +
        [...builtins, ...variables].map(item => `## ${item.name}\n\n\`\`\`javascript\n${item.signature}\n\`\`\`\n\n${item.description}\n`).join('\n') +
        '\n## 検証について\n\n`LineCalc::debug` は保存済みのファイルを読み込み、トップレベルの文も実行します。' +
        '関数本体の実行時エラーは、その関数をファイル内から呼び出した場合に検出できます。' +
        '検証結果とエラー位置はLineCalcの確認画面に表示されます。\n';
}

module.exports = { builtins, variables, referenceMarkdown };
