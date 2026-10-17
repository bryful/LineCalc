'use strict';

const { test } = require('node:test');
const assert = require('node:assert/strict');
const { registerEditorFeatures } = require('../src/editor');

function fixture() {
    const providers = {};
    const settings = { enableCompletions: true, enableHover: true };
    const disposable = {};
    const vscode = {
        CompletionItem: class { constructor(label, kind) { this.label = label; this.kind = kind; } },
        CompletionItemKind: { Function: 1, Variable: 2 },
        SnippetString: class { constructor(value) { this.value = value; } },
        MarkdownString: class {
            constructor(value = '') { this.value = value; }
            appendCodeblock(value) { this.value += value; }
            appendText(value) { this.value += value; }
        },
        Hover: class { constructor(contents, range) { this.contents = contents; this.range = range; } },
        workspace: {
            getConfiguration: () => ({ get: key => settings[key] }),
            registerTextDocumentContentProvider: (_, provider) => { providers.reference = provider; return disposable; }
        },
        languages: {
            registerCompletionItemProvider: (_, provider) => { providers.completion = provider; return disposable; },
            registerHoverProvider: (_, provider) => { providers.hover = provider; return disposable; }
        }
    };
    const context = { subscriptions: [] };
    registerEditorFeatures(vscode, context);
    function document(text, word, column = 0) {
        return { uri: {}, lineAt: () => ({ text }), getWordRangeAtPosition: () => ({ start: { line: 0, character: column } }), getText: () => word };
    }
    return { providers, settings, context, document };
}

test('completion provides snippets for functions and plain memory names', () => {
    const f = fixture();
    const items = f.providers.completion.provideCompletionItems(f.document('setDis', ''), { line: 0, character: 6 });
    assert.ok(items.find(item => item.label === 'setDisplayFont').insertText.value.includes('Consolas'));
    assert.equal(items.find(item => item.label === 'M12').insertText, 'M12');
    assert.equal(f.context.subscriptions.length, 3);
});

test('completion and hover can be disabled and do not replace member access', () => {
    const f = fixture();
    assert.deepEqual(f.providers.completion.provideCompletionItems(f.document('obj.he', ''), { line: 0, character: 6 }), []);
    assert.equal(f.providers.hover.provideHover(f.document('obj.hex', 'hex', 4), {}), undefined);
    f.settings.enableCompletions = false;
    f.settings.enableHover = false;
    assert.deepEqual(f.providers.completion.provideCompletionItems(f.document('', ''), { line: 0, character: 0 }), []);
    assert.equal(f.providers.hover.provideHover(f.document('hex', 'hex'), {}), undefined);
});

test('hover and reference describe definition-file restrictions', () => {
    const f = fixture();
    const hover = f.providers.hover.provideHover(f.document('chooseDisplayFont()', 'chooseDisplayFont'), {});
    assert.ok(hover.contents.value.includes('定義ファイル内では実行できません'));
    assert.ok(f.providers.reference.provideTextDocumentContent().includes('setDisplayFont'));
});
