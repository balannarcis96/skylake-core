/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2025 Balan Narcis (balannarcis96@gmail.com)
 *
 * Custom text editor for tuning preset documents.
 *
 * Backed by the real TextDocument rather than a private model, so undo/redo,
 * dirty state, save, git decorations and "reopen as text" all behave the way
 * the rest of the editor does. Every mutation is a single whole-document
 * replacement; the style-preserving writer is what keeps that from turning
 * into a whole-file diff.
 */
'use strict';

const path = require('path');
const vscode = require('vscode');

const { parseTree, propertyAtPath, JsonParseError } = require('../core/jsonAst');
const { stringify, detectStyle } = require('../core/serialize');
const { buildIndexedFile, normalizeId } = require('../core/workspaceIndex');
const { buildEditorModel } = require('../core/editorModel');
const mutations = require('../core/mutations');
const { OUTPUTS, DEFINES_GROUP, CONSTEXPR_GROUP } = require('../core/schema');

const VIEW_TYPE = 'skylakeTuning.presetEditor';

class PresetEditorProvider {
    /**
     * @param {vscode.ExtensionContext} context
     * @param {import('../core/workspaceIndex').WorkspaceIndex} index
     */
    constructor(context, index) {
        this.context = context;
        this.index = index;
        /** @type {Map<string, {panel: vscode.WebviewPanel, document: vscode.TextDocument}>} */
        this.active = new Map();
        // Explicit undo history per document. A webview swallows Ctrl+Z, so the
        // text document's own stack is unreachable from here; keeping snapshots
        // is what makes every mutation reversible in one click.
        /** @type {Map<string, {undo: string[], redo: string[]}>} */
        this.history = new Map();
        /** @type {{docUri: string, presetIndex: number, configIndex: number, group: string, key: string}|undefined} */
        this.lastContext = undefined;

        this._indexSubscription = index.onDidChange(() => this._refreshAll());
    }

    dispose() {
        this._indexSubscription.dispose();
    }

    /**
     * @param {vscode.ExtensionContext} context
     * @param {import('../core/workspaceIndex').WorkspaceIndex} index
     */
    static register(context, index) {
        const provider = new PresetEditorProvider(context, index);
        const registration = vscode.window.registerCustomEditorProvider(VIEW_TYPE, provider, {
            webviewOptions: { retainContextWhenHidden: true },
            supportsMultipleEditorsPerDocument: false
        });
        context.subscriptions.push(registration, provider);
        return provider;
    }

    /**
     * @param {vscode.TextDocument} document
     * @param {vscode.WebviewPanel} panel
     */
    async resolveCustomTextEditor(document, panel) {
        const key = document.uri.toString();
        this.active.set(key, { panel, document });

        panel.webview.options = {
            enableScripts: true,
            localResourceRoots: [vscode.Uri.joinPath(this.context.extensionUri, 'media')]
        };
        panel.webview.html = this._html(panel.webview);

        const post = () => this._post(document, panel);

        const changeSubscription = vscode.workspace.onDidChangeTextDocument((event) => {
            if (event.document.uri.toString() === key) {
                post();
            }
        });

        panel.webview.onDidReceiveMessage((message) => this._onMessage(document, panel, message));

        panel.onDidDispose(() => {
            changeSubscription.dispose();
            this.active.delete(key);
            this.history.delete(key);
        });

        post();
    }

    _refreshAll() {
        for (const { panel, document } of this.active.values()) {
            this._post(document, panel);
        }
    }

    /**
     * @param {vscode.TextDocument} document
     * @param {vscode.WebviewPanel} panel
     */
    _post(document, panel) {
        const model = this.modelFor(document);
        const history = this.history.get(document.uri.toString());
        panel.webview.postMessage({
            type: 'model',
            model,
            docUri: document.uri.toString(),
            dirty: document.isDirty,
            canUndo: (history?.undo.length ?? 0) > 0,
            canRedo: (history?.redo.length ?? 0) > 0
        });
    }

    /**
     * Replace a document's entire text in one edit.
     * @param {vscode.TextDocument} document
     * @param {string} next
     */
    async _replaceAll(document, next) {
        const edit = new vscode.WorkspaceEdit();
        edit.replace(
            document.uri,
            new vscode.Range(document.positionAt(0), document.positionAt(document.getText().length)),
            next
        );
        return vscode.workspace.applyEdit(edit);
    }

    /** @param {vscode.TextDocument} document */
    async undo(document) {
        const key = document.uri.toString();
        const history = this.history.get(key);
        if (!history || history.undo.length === 0) {
            return false;
        }
        const previous = history.undo.pop();
        history.redo.push(document.getText());
        return this._replaceAll(document, /** @type {string} */ (previous));
    }

    /** @param {vscode.TextDocument} document */
    async redo(document) {
        const key = document.uri.toString();
        const history = this.history.get(key);
        if (!history || history.redo.length === 0) {
            return false;
        }
        const next = history.redo.pop();
        history.undo.push(document.getText());
        return this._replaceAll(document, /** @type {string} */ (next));
    }

