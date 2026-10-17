'use strict';

const assert = require('node:assert/strict');
const path = require('node:path');
const fs = require('node:fs/promises');
const childProcess = require('node:child_process');

async function run() {
    const vscode = require('vscode');
    const extensionRoot = path.resolve(__dirname, '..');
    const executable = path.resolve(extensionRoot, '../x64/Release/LineCalc.exe');
    const originalSpawn = childProcess.spawn;
    const launched = [];
    let checks = 0;
    const check = (condition, message) => { assert.ok(condition, message); checks++; };
    childProcess.spawn = (...args) => {
        const child = originalSpawn(...args);
        if (String(args[0]).toLowerCase() === executable.toLowerCase()) launched.push(child);
        return child;
    };

    function closeNative(child, mode, expected = 'OK') {
        return new Promise((resolve, reject) => {
            const helper = originalSpawn('powershell.exe', [
                '-NoProfile', '-ExecutionPolicy', 'Bypass', '-File', path.join(__dirname, 'close-linecalc.ps1'),
                '-AppProcessId', String(child.pid), '-Mode', mode, '-Expected', expected
            ], { windowsHide: true, stdio: ['ignore', 'pipe', 'pipe'] });
            let output = '';
            helper.stdout.on('data', bytes => { output += bytes.toString('utf8'); });
            helper.stderr.on('data', bytes => { output += bytes.toString('utf8'); });
            const timer = setTimeout(() => { helper.kill(); child.kill(); reject(new Error('Native test timed out')); }, 25000);
            helper.once('error', error => { clearTimeout(timer); child.kill(); reject(error); });
            helper.once('exit', code => {
                clearTimeout(timer);
                if (code !== 0) { child.kill(); reject(new Error(output)); }
                else resolve(output);
            });
        });
    }

    try {
        const extension = vscode.extensions.getExtension('bryful.linecalc');
        check(extension, 'Extension is discoverable');
        await extension.activate();
        const commands = await vscode.commands.getCommands(true);
        for (const name of ['open', 'openIncludeDir', 'debug', 'include', 'registerExecutable', 'openDefinition', 'createJs', 'start', 'reference']) {
            check(commands.includes(`linecalc.${name}`), `Command registered: ${name}`);
        }
        await vscode.workspace.getConfiguration('linecalc').update('executablePath', executable, vscode.ConfigurationTarget.Global);
        check(vscode.workspace.isTrusted, 'Integration workspace is trusted');
        const folder = vscode.workspace.workspaceFolders[0].uri;
        const uri = vscode.Uri.joinPath(folder, '拡張 テスト & (file).js');
        await vscode.workspace.fs.writeFile(uri, Buffer.from('setTopmost(false);\nfunction triple(v){return v*3;}\nhex(255);\nsetDis\n'));
        const document = await vscode.workspace.openTextDocument(uri);
        let editor = await vscode.window.showTextDocument(document);
        const completions = await vscode.commands.executeCommand('vscode.executeCompletionItemProvider', uri, new vscode.Position(3, 6));
        check(completions.items.some(item => (typeof item.label === 'string' ? item.label : item.label.label) === 'setDisplayFont' && item.detail?.includes('LineCalc')), 'Actual completion provider');
        const hovers = await vscode.commands.executeCommand('vscode.executeHoverProvider', uri, new vscode.Position(2, 1));
        check(hovers.some(hover => hover.contents.some(item => String(item.value).includes('0x'))), 'Actual hover provider');
        await vscode.commands.executeCommand('linecalc.reference');
        check(vscode.window.activeTextEditor.document.uri.scheme === 'linecalc-reference', 'Read-only reference opens');
        check(vscode.window.activeTextEditor.document.getText().includes('chooseDisplayFont'), 'Reference content');
        editor = await vscode.window.showTextDocument(document);
        await editor.edit(edit => {
            edit.delete(document.lineAt(3).range);
            edit.insert(new vscode.Position(3, 0), '// saved by debug command');
        });
        check(document.isDirty, 'Test document has unsaved changes');

        for (const mode of ['debug', 'include']) {
            const previousCount = launched.length;
            const execution = vscode.commands.executeCommand(`linecalc.${mode}`);
            for (let i = 0; i < 100 && launched.length === previousCount; i++) await new Promise(resolve => setTimeout(resolve, 50));
            check(launched.length > previousCount, `Actual executable launched by ${mode}`);
            const result = await closeNative(launched.at(-1), mode);
            check(result.includes(`${mode} OK`), `${mode} displayed its expected native window`);
            check(await execution === 0, `${mode} exited successfully`);
        }
        check(!document.isDirty, 'Debug saved the document');
        check((await fs.readFile(uri.fsPath, 'utf8')).includes('saved by debug command'), 'Saved contents reached disk');
        await editor.edit(edit => edit.insert(document.lineAt(document.lineCount - 1).range.end, '\nlet broken = ;'));
        const previousCount = launched.length;
        const failedDebug = vscode.commands.executeCommand('linecalc.debug');
        for (let i = 0; i < 100 && launched.length === previousCount; i++) await new Promise(resolve => setTimeout(resolve, 50));
        check(launched.length > previousCount, 'Invalid file launches debug');
        const failure = await closeNative(launched.at(-1), 'debug', 'ERROR');
        check(failure.includes('SyntaxError') && failure.includes(uri.fsPath), 'Error dialog contains the syntax error and exact Unicode file path');
        check(await failedDebug === 1, 'Invalid file returns failure code');
        const otherDocument = await vscode.workspace.openTextDocument({ language: 'cpp', content: 'int main() { return 0; }' });
        await vscode.window.showTextDocument(otherDocument);
        const previousLaunchCount = launched.length;
        const opening = vscode.commands.executeCommand('linecalc.open');
        for (let i = 0; i < 100 && launched.length === previousLaunchCount; i++) await new Promise(resolve => setTimeout(resolve, 50));
        check(launched.length > previousLaunchCount, 'Open launches while editing an untitled C++ document');
        const opened = await closeNative(launched.at(-1), 'include');
        check(opened.includes('include OK'), 'Open displays the normal calculator window');
        check(await opening === 0, 'Open exited successfully');
        check(otherDocument.isUntitled, 'Open did not save the unrelated document');
        console.log(`LINECALC_INTEGRATION_OK ${checks} checks`);
        if (process.env.LINECALC_TEST_REPORT) await fs.writeFile(process.env.LINECALC_TEST_REPORT, JSON.stringify({ success: true, checks }));
    } finally {
        childProcess.spawn = originalSpawn;
        for (const child of launched) if (child.exitCode === null) child.kill();
    }
}

module.exports = { run: async () => {
    try { await run(); }
    catch (error) {
        if (process.env.LINECALC_TEST_REPORT) await fs.writeFile(process.env.LINECALC_TEST_REPORT, JSON.stringify({ success: false, error: error.stack }));
        throw error;
    }
} };
