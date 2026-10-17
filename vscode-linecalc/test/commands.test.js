'use strict';

const { test } = require('node:test');
const assert = require('node:assert/strict');
const { EventEmitter } = require('node:events');
const fs = require('node:fs/promises');
const path = require('node:path');
const { createCommands, defaultDirectory, defaultDefinition } = require('../src/commands');
const { builtins, variables, referenceMarkdown } = require('../src/catalog');

async function fixture(t) {
    const root = await fs.mkdtemp(path.join(__dirname, '.test-files-'));
    t.after(() => fs.rm(root, { recursive: true, force: true }));
    const exe = path.join(root, '登録 exe & 日本語.exe');
    const filename = path.join(root, '計算 & (draft).js');
    const directory = path.join(root, 'Local', 'LineCalc');
    await fs.writeFile(exe, 'test fixture — never executed');
    await fs.writeFile(filename, 'function triple(v){ return v*3; }');
    const calls = { spawns: [], info: [], open: [], updates: [], saved: 0, logs: [] };
    let configured = exe;
    const uri = value => ({ scheme: 'file', fsPath: value });
    const document = {
        uri: uri(filename), isDirty: true, isUntitled: false, languageId: 'javascript',
        async save() { calls.saved++; document.isDirty = false; return true; }
    };
    const vscode = {
        Uri: { file: uri }, ConfigurationTarget: { Global: 1 },
        workspace: {
            isTrusted: true,
            getConfiguration() {
                return { get: () => configured, update: async (...args) => { calls.updates.push(args); configured = args[1]; } };
            },
            async openTextDocument(value) { calls.open.push(value); return { uri: value }; }
        },
        window: {
            activeTextEditor: { document },
            showOpenDialog: async () => [uri(exe)],
            showSaveDialog: async () => uri(path.join(directory, 'new.js')),
            showInformationMessage: async (...args) => { calls.info.push(args); return undefined; },
            showTextDocument: async value => value
        },
        commands: { executeCommand: async (...args) => { calls.open.push(args); } }
    };
    let nextError;
    let nextCode = 0;
    let autoExit = true;
    const children = [];
    function spawn(executable, args, options) {
        calls.spawns.push({ executable, args, options });
        const child = new EventEmitter();
        child.unref = () => {};
        children.push(child);
        setImmediate(async () => {
            if (nextError) { child.emit('error', nextError); return; }
            if (args[0] === '-createjs' && nextCode === 0) {
                await fs.mkdir(path.dirname(args[1]), { recursive: true });
                await fs.writeFile(args[1], '// created template');
            }
            if (autoExit) child.emit('exit', nextCode, null);
        });
        return child;
    }
    const commands = createCommands(vscode, { appendLine: text => calls.logs.push(text) }, { spawn, directory, platform: 'win32' });
    return {
        root, exe, filename, directory, document, vscode, calls, commands, children,
        configure(value) { configured = value; },
        failSpawn(error) { nextError = error; },
        exitCode(code) { nextCode = code; },
        keepAlive() { autoExit = false; }
    };
}

test('default folder and executable-derived file name', () => {
    assert.equal(defaultDirectory({ LOCALAPPDATA: path.join('C:', 'User', 'Local') }), path.join('C:', 'User', 'Local', 'LineCalc'));
    assert.equal(defaultDirectory({}, 'user-home'), path.join('user-home', 'AppData', 'Local', 'LineCalc'));
    assert.equal(defaultDefinition('Renamed.EXE', 'settings'), path.join('settings', 'Renamed.js'));
});

test('debug saves the active file and passes Unicode/metacharacter paths without a shell', async t => {
    const f = await fixture(t);
    assert.equal(await f.commands.debug(), 0);
    assert.equal(f.calls.saved, 1);
    assert.deepEqual(f.calls.spawns[0], {
        executable: f.exe, args: ['-debug', f.filename],
        options: { cwd: path.dirname(f.filename), shell: false, windowsHide: false, stdio: 'ignore' }
    });
    assert.ok(f.calls.logs.includes('検証 OK'));
});

test('include uses the active file directory for relative paths', async t => {
    const f = await fixture(t);
    await f.commands.include();
    assert.deepEqual(f.calls.spawns[0].args, ['-include', f.filename]);
    assert.equal(f.calls.spawns[0].options.cwd, path.dirname(f.filename));
});

test('no editor, wrong extension and remote documents never launch', async t => {
    const f = await fixture(t);
    f.vscode.window.activeTextEditor = undefined;
    await assert.rejects(f.commands.debug(), /エディターで開いて/);
    f.vscode.window.activeTextEditor = { document: f.document };
    f.document.uri = { scheme: 'vscode-remote', fsPath: f.filename };
    await assert.rejects(f.commands.debug(), /ローカル/);
    f.document.uri = { scheme: 'file', fsPath: f.filename + '.txt' };
    await assert.rejects(f.commands.include(), /\.js/);
    assert.equal(f.calls.spawns.length, 0);
});

test('cancelled save and cancelled executable registration never launch', async t => {
    const f = await fixture(t);
    f.document.save = async () => false;
    await f.commands.debug();
    assert.equal(f.calls.spawns.length, 0);
    f.document.isDirty = false;
    f.configure('');
    f.vscode.window.showOpenDialog = async () => undefined;
    await f.commands.include();
    assert.equal(f.calls.spawns.length, 0);
});