    /**
     * Build the view model from the document's CURRENT text, overlaying it on
     * the workspace index so unsaved edits are reflected immediately.
     * @param {vscode.TextDocument} document
     */
    modelFor(document) {
        const id = normalizeId(document.uri.fsPath);
        const live = buildIndexedFile(document.uri, id, document.getText());

        if (!live) {
            return {
                ok: false,
                fileName: path.basename(document.uri.fsPath),
                relPath: document.uri.fsPath,
                parseError: {
                    message:
                        'This file is not a Skylake tuning preset document ' +
                        '(expected a top-level "presets" array whose entries hold "config" blocks with a "target_name").',
                    offset: 0
                }
            };
        }

        // Swap the on-disk copy for the in-editor one so overrides/inheritance
        // are computed against what the user is actually looking at.
        const saved = this.index.files.get(id);
        this.index.files.set(id, live);
        try {
            return buildEditorModel(this.index, live);
        } finally {
            if (saved) {
                this.index.files.set(id, saved);
            } else {
                this.index.files.delete(id);
            }
        }
    }

    /**
     * Parse the document, apply `mutate` to the raw JSON, write it back.
     * @param {vscode.TextDocument} document
     * @param {(raw: any) => any} mutate
     * @param {string} label
     */
    async applyEdit(document, mutate, label) {
        const text = document.getText();
        let raw;
        try {
            raw = parseTree(text).value;
        } catch (error) {
            const detail = error instanceof JsonParseError ? error.message : String(error);
            vscode.window.showErrorMessage(`Cannot edit: the file is not valid JSON (${detail}).`);
            return false;
        }

        const next = mutate(raw);
        if (next === undefined) {
            return false;
        }

        const style = detectStyle(text);
        const serialized = stringify(next, style);
        if (serialized === text) {
            return false;
        }

        const applied = await this._replaceAll(document, serialized);
        if (!applied) {
            vscode.window.showErrorMessage(`Failed to apply: ${label}`);
            return false;
        }

        // Record the pre-edit snapshot so the change stays one click away from
        // being reverted. Bounded so a long session cannot grow without limit.
        const key = document.uri.toString();
        const history = this.history.get(key) ?? { undo: [], redo: [] };
        history.undo.push(text);
        if (history.undo.length > 100) {
            history.undo.shift();
        }
        history.redo.length = 0;
        this.history.set(key, history);

        const entry = this.active.get(key);
        if (entry) {
            entry.panel.webview.postMessage({ type: 'applied', label });
        }
        return applied;
    }

    /**
     * @param {vscode.TextDocument} document
     * @param {vscode.WebviewPanel} panel
     * @param {*} message
     */
    async _onMessage(document, panel, message) {
        switch (message?.type) {
            case 'ready':
                this._post(document, panel);
                return;

            case 'context':
                // Cached so the `webview/context` menu commands, which arrive
                // through the command palette rather than the webview channel,
                // know which row was clicked.
                this.lastContext = { docUri: document.uri.toString(), ...message.context };
                return;

            case 'setHeader':
                await this.applyEdit(
                    document,
                    (raw) => mutations.setHeader(raw, message.patch),
                    'change file header'
                );
                return;

            case 'setPresetName':
                await this.applyEdit(
                    document,
                    (raw) => mutations.setPresetName(raw, message.presetIndex, message.value),
                    'rename preset'
                );
                return;

            case 'setConfigField':
                await this.applyEdit(
                    document,
                    (raw) =>
                        mutations.setConfigField(
                            raw,
                            message.presetIndex,
                            message.configIndex,
                            message.field,
                            message.value === '' && message.field !== 'constexpr_namespace'
                                ? undefined
                                : message.value
                        ),
                    'change config field'
                );
                return;

            case 'setValue':
                await this.applyEdit(
                    document,
                    (raw) =>
                        mutations.setEntryValue(
                            raw,
                            message.presetIndex,
                            message.configIndex,
                            message.group,
                            message.key,
                            message.value
                        ),
                    'change value'
                );
                return;

            case 'setMeta':
                await this.applyEdit(
                    document,
                    (raw) =>
                        mutations.setEntryMeta(
                            raw,
                            message.presetIndex,
                            message.configIndex,
                            message.group,
                            message.key,
                            message.patch
                        ),
                    'change metadata'
                );
                return;

            case 'deleteEntry':
                await this.deleteEntry(document, message);
                return;

            case 'addEntry':
                await this.promptAddEntry(document, message.presetIndex, message.configIndex, message.group);
                return;

            case 'addGroup':
                await this.promptAddGroup(document, message.presetIndex, message.configIndex);
                return;

            case 'deleteGroup':
                await this.promptDeleteGroup(document, message);
                return;

            case 'addConfig':
                await this.promptAddConfig(document, message.presetIndex);
                return;

            case 'deleteConfig':
                await this.promptDeleteConfig(document, message);
                return;

            case 'addPreset':
                await this.promptAddPreset(document);
                return;

            case 'renamePreset':
                await this.promptRenamePreset(document, message);
                return;

            case 'deletePreset':
                await this.promptDeletePreset(document, message);
                return;

            case 'adoptInherited':
                await this.applyEdit(
                    document,
                    (raw) =>
                        mutations.addEntry(
                            raw,
                            message.presetIndex,
                            message.configIndex,
                            message.group || CONSTEXPR_GROUP,
                            message.key,
                            mutations.makeOverrideEntry(message.value)
                        ),
                    'override inherited key'
                );
                return;

            case 'revealInText':
                await this.revealPath(document, message.path);
                return;

            case 'openFile': {
                const uri = vscode.Uri.file(message.fileId);
                await vscode.commands.executeCommand('vscode.open', uri);
                return;
            }

            case 'undo':
                await this.undo(document);
                return;

            case 'redo':
                await this.redo(document);
                return;

            case 'save':
                await document.save();
                return;

            case 'reopenAsText':
                await vscode.commands.executeCommand('workbench.action.reopenTextEditor', document.uri);
                return;

            case 'command':
                await vscode.commands.executeCommand(message.command, ...(message.args ?? []));
                return;

            default:
                return;
        }
    }

