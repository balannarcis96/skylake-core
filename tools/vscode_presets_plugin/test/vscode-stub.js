/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2025 Balan Narcis (balannarcis96@gmail.com)
 *
 * Minimal stand-in for the `vscode` module.
 *
 * Enough of the API surface to drive the index, the editor model, the
 * diagnostics and the tree outside the editor host, so the integration test can
 * run against a real preset tree from the command line.
 */
'use strict';

const fs = require('fs');
const path = require('path');
const Module = require('module');

class EventEmitter {
    constructor() {
        this._handlers = [];
        this.event = (handler) => {
            this._handlers.push(handler);
            return { dispose: () => {
                const at = this._handlers.indexOf(handler);
                if (at >= 0) {
                    this._handlers.splice(at, 1);
                }
            } };
        };
    }

    fire(value) {
        for (const handler of [...this._handlers]) {
            handler(value);
        }
    }

    dispose() {
        this._handlers.length = 0;
    }
}

class Uri {
    constructor(fsPath) {
        this.fsPath = fsPath;
        this.scheme = 'file';
        this.path = fsPath;
    }

    static file(fsPath) {
        return new Uri(fsPath);
    }

    static joinPath(base, ...parts) {
        return new Uri(path.join(base.fsPath, ...parts));
    }

    static parse(value) {
        const uri = new Uri(value);
        uri.scheme = value.split(':')[0];
        return uri;
    }

    toString() {
        return `file://${this.fsPath}`;
    }
}

class Position {
    constructor(line, character) {
        this.line = line;
        this.character = character;
    }
}

class Range {
    constructor(start, end) {
        this.start = start;
        this.end = end;
    }
}

class Location {
    constructor(uri, range) {
        this.uri = uri;
        this.range = range;
    }
}

class Diagnostic {
    constructor(range, message, severity) {
        this.range = range;
        this.message = message;
        this.severity = severity;
    }
}

class DiagnosticRelatedInformation {
    constructor(location, message) {
        this.location = location;
        this.message = message;
    }
}

class MarkdownString {
    constructor(value) {
        this.value = value;
    }
}

class ThemeIcon {
    constructor(id) {
        this.id = id;
    }
}

class TreeItem {
    constructor(label, collapsibleState) {
        this.label = label;
        this.collapsibleState = collapsibleState;
    }
}

/**
 * @param {string[]} roots Absolute workspace folder paths.
 */
