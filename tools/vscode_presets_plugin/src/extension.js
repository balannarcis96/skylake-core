/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2025 Balan Narcis (balannarcis96@gmail.com)
 *
 * Extension entry point.
 */
'use strict';

const vscode = require('vscode');

const { WorkspaceIndex } = require('./core/workspaceIndex');
const { PresetEditorProvider } = require('./providers/presetEditor');
const { DiagnosticsProvider } = require('./providers/diagnostics');
const { TuningTreeProvider } = require('./providers/tree');
const { registerCommands } = require('./commands');

/** @param {vscode.ExtensionContext} context */
async function activate(context) {
    const index = new WorkspaceIndex();
    context.subscriptions.push(index);

    const editor = PresetEditorProvider.register(context, index);
    const diagnostics = new DiagnosticsProvider(context, index);
    context.subscriptions.push(diagnostics);

    const tree = new TuningTreeProvider(index);
    context.subscriptions.push(
        tree,
        vscode.window.registerTreeDataProvider('skylakeTuning.tree', tree)
    );

    registerCommands(context, index, editor);

    // Re-index on any change to a preset file or a CMakeLists.txt. The scan is
    // cheap (tens of small files) and debounced, so a full rebuild beats trying
    // to patch the index and risking a stale cross-file resolution.
    const watcher = vscode.workspace.createFileSystemWatcher('**/{*_preset.json,*_presets.json,CMakeLists.txt}');
    const schedule = debounce(() => index.refresh(), 250);
    context.subscriptions.push(
        watcher,
        watcher.onDidChange(schedule),
        watcher.onDidCreate(schedule),
        watcher.onDidDelete(schedule)
    );

    // Saving through the raw text editor must refresh the cross-file view too.
    context.subscriptions.push(
        vscode.workspace.onDidSaveTextDocument((document) => {
            if (/(_presets?\.json|CMakeLists\.txt)$/i.test(document.uri.fsPath)) {
                schedule();
            }
        })
    );

    context.subscriptions.push(
        vscode.workspace.onDidChangeConfiguration((event) => {
            if (event.affectsConfiguration('skylakeTuning')) {
                schedule();
            }
        }),
        vscode.workspace.onDidChangeWorkspaceFolders(() => schedule())
    );

    await index.refresh();
}

function deactivate() {
    /* Everything is disposed through context.subscriptions. */
}

/**
 * @param {() => void} fn
 * @param {number} delay
 */
function debounce(fn, delay) {
    /** @type {NodeJS.Timeout|undefined} */
    let handle;
    return () => {
        if (handle) {
            clearTimeout(handle);
        }
        handle = setTimeout(() => {
            handle = undefined;
            fn();
        }, delay);
    };
}

module.exports = { activate, deactivate };
