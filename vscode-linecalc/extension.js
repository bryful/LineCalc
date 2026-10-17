'use strict';

const { createCommands } = require('./src/commands');
const { registerEditorFeatures } = require('./src/editor');

function activate(context) {
    const vscode = require('vscode');
    const output = vscode.window.createOutputChannel('LineCalc');
    context.subscriptions.push(output);
    const commands = createCommands(vscode, output);
    for (const [name, command] of Object.entries(commands)) {
        context.subscriptions.push(vscode.commands.registerCommand(`linecalc.${name}`, async () => {
            try { return await command(); }
            catch (error) {
                output.appendLine(`ERROR: ${error.message}`);
                vscode.window.showErrorMessage(`LineCalc: ${error.message}`);
                return undefined;
            }
        }));
    }
    context.subscriptions.push(vscode.commands.registerCommand('linecalc.reference', async () => {
        const document = await vscode.workspace.openTextDocument(vscode.Uri.parse('linecalc-reference:/builtins.md'));
        return vscode.window.showTextDocument(document, { preview: true });
    }));
    registerEditorFeatures(vscode, context);
}

module.exports = { activate };