    // -- prompts ------------------------------------------------------------

    /**
     * @param {vscode.TextDocument} document
     * @param {{presetIndex: number, configIndex: number, group: string, key: string}} target
     */
    async deleteEntry(document, target) {
        const choice = await vscode.window.showWarningMessage(
            `Delete '${target.key}' from ${target.group}?`,
            { modal: true },
            'Delete'
        );
        if (choice !== 'Delete') {
            return;
        }
        await this.applyEdit(
            document,
            (raw) =>
                mutations.deleteEntry(
                    raw,
                    target.presetIndex,
                    target.configIndex,
                    target.group,
                    target.key
                ),
            'delete entry'
        );
    }

    /**
     * @param {vscode.TextDocument} document
     * @param {number} presetIndex
     * @param {number} configIndex
     * @param {string} group
     */
    async promptAddEntry(document, presetIndex, configIndex, group) {
        const model = this.modelFor(document);
        const config = model?.presets?.[presetIndex]?.configs?.[configIndex];
        if (!config) {
            return;
        }

        const inheritedNames = new Set(config.inheritedOnly.map((entry) => entry.key));
        /** @type {vscode.QuickPickItem[]} */
        const items = [
            {
                label: '$(add) New key…',
                detail: 'Declare a key that does not exist in any lower-priority layer',
                alwaysShow: true
            },
            ...config.inheritedOnly.map((entry) => ({
                label: entry.key,
                description: entry.value,
                detail: `inherited from ${entry.file} — override it here`
            }))
        ];

        const picked = await vscode.window.showQuickPick(items, {
            title: `Add to ${group} — target '${config.targetName}'`,
            placeHolder: 'Pick an inherited key to override, or declare a new one',
            matchOnDescription: true,
            matchOnDetail: true
        });
        if (!picked) {
            return;
        }

        if (inheritedNames.has(picked.label)) {
            const source = config.inheritedOnly.find((entry) => entry.key === picked.label);
            const value = await vscode.window.showInputBox({
                title: `Override ${picked.label}`,
                prompt: `Inherited value from ${source.file}`,
                value: source.value
            });
            if (value === undefined) {
                return;
            }
            await this.applyEdit(
                document,
                (raw) =>
                    mutations.addEntry(
                        raw,
                        presetIndex,
                        configIndex,
                        group,
                        picked.label,
                        mutations.makeOverrideEntry(value)
                    ),
                'add override'
            );
            return;
        }

        const kind = group === DEFINES_GROUP ? 'define' : 'constexpr';
        const key = await vscode.window.showInputBox({
            title: `New ${kind} in ${group}`,
            prompt: 'Key name',
            validateInput: (candidate) => {
                if (!/^[A-Za-z_][A-Za-z0-9_]*$/.test(candidate)) {
                    return 'Must be a valid C identifier';
                }
                const existing = config.groups.some((bucket) =>
                    bucket.entries.some((entry) => entry.key === candidate)
                );
                return existing ? 'Already declared in this config' : undefined;
            }
        });
        if (!key) {
            return;
        }

        const value = await vscode.window.showInputBox({
            title: `New ${kind} ${key}`,
            prompt: 'Value, written verbatim into the generated header',
            placeHolder: kind === 'constexpr' ? '(1U << 16U)' : '1'
        });
        if (value === undefined) {
            return;
        }

        let type;
        if (kind === 'constexpr') {
            type = await vscode.window.showInputBox({
                title: `Type for ${key}`,
                prompt: 'Required — a new constexpr without a type makes the generator exit with -1',
                value: 'u32',
                validateInput: (candidate) => (candidate.trim() ? undefined : 'A type is required')
            });
            if (type === undefined) {
                return;
            }
        }

        const desc = await vscode.window.showInputBox({
            title: `Description for ${key}`,
            prompt: 'Rendered as the comment above the entry (optional)',
            placeHolder: '[Tune] …'
        });
        if (desc === undefined) {
            return;
        }

        await this.applyEdit(
            document,
            (raw) =>
                mutations.addEntry(
                    raw,
                    presetIndex,
                    configIndex,
                    group,
                    key,
                    mutations.makeNewEntry({ value, type, desc, kind })
                ),
            'add entry'
        );
    }