test('untitled JavaScript must be saved locally with .js extension', async t => {
    const f = await fixture(t);
    f.document.isUntitled = true;
    f.document.uri = { scheme: 'untitled', fsPath: '' };
    f.document.save = async () => {
        f.vscode.window.activeTextEditor = { document: {
            ...f.document, uri: { scheme: 'file', fsPath: f.filename }, isUntitled: false, isDirty: false
        } };
        return true;
    };
    await f.commands.debug();
    assert.deepEqual(f.calls.spawns[0].args, ['-debug', f.filename]);
});

test('register saves the absolute executable path in user settings', async t => {
    const f = await fixture(t);
    assert.equal(await f.commands.registerExecutable(), f.exe);
    assert.deepEqual(f.calls.updates, [['executablePath', f.exe, 1]]);
});

test('invalid executable or missing active file is rejected', async t => {
    const f = await fixture(t);
    f.configure('relative.exe');
    await assert.rejects(f.commands.debug(), /絶対パス/);
    f.configure(path.join(f.root, 'absent.exe'));
    await assert.rejects(f.commands.include(), /絶対パス/);
    f.configure(f.exe);
    await fs.unlink(f.filename);
    await assert.rejects(f.commands.debug(), /見つかりません/);
    assert.equal(f.calls.spawns.length, 0);
});

test('workspace trust is checked before saving or launching code', async t => {
    const f = await fixture(t);
    f.vscode.workspace.isTrusted = false;
    for (const name of ['open', 'debug', 'include', 'createJs', 'start']) await assert.rejects(f.commands[name](), /信頼/);
    assert.equal(f.calls.saved, 0);
    assert.equal(f.calls.spawns.length, 0);
});

test('OpenIncludeDir creates the standard directory and opens a new VS Code window', async t => {
    const f = await fixture(t);
    await f.commands.openIncludeDir();
    assert.ok((await fs.stat(f.directory)).isDirectory());
    assert.deepEqual(f.calls.open[0], ['vscode.openFolder', { scheme: 'file', fsPath: f.directory }, { forceNewWindow: true }]);
});

test('OpenDefinition respects renamed executable names', async t => {
    const f = await fixture(t);
    const filename = defaultDefinition(f.exe, f.directory);
    await fs.mkdir(f.directory, { recursive: true });
    await fs.writeFile(filename, '// existing');
    await f.commands.openDefinition();
    assert.equal(f.calls.open[0].fsPath, filename);
    assert.equal(f.calls.spawns.length, 0);
});

test('CreateJS uses LineCalc and opens the resulting file', async t => {
    const f = await fixture(t);
    await f.commands.createJs();
    assert.deepEqual(f.calls.spawns[0].args, ['-createjs', path.join(f.directory, 'new.js')]);
    assert.equal(f.calls.open[0].fsPath, path.join(f.directory, 'new.js'));
});

test('CreateJS preserves an existing file', async t => {
    const f = await fixture(t);
    f.vscode.window.showSaveDialog = async () => ({ scheme: 'file', fsPath: f.filename });
    const before = await fs.readFile(f.filename);
    await assert.rejects(f.commands.createJs(), /上書きしません/);
    assert.deepEqual(await fs.readFile(f.filename), before);
    assert.equal(f.calls.spawns.length, 0);
});

test('missing default definition can be created through the offered action', async t => {
    const f = await fixture(t);
    f.vscode.window.showInformationMessage = async () => '雛形を作成';
    await f.commands.openDefinition();
    assert.deepEqual(f.calls.spawns[0].args, ['-createjs', defaultDefinition(f.exe, f.directory)]);
});

test('Open launches without arguments while editing other code and Start remains an alias', async t => {
    const f = await fixture(t);
    f.document.uri.fsPath = path.join(f.root, 'unrelated.cpp');
    f.document.languageId = 'cpp';
    await f.commands.open();
    await f.commands.start();
    assert.deepEqual(f.calls.spawns[0].args, []);
    assert.deepEqual(f.calls.spawns[1].args, []);
    assert.equal(f.calls.spawns[0].options.cwd, path.dirname(f.exe));
    assert.equal(f.calls.saved, 0);
});

test('process startup failures propagate and debug failure is logged', async t => {
    const f = await fixture(t);
    f.failSpawn(new Error('access denied'));
    await assert.rejects(f.commands.debug(), /access denied/);
    const g = await fixture(t);
    g.exitCode(1);
    assert.equal(await g.commands.debug(), 1);
    assert.ok(g.calls.logs.some(line => line.includes('検証失敗')));
});

test('duplicate debug is prevented while the first result dialog is open', async t => {
    const f = await fixture(t);
    f.keepAlive();
    const first = f.commands.debug();
    while (f.children.length === 0) await new Promise(resolve => setImmediate(resolve));
    await f.commands.debug();
    assert.equal(f.calls.spawns.length, 1);
    f.children[0].emit('exit', 0, null);
    await first;
});

test('catalog covers all native functions and all memory variables', async () => {
    const cpp = await fs.readFile(path.join(__dirname, '../../LineCalc/Calculator.cpp'), 'utf8');
    const nativeLine = cpp.match(/NATIVE_NAMES\[\]\s*=\s*\{([^}]+)\}/)[1];
    const names = [...nativeLine.matchAll(/"([^"]+)"/g)].map(match => match[1]);
    assert.deepEqual([...builtins.map(item => item.name)].sort(), names.sort());
    assert.equal(variables.length, 13);
    assert.ok(referenceMarkdown().includes('定義ファイル内では実行できません'));
    assert.ok(referenceMarkdown().includes('M12'));
});
