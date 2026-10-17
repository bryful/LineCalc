// 構文木を使い、計算機固有の初期値・数値制約・表示規則だけを追加する。
(() => {
    const parse = acorn.parse;
    const finite = Number.isFinite;
    const reserved = name => /^(M([1-9]|1[0-2])|ans|__lc.*)$/.test(name) ||
        ['resetJS','setTopmost','isTopmost','clearHis','clearMem','help','vars','funcs','mem','hex','bin',
         'fonts','setDisplayFont','getDisplayFont','chooseDisplayFont'].includes(name);
    Object.defineProperty(globalThis, '__lcNumber', {value(value) {
        if (typeof value !== 'number' || !finite(value))
            throw new TypeError('変数には有限の数値だけを代入できます。');
        return value;
    }});
    Object.defineProperty(globalThis, '__lcPrepare', {value(source) {
        const tokens = [];
        let tree;
        try {
            tree = parse(source, {ecmaVersion: 2024, onToken: tokens, locations: true});
        } catch (error) {
            if (error.loc) error.message = '行 ' + error.loc.line + '、列 ' + (error.loc.column+1) + '\n' + error.message;
            throw error;
        }
        const inserts = [];
        const names = new Set();
        const add = (at, text, end = at) => inserts.push({at, text, end, order: inserts.length});
        function binding(node) {
            if (node.type !== 'Identifier')
                throw new SyntaxError('行 '+node.loc.start.line+'、列 '+(node.loc.start.column+1)+'\n変数宣言には単一の変数名を指定してください。');
            if (reserved(node.name)) throw new SyntaxError('行 '+node.loc.start.line+'、列 '+(node.loc.start.column+1)+'\n'+node.name + ' は予約名です。');
        }
        function visit(node, functionDepth = 0, blockDepth = 0) {
            if (!node || typeof node !== 'object') return;
            if (node.type === 'FunctionDeclaration' || node.type === 'FunctionExpression' || node.type === 'ArrowFunctionExpression') {
                if (node.id) {
                    binding(node.id);
                    if (!functionDepth && !blockDepth) names.add(node.id.name);
                }
                node.params.forEach(binding);
                visit(node.body, functionDepth + 1, 0);
                return;
            }
            if (node.type === 'VariableDeclaration') {
                for (const item of node.declarations) {
                    binding(item.id);
                    if (!functionDepth && (!blockDepth || node.kind === 'var')) names.add(item.id.name);
                    if (item.init) {
                        add(item.init.start, '__lcNumber(');
                        add(item.init.end, ')');
                    } else {
                        add(item.id.end, '=0');
                    }
                }
            }
            if (node.type === 'AssignmentExpression' && node.left.type === 'Identifier') {
                const name = node.left.name;
                if (reserved(name) && !/^M([1-9]|1[0-2])$/.test(name))
                    throw new SyntaxError(name + ' は予約名です。');
                if (!functionDepth && !/^M([1-9]|1[0-2])$/.test(name)) names.add(name);
                if (!/^M([1-9]|1[0-2])$/.test(name)) {
                    if (['=','&&=','||=','??='].includes(node.operator)) {
                        add(node.right.start, '__lcNumber(');
                        add(node.right.end, ')');
                    } else {
                        const operator = node.operator.slice(0,-1);
                        const token = tokens.find(token => token.start >= node.left.end &&
                            token.end <= node.right.start && token.value === node.operator);
                        add(token.start, '=__lcNumber(' + name + operator + '(', token.end);
                        add(node.end, '))');
                    }
                }
            }
            if (node.type === 'UnaryExpression' && node.operator === 'delete' &&
                node.argument.type === 'Identifier' && reserved(node.argument.name))
                throw new SyntaxError('予約名は delete できません。');
            for (const key of Object.keys(node)) {
                if (['start','end','type'].includes(key)) continue;
                const child = node[key];
                const depth = blockDepth + (node.type === 'BlockStatement' ? 1 : 0);
                if (Array.isArray(child)) child.forEach(value => visit(value, functionDepth, depth));
                else if (child && typeof child === 'object') visit(child, functionDepth, depth);
            }
        }
        visit(tree);
        inserts.sort((a,b) => b.at-a.at || b.order-a.order);
        let code = source;
        for (const insert of inserts) code = code.slice(0,insert.at) + insert.text + code.slice(insert.end);
        const last = tree.body.filter(node => node.type !== 'EmptyStatement').at(-1);
        const suppress = !last || ['VariableDeclaration','FunctionDeclaration'].includes(last.type) ||
            (last.type === 'ExpressionStatement' && last.expression.type === 'AssignmentExpression' &&
             source.slice(last.expression.end,last.end).trim().endsWith(';'));
        function formatError(text, filename) {
            const generatedLines = code.split('\n');
            const originalLines = source.split('\n');
            const edits = [...inserts].reverse();
            return text.split(filename+':').map((part,index) => index === 0 ? part :
                part.replace(/^(\d+):(\d+)/, (_,line,column) => {
                    let offset = Number(column)-1;
                    for (let i=0; i<Number(line)-1; i++) offset += generatedLines[i].length+1;
                    let delta = 0;
                    let original = offset;
                    for (const edit of edits) {
                        const start = edit.at+delta;
                        if (offset < start) break;
                        if (offset < start+edit.text.length) { original=edit.at; break; }
                        delta += edit.text.length-(edit.end-edit.at);
                        original = offset-delta;
                    }
                    let row = 1;
                    for (const originalLine of originalLines) {
                        if (original <= originalLine.length) return row+':'+(original+1);
                        original -= originalLine.length+1;
                        row++;
                    }
                    return line+':'+column;
                })).join(filename+':');
        }
        return {code, names: [...names], suppress, formatError};
    }});
    delete globalThis.acorn;
})();