    /**
     * @param {vscode.TextDocument} document
     * @param {number} presetIndex
     * @param {number} configIndex
     */
    async promptAddGroup(document, presetIndex, configIndex) {
        const model = this.modelFor(document);
        const existing = new Set(
            model?.presets?.[presetIndex]?.configs?.[configIndex]?.groups.map((group) => group.name) ?? []
        );
        const suggestions = (model?.groupSuggestions ?? []).filter((name) => !existing.has(name));

        const picked = await vscode.window.showQuickPick(
            [
                { label: '$(add) Custom group…', alwaysShow: true },
                ...suggestions.map((name) => ({
                    label: name,
                    description: name === DEFINES_GROUP ? 'preprocessor defines' : 'constexpr bucket'
                }))
            ],
            {
                title: 'Add group',
                placeHolder: 'Groups named constexprs.<suffix> are cosmetic — they all flatten into one table'
            }
        );
        if (!picked) {
            return;
        }

        let group = picked.label;
        if (group.startsWith('$(add)')) {
            const suffix = await vscode.window.showInputBox({
                title: 'New constexpr group',
                prompt: 'Suffix appended to "constexprs." — presentation only',
                placeHolder: 'libsql',
                validateInput: (candidate) =>
                    /^[A-Za-z0-9_]+$/.test(candidate) ? undefined : 'Use letters, digits or underscores'
            });
            if (!suffix) {
                return;
            }
            group = `${CONSTEXPR_GROUP}.${suffix}`;
        }

        await this.applyEdit(
            document,
            (raw) => mutations.addGroup(raw, presetIndex, configIndex, group),
            'add group'
        );
    }

    /**
     * @param {vscode.TextDocument} document
     * @param {{presetIndex: number, configIndex: number, group: string, count: number}} target
     */
    async promptDeleteGroup(document, target) {
        if (target.count > 0) {
            const choice = await vscode.window.showWarningMessage(
                `Delete group '${target.group}' and its ${target.count} entr${target.count === 1 ? 'y' : 'ies'}?`,
                { modal: true },
                'Delete'
            );
            if (choice !== 'Delete') {
                return;
            }
        }
        await this.applyEdit(
            document,
            (raw) =>
                mutations.deleteGroup(raw, target.presetIndex, target.configIndex, target.group),
            'delete group'
        );
    }

    /**
     * @param {vscode.TextDocument} document
     * @param {number} presetIndex
     */
    async promptAddConfig(document, presetIndex) {
        const model = this.modelFor(document);
        const used = new Set(
            model?.presets?.[presetIndex]?.configs.map((config) => config.targetName) ?? []
        );
        const known = (model?.knownTargets ?? []).filter((name) => !used.has(name));

        const picked = await vscode.window.showQuickPick(
            [
                { label: '$(add) New target name…', alwaysShow: true },
                ...known.map((name) => ({ label: name, description: 'known in this workspace' }))
            ],
            { title: 'Add target config', placeHolder: 'Matched against target_name by the generator' }
        );
        if (!picked) {
            return;
        }

        let targetName = picked.label;
        if (targetName.startsWith('$(add)')) {
            const entered = await vscode.window.showInputBox({
                title: 'Target name',
                prompt: 'Value the generator matches against "target_name"',
                validateInput: (candidate) =>
                    candidate.trim() ? (used.has(candidate) ? 'Already present' : undefined) : 'Required'
            });
            if (!entered) {
                return;
            }
            targetName = entered;
        }

        await this.applyEdit(
            document,
            (raw) =>
                mutations.addConfig(raw, presetIndex, {
                    targetName,
                    namespace: '',
                    defaultOutput: 'public'
                }),
            'add config'
        );
    }

    /**
     * @param {vscode.TextDocument} document
     * @param {{presetIndex: number, configIndex: number, targetName: string}} target
     */
    async promptDeleteConfig(document, target) {
        const choice = await vscode.window.showWarningMessage(
            `Remove the '${target.targetName}' config from this preset?`,
            { modal: true },
            'Remove'
        );
        if (choice !== 'Remove') {
            return;
        }
        await this.applyEdit(
            document,
            (raw) => mutations.deleteConfig(raw, target.presetIndex, target.configIndex),
            'delete config'
        );
    }

