'use strict';

const path = require('node:path');
const os = require('node:os');
const fs = require('node:fs/promises');
const { spawn } = require('node:child_process');

function defaultDirectory(environment = process.env, home = os.homedir()) {
    return path.join(environment.LOCALAPPDATA || path.join(home, 'AppData', 'Local'), 'LineCalc');
}

function defaultDefinition(executable, directory) {
    return path.join(directory, path.basename(executable, path.extname(executable)) + '.js');
}

function createCommands(vscode, output, dependencies = {}) {
    const fileSystem = dependencies.fs || fs;
    const spawnProcess = dependencies.spawn || spawn;
    const platform = dependencies.platform || process.platform;
    const directory = dependencies.directory || defaultDirectory();
    const pendingDebug = new Set();

    function requireWindows() {
        if (platform !== 'win32') throw new Error('この拡張機能のLineCalc操作はWindows版VS Codeで使用してください。');
    }

    function requireTrust() {
        if (!vscode.workspace.isTrusted) throw new Error('コードを実行するには、VS Codeでこのワークスペースを信頼してください。');
    }

    async function isFile(filename) {
        try { return (await fileSystem.stat(filename)).isFile(); }
        catch (error) {
            if (error.code === 'ENOENT' || error.code === 'ENOTDIR') return false;
            throw error;
        }
    }

    async function validateExecutable(filename) {
        if (!path.isAbsolute(filename) || path.extname(filename).toLowerCase() !== '.exe' || !(await isFile(filename))) {
            throw new Error(`有効なLineCalc.exeの絶対パスを登録してください。\n${filename}`);
        }
        return filename;
    }

    async function registerExecutable() {
        requireWindows();
        const current = vscode.workspace.getConfiguration('linecalc').get('executablePath', '');
        const selected = await vscode.window.showOpenDialog({
            title: 'LineCalc.exeを登録', openLabel: '登録', canSelectMany: false,
            canSelectFiles: true, canSelectFolders: false, filters: { 'Windows実行ファイル': ['exe'] },
            ...(current && path.isAbsolute(current) ? { defaultUri: vscode.Uri.file(current) } : {})
        });
        if (!selected?.length) return undefined;
        if (selected[0].scheme !== 'file') throw new Error('ローカルの実行ファイルを選択してください。');
        const filename = await validateExecutable(selected[0].fsPath);
        await vscode.workspace.getConfiguration('linecalc').update('executablePath', filename, vscode.ConfigurationTarget.Global);
        vscode.window.showInformationMessage(`LineCalc.exeを登録しました: ${filename}`);
        return filename;
    }

    async function executable() {
        requireWindows();
        const configured = vscode.workspace.getConfiguration('linecalc').get('executablePath', '');
        if (!configured) return registerExecutable();
        return validateExecutable(configured);
    }

    async function saveActiveJavaScript() {
        let document = vscode.window.activeTextEditor?.document;
        if (!document) throw new Error('検証・実行するJavaScriptファイルをエディターで開いてください。');
        if (document.uri.scheme !== 'file' && document.uri.scheme !== 'untitled') {
            throw new Error('ローカルに保存された.jsファイルだけをLineCalcへ渡せます。');
        }
        if (document.isUntitled && document.languageId !== 'javascript') throw new Error('JavaScriptファイルを開いてください。');
        if (!document.isUntitled && path.extname(document.uri.fsPath).toLowerCase() !== '.js') {
            throw new Error('.jsファイルをアクティブにしてください。');
        }
        const wasUntitled = document.isUntitled;
        if ((document.isDirty || wasUntitled) && !(await document.save())) return undefined;
        if (wasUntitled) document = vscode.window.activeTextEditor?.document;
        if (!document) return undefined;
        if (document.uri.scheme !== 'file' || path.extname(document.uri.fsPath).toLowerCase() !== '.js') {
            throw new Error('.js拡張子のローカルファイルとして保存してください。');
        }
        if (!(await isFile(document.uri.fsPath))) throw new Error(`ファイルが見つかりません: ${document.uri.fsPath}`);
        return document.uri.fsPath;
    }

    function launch(exe, args, workingDirectory, mode) {
        output.appendLine(`${new Date().toLocaleString()} ${mode}: ${JSON.stringify([exe, ...args])}`);
        return new Promise((resolve, reject) => {
            let child;
            try {
                // シェルを経由せず、パスとオプションを独立した引数として渡す。
                child = spawnProcess(exe, args, { cwd: workingDirectory, shell: false, windowsHide: false, stdio: 'ignore' });
            } catch (error) { reject(error); return; }
            child.once('error', error => reject(new Error(`LineCalcを起動できません。\n${exe}\n${error.message}`)));
            child.once('exit', (code, signal) => {
                output.appendLine(`${mode}: 終了コード=${code}, signal=${signal || 'なし'}`);
                if (mode === 'debug') {
                    output.appendLine(code === 0 ? '検証 OK' : '検証失敗または中断。エラー位置はLineCalcの確認画面に表示されます。');
                }
                resolve(code);
            });
            child.unref();
        });
    }

    async function runActive(mode) {
        requireWindows();
        requireTrust();
        const filename = await saveActiveJavaScript();
        if (!filename) return;
        const key = filename.toLowerCase();
        if (mode === 'debug' && pendingDebug.has(key)) {
            vscode.window.showInformationMessage('このファイルは確認中です。LineCalcの結果画面を閉じてください。');
            return;
        }
        if (mode === 'debug') pendingDebug.add(key);
        try {
            const exe = await executable();
            if (!exe) return;
            requireTrust();
            if (!(await isFile(filename))) throw new Error(`ファイルが見つかりません: ${filename}`);
            return await launch(exe, [`-${mode}`, filename], path.dirname(filename), mode);
        } finally {
            if (mode === 'debug') pendingDebug.delete(key);
        }
    }

    async function openIncludeDir() {
        requireWindows();
        await fileSystem.mkdir(directory, { recursive: true });
        return vscode.commands.executeCommand('vscode.openFolder', vscode.Uri.file(directory), { forceNewWindow: true });
    }

    async function createAt(filename, exe) {
        requireTrust();
        if (await isFile(filename)) throw new Error(`既存ファイルは上書きしません。\n${filename}`);
        const code = await launch(exe, ['-createjs', filename], path.dirname(exe), 'createjs');
        if (code === 0 && await isFile(filename)) {
            const document = await vscode.workspace.openTextDocument(vscode.Uri.file(filename));
            await vscode.window.showTextDocument(document);
        }
        return code;
    }

    async function createJs() {
        requireWindows();
        requireTrust();
        const exe = await executable();
        if (!exe) return;
        const selected = await vscode.window.showSaveDialog({
            title: 'LineCalc定義ファイルの雛形を作成', saveLabel: '雛形を作成',
            defaultUri: vscode.Uri.file(defaultDefinition(exe, directory)), filters: { JavaScript: ['js'] }
        });
        if (!selected) return;
        if (selected.scheme !== 'file' || path.extname(selected.fsPath).toLowerCase() !== '.js') {
            throw new Error('ローカルの.jsファイルを指定してください。');
        }
        return createAt(selected.fsPath, exe);
    }

    async function openDefinition() {
        const exe = await executable();
        if (!exe) return;
        const filename = defaultDefinition(exe, directory);
        if (!(await isFile(filename))) {
            const choice = await vscode.window.showInformationMessage(`既定の定義ファイルがありません。\n${filename}`, '雛形を作成');
            if (choice === '雛形を作成') return createAt(filename, exe);
            return;
        }
        const document = await vscode.workspace.openTextDocument(vscode.Uri.file(filename));
        return vscode.window.showTextDocument(document);
    }

    async function start() {
        requireWindows();
        requireTrust();
        const exe = await executable();
        if (!exe) return;
        requireTrust();
        return launch(exe, [], path.dirname(exe), 'start');
    }

    return { open: start, openIncludeDir, debug: () => runActive('debug'), include: () => runActive('include'), registerExecutable, openDefinition, createJs, start };
}

module.exports = { defaultDirectory, defaultDefinition, createCommands };