function install(roots) {
    const folders = roots.map((root) => ({
        uri: Uri.file(root),
        name: path.basename(root),
        index: 0
    }));

    /** @type {Map<string, any>} */
    const config = new Map();

    /**
     * Very small glob matcher covering the patterns this extension actually
     * uses: `**` , `*` and `{a,b}` alternation.
     * @param {string} pattern
     */
    const globToRegExp = (pattern) => {
        let out = '';
        for (let i = 0; i < pattern.length; i++) {
            const ch = pattern[i];
            if (ch === '*') {
                if (pattern[i + 1] === '*') {
                    out += '.*';
                    i++;
                    if (pattern[i + 1] === '/') {
                        i++;
                    }
                } else {
                    out += '[^/]*';
                }
            } else if (ch === '{') {
                const close = pattern.indexOf('}', i);
                const options = pattern.slice(i + 1, close).split(',');
                out += `(${options.map((option) => option.replace(/[.*+?^${}()|[\]\\]/g, '\\$&')).join('|')})`;
                i = close;
            } else if ('.+?^$()|[]\\'.includes(ch)) {
                out += `\\${ch}`;
            } else {
                out += ch;
            }
        }
        return new RegExp(`^${out}$`);
    };

    const walk = (dir, out = []) => {
        let items;
        try {
            items = fs.readdirSync(dir, { withFileTypes: true });
        } catch {
            return out;
        }
        for (const item of items) {
            const full = path.join(dir, item.name);
            if (item.isDirectory()) {
                if (['.git', 'node_modules', 'out'].includes(item.name)) {
                    continue;
                }
                walk(full, out);
            } else {
                out.push(full);
            }
        }
        return out;
    };

    const vscode = {
        EventEmitter,
        Uri,
        Position,
        Range,
        Location,
        Diagnostic,
        DiagnosticRelatedInformation,
        MarkdownString,
        ThemeIcon,
        TreeItem,
        DiagnosticSeverity: { Error: 0, Warning: 1, Information: 2, Hint: 3 },
        TreeItemCollapsibleState: { None: 0, Collapsed: 1, Expanded: 2 },
        ViewColumn: { Beside: -2 },
        TextEditorRevealType: { InCenter: 2 },

        workspace: {
            workspaceFolders: folders,
            textDocuments: [],
            getConfiguration(section) {
                return {
                    get(key, fallback) {
                        const full = `${section}.${key}`;
                        return config.has(full) ? config.get(full) : fallback;
                    }
                };
            },
            async findFiles(include, exclude) {
                const matcher = globToRegExp(include);
                const excluder = exclude ? globToRegExp(exclude) : undefined;
                /** @type {Uri[]} */
                const out = [];
                for (const folder of folders) {
                    for (const file of walk(folder.uri.fsPath)) {
                        const relative = path.relative(folder.uri.fsPath, file).split(path.sep).join('/');
                        if (!matcher.test(relative)) {
                            continue;
                        }
                        if (excluder && excluder.test(relative)) {
                            continue;
                        }
                        out.push(Uri.file(file));
                    }
                }
                return out;
            },
            fs: {
                async readFile(uri) {
                    return fs.readFileSync(uri.fsPath);
                }
            },
            getWorkspaceFolder(uri) {
                return folders.find((folder) => uri.fsPath.startsWith(folder.uri.fsPath));
            },
            createFileSystemWatcher() {
                const noop = () => ({ dispose() {} });
                return { onDidChange: noop, onDidCreate: noop, onDidDelete: noop, dispose() {} };
            },
            onDidSaveTextDocument: () => ({ dispose() {} }),
            onDidChangeTextDocument: () => ({ dispose() {} }),
            onDidChangeConfiguration: () => ({ dispose() {} }),
            onDidChangeWorkspaceFolders: () => ({ dispose() {} }),
            registerTextDocumentContentProvider: () => ({ dispose() {} }),
            async openTextDocument() {
                throw new Error('not supported in the stub');
            },
            async applyEdit() {
                return true;
            }
        },

        languages: {
            createDiagnosticCollection() {
                /** @type {Map<string, any[]>} */
                const store = new Map();
                return {
                    set(uri, diagnostics) {
                        store.set(uri.fsPath, diagnostics);
                    },
                    clear() {
                        store.clear();
                    },
                    dispose() {
                        store.clear();
                    },
                    get all() {
                        return store;
                    }
                };
            }
        },

        window: {
            showErrorMessage() {},
            showWarningMessage() {},
            showInformationMessage() {},
            setStatusBarMessage() {},
            registerTreeDataProvider: () => ({ dispose() {} }),
            registerCustomEditorProvider: () => ({ dispose() {} })
        },

        commands: {
            registerCommand: () => ({ dispose() {} }),
            executeCommand: async () => undefined
        },

        env: { clipboard: { async writeText() {} } },

        _setConfig(key, value) {
            config.set(key, value);
        }
    };

    // Route `require('vscode')` to the stub.
    const original = Module._resolveFilename;
    Module._resolveFilename = function (request, ...rest) {
        if (request === 'vscode') {
            return 'vscode';
        }
        return original.call(this, request, ...rest);
    };
    require.cache.vscode = /** @type {any} */ ({
        id: 'vscode',
        filename: 'vscode',
        loaded: true,
        exports: vscode
    });

    return vscode;
}

module.exports = { install };