    /** @param {vscode.TextDocument} document */
    async promptAddPreset(document) {
        const model = this.modelFor(document);
        const used = new Set((model?.presets ?? []).map((preset) => preset.name));
        const known = (model?.knownPresets ?? []).filter((name) => !used.has(name));

        const picked = await vscode.window.showQuickPick(
            [
                { label: '$(add) New preset name…', alwaysShow: true },
                ...known.map((name) => ({ label: name, description: 'used elsewhere in this workspace' }))
            ],
            { title: 'Add preset block', placeHolder: 'Selected by -DSKL_TUNE_PRESET' }
        );
        if (!picked) {
            return;
        }

        let name = picked.label;
        if (name.startsWith('$(add)')) {
            const entered = await vscode.window.showInputBox({
                title: 'Preset name',
                prompt: 'Matched against -DSKL_TUNE_PRESET',
                validateInput: (candidate) =>
                    candidate.trim() ? (used.has(candidate) ? 'Already present' : undefined) : 'Required'
            });
            if (!entered) {
                return;
            }
            name = entered;
        }

        await this.applyEdit(document, (raw) => mutations.addPreset(raw, name), 'add preset');
    }

    /**
     * @param {vscode.TextDocument} document
     * @param {{presetIndex: number, name: string}} target
     */
    async promptRenamePreset(document, target) {
        const model = this.modelFor(document);
        const used = new Set(
            (model?.presets ?? [])
                .filter((_, index) => index !== target.presetIndex)
                .map((preset) => preset.name)
        );

        const name = await vscode.window.showInputBox({
            title: `Rename preset '${target.name}'`,
            value: target.name,
            prompt: 'This is the name matched against -DSKL_TUNE_PRESET, so the build selects it by this string.',
            validateInput: (candidate) => {
                if (!candidate.trim()) {
                    return 'A preset name is required';
                }
                return used.has(candidate) ? 'Another preset in this file already uses that name' : undefined;
            }
        });
        if (!name || name === target.name) {
            return;
        }
        await this.applyEdit(
            document,
            (raw) => mutations.setPresetName(raw, target.presetIndex, name),
            `rename preset to '${name}'`
        );
    }

    /**
     * @param {vscode.TextDocument} document
     * @param {{presetIndex: number, name: string}} target
     */
    async promptDeletePreset(document, target) {
        const choice = await vscode.window.showWarningMessage(
            `Delete the '${target.name}' preset block and everything in it?`,
            { modal: true },
            'Delete'
        );
        if (choice !== 'Delete') {
            return;
        }
        await this.applyEdit(
            document,
            (raw) => mutations.deletePreset(raw, target.presetIndex),
            'delete preset'
        );
    }

    /**
     * Jump the raw text editor to a logical path.
     * @param {vscode.TextDocument} document
     * @param {(string|number)[]} jsonPath
     */
    async revealPath(document, jsonPath) {
        let ast;
        try {
            ast = parseTree(document.getText());
        } catch {
            return;
        }
        const property = propertyAtPath(ast, jsonPath);
        const offset = property ? property.keyOffset : 0;
        const position = document.positionAt(offset);

        const editor = await vscode.window.showTextDocument(document, {
            viewColumn: vscode.ViewColumn.Beside,
            preview: false
        });
        const range = property
            ? new vscode.Range(position, document.positionAt(property.end))
            : new vscode.Range(position, position);
        editor.selection = new vscode.Selection(range.start, range.end);
        editor.revealRange(range, vscode.TextEditorRevealType.InCenter);
    }

    // -- context-menu command targets --------------------------------------

    /**
     * Resolve the row a `webview/context` command applies to.
     * VS Code hands the `data-vscode-context` payload straight through as the
     * command argument; we fall back to the cached one for keyboard invocation.
     * @param {*} arg
     */
    resolveContext(arg) {
        const context = arg && typeof arg === 'object' && arg.key ? arg : this.lastContext;
        if (!context) {
            return undefined;
        }
        const uriString = context.docUri ?? this.lastContext?.docUri;
        if (!uriString) {
            return undefined;
        }
        const entry = this.active.get(uriString);
        if (!entry) {
            return undefined;
        }
        return { ...context, document: entry.document, panel: entry.panel };
    }

    /** @param {*} arg */
    async ctxEditValue(arg) {
        const context = this.resolveContext(arg);
        if (!context) {
            return;
        }
        const model = this.modelFor(context.document);
        const entry = findEntry(model, context);
        const value = await vscode.window.showInputBox({
            title: `Value of ${context.key}`,
            value: entry?.value ?? '',
            prompt: entry?.inherited
                ? `Inherited: ${entry.inherited.value}  (from ${entry.inherited.file})`
                : 'Written verbatim into the generated header'
        });
        if (value === undefined) {
            return;
        }
        await this.applyEdit(
            context.document,
            (raw) =>
                mutations.setEntryValue(
                    raw,
                    context.presetIndex,
                    context.configIndex,
                    context.group,
                    context.key,
                    value
                ),
            'change value'
        );
    }

