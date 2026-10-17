'use strict';

const { builtins, variables, referenceMarkdown } = require('./catalog');

function registerEditorFeatures(vscode, context) {
    const selector = { language: 'javascript', scheme: 'file' };
    context.subscriptions.push(vscode.languages.registerCompletionItemProvider(selector, {
        provideCompletionItems(document, position) {
            if (!vscode.workspace.getConfiguration('linecalc', document.uri).get('enableCompletions', true)) return [];
            const before = document.lineAt(position.line).text.slice(0, position.character);
            if (/\.\s*[\w$]*$/.test(before)) return [];
            return [...builtins, ...variables].map(item => {
                const completion = new vscode.CompletionItem(item.name, item.snippet ? vscode.CompletionItemKind.Function : vscode.CompletionItemKind.Variable);
                completion.detail = `LineCalc — ${item.signature}`;
                completion.documentation = new vscode.MarkdownString(item.description);
                completion.insertText = item.snippet ? new vscode.SnippetString(item.snippet) : item.name;
                return completion;
            });
        }
    }));
    context.subscriptions.push(vscode.languages.registerHoverProvider(selector, {
        provideHover(document, position) {
            if (!vscode.workspace.getConfiguration('linecalc', document.uri).get('enableHover', true)) return undefined;
            const range = document.getWordRangeAtPosition(position);
            if (!range) return undefined;
            const before = document.lineAt(range.start.line).text.slice(0, range.start.character);
            if (/\.\s*$/.test(before)) return undefined;
            const item = [...builtins, ...variables].find(entry => entry.name === document.getText(range));
            if (!item) return undefined;
            const markdown = new vscode.MarkdownString();
            markdown.appendCodeblock(item.signature, 'javascript');
            markdown.appendText(item.description);
            return new vscode.Hover(markdown, range);
        }
    }));
    context.subscriptions.push(vscode.workspace.registerTextDocumentContentProvider('linecalc-reference', {
        provideTextDocumentContent: referenceMarkdown
    }));
}

module.exports = { registerEditorFeatures };