    /** @param {*} arg */
    async ctxEditMeta(arg) {
        const context = this.resolveContext(arg);
        if (!context) {
            return;
        }
        const model = this.modelFor(context.document);
        const entry = findEntry(model, context);
        if (!entry) {
            return;
        }

        const field = await vscode.window.showQuickPick(
            [
                { label: 'Description', description: entry.ownDesc ?? '(inherited)', value: 'desc' },
                ...(entry.kind === 'constexpr'
                    ? [{ label: 'Type', description: entry.ownType ?? '(inherited)', value: 'type' }]
                    : []),
                { label: 'Namespace override', description: entry.namespace ?? '(config default)', value: 'namespace' }
            ],
            { title: `Edit metadata of ${context.key}` }
        );
        if (!field) {
            return;
        }

        const key = /** @type {'desc'|'type'|'namespace'} */ (field.value);
        const current = key === 'desc' ? entry.ownDesc : key === 'type' ? entry.ownType : entry.namespace;
        const value = await vscode.window.showInputBox({
            title: `${field.label} of ${context.key}`,
            value: current ?? '',
            prompt: 'Leave empty to remove'
        });
        if (value === undefined) {
            return;
        }
        await this.applyEdit(
            context.document,
            (raw) =>
                mutations.setEntryMeta(
                    raw,
                    context.presetIndex,
                    context.configIndex,
                    context.group,
                    context.key,
                    { [key]: value === '' ? null : value }
                ),
            'change metadata'
        );
    }

    /** @param {*} arg */
    async ctxToggleOutput(arg) {
        const context = this.resolveContext(arg);
        if (!context) {
            return;
        }
        const model = this.modelFor(context.document);
        const entry = findEntry(model, context);
        const current = entry?.ownOutput ?? entry?.output ?? 'public';
        const next = current === 'public' ? 'private' : 'public';
        await this.applyEdit(
            context.document,
            (raw) =>
                mutations.setEntryMeta(
                    raw,
                    context.presetIndex,
                    context.configIndex,
                    context.group,
                    context.key,
                    { output: next }
                ),
            'toggle output'
        );
        vscode.window.setStatusBarMessage(`${context.key} → ${next} header`, 3000);
    }

    /** @param {*} arg */
    async ctxMoveToGroup(arg) {
        const context = this.resolveContext(arg);
        if (!context) {
            return;
        }
        const model = this.modelFor(context.document);
        const config = model?.presets?.[context.presetIndex]?.configs?.[context.configIndex];
        if (!config) {
            return;
        }
        const candidates = config.groups
            .map((group) => group.name)
            .filter((name) => name !== context.group);

        const picked = await vscode.window.showQuickPick(
            candidates.map((name) => ({ label: name })),
            {
                title: `Move ${context.key} out of ${context.group}`,
                placeHolder: 'Grouping is cosmetic — every constexprs.* bucket flattens into one table'
            }
        );
        if (!picked) {
            return;
        }
        await this.applyEdit(
            context.document,
            (raw) =>
                mutations.moveEntry(
                    raw,
                    context.presetIndex,
                    context.configIndex,
                    context.group,
                    picked.label,
                    context.key
                ),
            'move entry'
        );
    }

    /** @param {*} arg */
    async ctxRenameKey(arg) {
        const context = this.resolveContext(arg);
        if (!context) {
            return;
        }
        const newKey = await vscode.window.showInputBox({
            title: `Rename ${context.key}`,
            value: context.key,
            validateInput: (candidate) =>
                /^[A-Za-z_][A-Za-z0-9_]*$/.test(candidate) ? undefined : 'Must be a valid C identifier'
        });
        if (!newKey || newKey === context.key) {
            return;
        }

        const others = this._filesDeclaring(context.key);
        if (others.length > 0) {
            const choice = await vscode.window.showQuickPick(
                [
                    {
                        label: `Rename here and in ${others.length} other file${others.length === 1 ? '' : 's'}`,
                        detail: others.map((file) => path.basename(file.id)).join(', '),
                        value: 'all'
                    },
                    { label: 'Rename in this file only', value: 'one' }
                ],
                { title: `'${context.key}' is declared in other preset files` }
            );
            if (!choice) {
                return;
            }
            if (choice.value === 'all') {
                await this._renameEverywhere(context, newKey, others);
                return;
            }
        }

        await this.applyEdit(
            context.document,
            (raw) =>
                mutations.renameEntry(
                    raw,
                    context.presetIndex,
                    context.configIndex,
                    context.group,
                    context.key,
                    newKey
                ),
            'rename key'
        );
    }

    /**
     * @param {string} key
     * @returns {import('../core/workspaceIndex').IndexedFile[]}
     */
    _filesDeclaring(key) {
        const out = [];
        for (const file of this.index.files.values()) {
            const declares = file.doc?.presets.some((preset) =>
                preset.configs.some((config) => config.entries.some((entry) => entry.key === key))
            );
            if (declares) {
                out.push(file);
            }
        }
        return out;
    }

    /**
     * @param {*} context
     * @param {string} newKey
     * @param {import('../core/workspaceIndex').IndexedFile[]} files
     */
    async _renameEverywhere(context, newKey, files) {
        const edit = new vscode.WorkspaceEdit();
        let touched = 0;

        for (const file of files) {
            const document = await vscode.workspace.openTextDocument(file.uri);
            const text = document.getText();
            let raw;
            try {
                raw = parseTree(text).value;
            } catch {
                continue;
            }

            let changed = false;
            const walk = (node) => {
                for (const preset of node?.presets ?? []) {
                    for (const config of preset?.config ?? []) {
                        for (const group of Object.keys(config ?? {})) {
                            const bucket = config[group];
                            if (
                                bucket &&
                                typeof bucket === 'object' &&
                                !Array.isArray(bucket) &&
                                context.key in bucket
                            ) {
                                config[group] = mutations.renameKeyPreservingOrder(
                                    bucket,
                                    context.key,
                                    newKey
                                );
                                changed = true;
                            }
                        }
                    }
                }
            };
            walk(raw);

            if (!changed) {
                continue;
            }
            const serialized = stringify(raw, detectStyle(text));
            if (serialized === text) {
                continue;
            }
            edit.replace(
                file.uri,
                new vscode.Range(document.positionAt(0), document.positionAt(text.length)),
                serialized
            );
            touched++;
        }

        if (touched === 0) {
            return;
        }
        const applied = await vscode.workspace.applyEdit(edit);
        if (applied) {
            vscode.window.showInformationMessage(
                `Renamed '${context.key}' to '${newKey}' in ${touched} file${touched === 1 ? '' : 's'}.`
            );
            await this.index.refresh();
        }
    }

    /** @param {*} arg */
    async ctxDuplicateKey(arg) {
        const context = this.resolveContext(arg);
        if (!context) {
            return;
        }
        const model = this.modelFor(context.document);
        const entry = findEntry(model, context);
        const newKey = await vscode.window.showInputBox({
            title: `Duplicate ${context.key}`,
            value: `${context.key}_COPY`,
            validateInput: (candidate) =>
                /^[A-Za-z_][A-Za-z0-9_]*$/.test(candidate) ? undefined : 'Must be a valid C identifier'
        });
        if (!newKey) {
            return;
        }
        await this.applyEdit(
            context.document,
            (raw) => {
                const config = raw?.presets?.[context.presetIndex]?.config?.[context.configIndex];
                const source = config?.[context.group]?.[context.key];
                if (source === undefined) {
                    return undefined;
                }
                return mutations.addEntry(
                    raw,
                    context.presetIndex,
                    context.configIndex,
                    context.group,
                    newKey,
                    mutations.clone(source),
                    context.key
                );
            },
            'duplicate entry'
        );
        void entry;
    }

    /** @param {*} arg */
    async ctxDeleteKey(arg) {
        const context = this.resolveContext(arg);
        if (!context) {
            return;
        }
        await this.deleteEntry(context.document, context);
    }

    /** @param {*} arg */
    async ctxGotoDefault(arg) {
        const context = this.resolveContext(arg);
        if (!context) {
            return;
        }
        const model = this.modelFor(context.document);
        const entry = findEntry(model, context);
        const fileId = entry?.inherited?.fileId;
        if (!fileId) {
            vscode.window.showInformationMessage(
                `'${context.key}' is introduced by this file — there is no lower-priority definition.`
            );
            return;
        }
        await this.openKeyIn(fileId, context.key);
    }

    /**
     * Open a file and select the given key.
     * @param {string} fileId
     * @param {string} key
     */
    async openKeyIn(fileId, key) {
        const uri = vscode.Uri.file(fileId);
        const document = await vscode.workspace.openTextDocument(uri);
        let ast;
        try {
            ast = parseTree(document.getText());
        } catch {
            await vscode.commands.executeCommand('vscode.open', uri);
            return;
        }

        const offset = findKeyOffset(ast, key);
        const editor = await vscode.window.showTextDocument(document, { preview: false });
        if (offset !== undefined) {
            const position = document.positionAt(offset);
            editor.selection = new vscode.Selection(position, position);
            editor.revealRange(
                new vscode.Range(position, position),
                vscode.TextEditorRevealType.InCenter
            );
        }
    }

    /** @param {*} arg */
    async ctxCopyKey(arg) {
        const context = this.resolveContext(arg);
        if (context) {
            await vscode.env.clipboard.writeText(context.key);
            vscode.window.setStatusBarMessage(`Copied ${context.key}`, 2000);
        }
    }

    /** @param {*} arg */
    async ctxCopyValue(arg) {
        const context = this.resolveContext(arg);
        if (!context) {
            return;
        }
        const entry = findEntry(this.modelFor(context.document), context);
        if (entry) {
            await vscode.env.clipboard.writeText(String(entry.value));
            vscode.window.setStatusBarMessage(`Copied ${entry.value}`, 2000);
        }
    }

    /** @param {*} arg */
    async ctxRevealInText(arg) {
        const context = this.resolveContext(arg);
        if (!context) {
            return;
        }
        await this.revealPath(context.document, [
            'presets',
            context.presetIndex,
            'config',
            context.configIndex,
            context.group,
            context.key
        ]);
    }

    /** @param {*} arg */
    async ctxOverrideIn(arg) {
        const context = this.resolveContext(arg);
        if (!context) {
            return;
        }
        const model = this.modelFor(context.document);
        const entry = findEntry(model, context);
        const config = model?.presets?.[context.presetIndex]?.configs?.[context.configIndex];
        if (!entry || !config) {
            return;
        }

        // Only files that already carry a config for this target are sensible
        // destinations; anywhere else the entry would be silently ignored.
        /** @type {vscode.QuickPickItem[]} */
        const items = [];
        for (const file of this.index.files.values()) {
            if (file.id === normalizeId(context.document.uri.fsPath)) {
                continue;
            }
            for (const preset of file.doc?.presets ?? []) {
                const match = preset.configs.find(
                    (candidate) => candidate.targetName === config.targetName
                );
                if (!match) {
                    continue;
                }
                items.push({
                    label: `${preset.name}`,
                    description: `${path.basename(path.dirname(file.id))}/${path.basename(file.id)}`,
                    detail: `priority ${file.doc?.priority} — ${
                        match.entries.some((candidate) => candidate.key === context.key)
                            ? 'already overrides this key'
                            : 'no override yet'
                    }`,
                    // @ts-ignore carry the destination along
                    _file: file,
                    _preset: preset
                });
            }
        }

        if (items.length === 0) {
            vscode.window.showInformationMessage(
                `No other preset file declares a config for target '${config.targetName}'.`
            );
            return;
        }

        const picked = /** @type {any} */ (
            await vscode.window.showQuickPick(items, {
                title: `Override ${context.key} elsewhere`,
                placeHolder: 'Destination preset file'
            })
        );
        if (!picked) {
            return;
        }

        const value = await vscode.window.showInputBox({
            title: `Value of ${context.key} in '${picked.label}'`,
            value: String(entry.value),
            prompt: `Writing to ${picked.description}`
        });
        if (value === undefined) {
            return;
        }

        /** @type {import('../core/workspaceIndex').IndexedFile} */
        const destFile = picked._file;
        const destPreset = picked._preset;
        const presetIndex = destFile.doc.presets.indexOf(destPreset);
        const configIndex = destPreset.configs.findIndex(
            (candidate) => candidate.targetName === config.targetName
        );
        const destConfig = destPreset.configs[configIndex];
        const group = destConfig.groups.includes(context.group)
            ? context.group
            : destConfig.groups.find((name) =>
                  entry.kind === 'define' ? name === DEFINES_GROUP : name.startsWith(CONSTEXPR_GROUP)
              ) ?? (entry.kind === 'define' ? DEFINES_GROUP : CONSTEXPR_GROUP);

        const document = await vscode.workspace.openTextDocument(destFile.uri);
        await this.applyEdit(
            document,
            (raw) =>
                mutations.addEntry(
                    raw,
                    presetIndex,
                    configIndex,
                    group,
                    context.key,
                    mutations.makeOverrideEntry(value)
                ),
            'add override'
        );
        await vscode.commands.executeCommand('vscode.openWith', destFile.uri, VIEW_TYPE);
    }

    /** @param {vscode.Webview} webview */
    _html(webview) {
        const nonce = makeNonce();
        const styleUri = webview.asWebviewUri(
            vscode.Uri.joinPath(this.context.extensionUri, 'media', 'editor.css')
        );
        const scriptUri = webview.asWebviewUri(
            vscode.Uri.joinPath(this.context.extensionUri, 'media', 'editor.js')
        );

        return `<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta http-equiv="Content-Security-Policy" content="default-src 'none'; style-src ${webview.cspSource}; script-src 'nonce-${nonce}'; font-src ${webview.cspSource};">
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<link href="${styleUri}" rel="stylesheet">
<title>Skylake Tuning Preset</title>
</head>
<body>
<div id="root"><div class="loading">Loading…</div></div>
<script nonce="${nonce}" src="${scriptUri}"></script>
</body>
</html>`;
    }
}

/**
 * @param {*} model
 * @param {{presetIndex: number, configIndex: number, group: string, key: string}} context
 */
function findEntry(model, context) {
    const config = model?.presets?.[context.presetIndex]?.configs?.[context.configIndex];
    const group = config?.groups.find((candidate) => candidate.name === context.group);
    return group?.entries.find((candidate) => candidate.key === context.key);
}

/**
 * Depth-first search for the first property named `key`.
 * @param {import('../core/jsonAst').AstNode} node
 * @param {string} key
 * @returns {number|undefined}
 */
function findKeyOffset(node, key) {
    if (node.type === 'object' && node.properties) {
        for (const property of node.properties) {
            if (property.key === key) {
                return property.keyOffset;
            }
            const nested = findKeyOffset(property.value, key);
            if (nested !== undefined) {
                return nested;
            }
        }
    } else if (node.type === 'array' && node.items) {
        for (const item of node.items) {
            const nested = findKeyOffset(item, key);
            if (nested !== undefined) {
                return nested;
            }
        }
    }
    return undefined;
}

function makeNonce() {
    // A CSP nonce must be unguessable, so take it from the CSPRNG rather than
    // Math.random.
    return require('crypto').randomBytes(24).toString('base64').replace(/[^A-Za-z0-9]/g, '');
}

module.exports = { PresetEditorProvider, VIEW_TYPE, OUTPUTS };
