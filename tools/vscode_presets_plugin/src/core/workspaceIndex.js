/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2025 Balan Narcis (balannarcis96@gmail.com)
 *
 * Workspace-wide index of tuning preset files.
 *
 * Discovery is schema-driven rather than path-driven so the same extension
 * works against skylake-core, m2-server, or any other consumer of
 * SkylakeTuning.cmake without configuration. The index is also what turns a
 * single file into something meaningful: a preset file in isolation cannot
 * tell you what it overrides or whether CMake even reads it.
 */
'use strict';

const fs = require('fs');
const path = require('path');
const vscode = require('vscode');

const { parseTree, JsonParseError } = require('./jsonAst');
const { detectStyle } = require('./serialize');
const { looksLikePresetDocument, buildDocument } = require('./schema');
const { scanProject } = require('./cmake');
const { resolve } = require('./resolver');

/**
 * @typedef {Object} IndexedFile
 * @property {vscode.Uri} uri
 * @property {string} id                 Normalized fsPath, the index key.
 * @property {string} text
 * @property {import('./jsonAst').AstNode|undefined} ast
 * @property {import('./schema').PresetDocument|undefined} doc
 * @property {import('./serialize').JsonStyle} style
 * @property {JsonParseError|undefined} parseError
 * @property {vscode.WorkspaceFolder|undefined} folder
 */

/**
 * @typedef {Object} TargetInfo
 * @property {string} name                      The JSON `target_name`.
 * @property {import('./cmake').TuneTarget[]} consumers
 * @property {string[]} fileIds                 Files declaring a config for it.
 * @property {string[]} presetNames             Preset names it appears under.
 * @property {string|undefined} defaultPresetFile
 * @property {string|undefined} defaultPresetName
 */

const DEFAULT_PRESET_FILE_HINTS = ['default_preset.json', 'default_presets.json'];

class WorkspaceIndex {
    constructor() {
        /** @type {Map<string, IndexedFile>} */
        this.files = new Map();
        /** @type {import('./cmake').PresetRegistration[]} */
        this.registrations = [];
        /** @type {import('./cmake').TuneTarget[]} */
        this.tuneTargets = [];
        /** @type {Map<string, TargetInfo>} */
        this.targets = new Map();
        /** @type {Set<string>} */
        this.presetNames = new Set();
        /** @type {string[]} */
        this.scannedLists = [];

        this._emitter = new vscode.EventEmitter();
        /** @type {vscode.Event<void>} */
        this.onDidChange = this._emitter.event;
        this._scanning = false;
        this._rescanQueued = false;
    }

    dispose() {
        this._emitter.dispose();
    }

    /** @returns {string} */
    get defaultPresetName() {
        return vscode.workspace
            .getConfiguration('skylakeTuning')
            .get('defaultPresetName', 'default');
    }

    /**
     * @param {vscode.Uri} uri
     * @returns {IndexedFile|undefined}
     */
    get(uri) {
        return this.files.get(normalizeId(uri.fsPath));
    }

    /** @returns {IndexedFile[]} */
    allFiles() {
        return [...this.files.values()].sort((a, b) => a.id.localeCompare(b.id));
    }

    /**
     * Rebuild the whole index. Cheap enough (tens of small JSON files) that
     * incremental updates are not worth the staleness risk.
     */
    async refresh() {
        if (this._scanning) {
            this._rescanQueued = true;
            return;
        }
        this._scanning = true;
        try {
            do {
                this._rescanQueued = false;
                await this._scan();
            } while (this._rescanQueued);
        } finally {
            this._scanning = false;
        }
        this._emitter.fire();
    }

    async _scan() {
        const config = vscode.workspace.getConfiguration('skylakeTuning');
        const includes = config.get('include', ['**/*_preset.json', '**/*_presets.json']);
        const exclude = config.get('exclude', '**/{node_modules,build,build2,out,.git,third_party}/**');

        /** @type {Map<string, IndexedFile>} */
        const files = new Map();

        /** @type {vscode.Uri[]} */
        const found = [];
        for (const include of includes) {
            const matches = await vscode.workspace.findFiles(include, exclude);
            found.push(...matches);
        }

        const uniqueUris = new Map();
        for (const uri of found) {
            uniqueUris.set(normalizeId(uri.fsPath), uri);
        }

        for (const [id, uri] of uniqueUris) {
            const indexed = await readIndexedFile(uri, id);
            if (indexed) {
                files.set(id, indexed);
            }
        }

        this.files = files;

        // CMake scan, once per workspace folder that has a root CMakeLists.txt.
        /** @type {import('./cmake').PresetRegistration[]} */
        const registrations = [];
        /** @type {import('./cmake').TuneTarget[]} */
        const tuneTargets = [];
        /** @type {string[]} */
        const scannedLists = [];

        for (const folder of vscode.workspace.workspaceFolders ?? []) {
            if (folder.uri.scheme !== 'file') {
                continue;
            }
            const rootDir = folder.uri.fsPath;
            /** @type {Map<string, string|undefined>} */
            const cache = new Map();
            const readFile = (absPath) => {
                if (cache.has(absPath)) {
                    return cache.get(absPath);
                }
                let content;
                try {
                    content = fs.readFileSync(absPath, 'utf8');
                } catch {
                    content = undefined;
                }
                cache.set(absPath, content);
                return content;
            };

            try {
                const scan = scanProject(rootDir, readFile);
                const base = registrations.length;
                for (const registration of scan.registrations) {
                    registrations.push({ ...registration, order: base + registration.order });
                }
                for (const target of scan.targets) {
                    tuneTargets.push({ ...target, order: base + target.order });
                }
                scannedLists.push(...scan.visited);
            } catch {
                // A malformed CMakeLists must not take the whole index down;
                // the JSON half of the extension still works without it.
            }
        }

        this.registrations = registrations;
        this.tuneTargets = tuneTargets;
        this.scannedLists = scannedLists;

        this._buildTargets();
    }

    _buildTargets() {
        /** @type {Map<string, TargetInfo>} */
        const targets = new Map();
        /** @type {Set<string>} */
        const presetNames = new Set();

        for (const file of this.files.values()) {
            if (!file.doc) {
                continue;
            }
            for (const preset of file.doc.presets) {
                presetNames.add(preset.name);
                for (const config of preset.configs) {
                    if (!config.targetName) {
                        continue;
                    }
                    let info = targets.get(config.targetName);
                    if (!info) {
                        info = {
                            name: config.targetName,
                            consumers: [],
                            fileIds: [],
                            presetNames: [],
                            defaultPresetFile: undefined,
                            defaultPresetName: undefined
                        };
                        targets.set(config.targetName, info);
                    }
                    if (!info.fileIds.includes(file.id)) {
                        info.fileIds.push(file.id);
                    }
                    if (!info.presetNames.includes(preset.name)) {
                        info.presetNames.push(preset.name);
                    }
                }
            }
        }

        for (const tuneTarget of this.tuneTargets) {
            const info = targets.get(tuneTarget.presetTargetName);
            if (!info) {
                // A consumer with no matching JSON config: real, and worth
                // surfacing, so materialize an empty target for it.
                targets.set(tuneTarget.presetTargetName, {
                    name: tuneTarget.presetTargetName,
                    consumers: [tuneTarget],
                    fileIds: [],
                    presetNames: [],
                    defaultPresetFile: tuneTarget.defaultPresetFile
                        ? normalizeId(tuneTarget.defaultPresetFile)
                        : undefined,
                    defaultPresetName: tuneTarget.defaultPresetName
                });
                continue;
            }
            info.consumers.push(tuneTarget);
            if (!info.defaultPresetFile && tuneTarget.defaultPresetFile) {
                info.defaultPresetFile = normalizeId(tuneTarget.defaultPresetFile);
            }
            if (!info.defaultPresetName && tuneTarget.defaultPresetName) {
                info.defaultPresetName = tuneTarget.defaultPresetName;
            }
        }

        // Fall back to a co-located default_preset.json when CMake did not say.
        for (const info of targets.values()) {
            if (info.defaultPresetFile) {
                continue;
            }
            const candidate = this._guessDefaultFile(info);
            if (candidate) {
                info.defaultPresetFile = candidate;
            }
        }

        this.targets = targets;
        this.presetNames = presetNames;
    }

    /**
     * @param {TargetInfo} info
     * @returns {string|undefined}
     */
    _guessDefaultFile(info) {
        const defaultName = this.defaultPresetName;
        const candidates = info.fileIds.filter((id) => {
            const file = this.files.get(id);
            return file?.doc?.presets.some(
                (preset) =>
                    preset.name === defaultName &&
                    preset.configs.some((config) => config.targetName === info.name)
            );
        });
        if (candidates.length === 0) {
            return undefined;
        }
        const byName = candidates.find((id) =>
            DEFAULT_PRESET_FILE_HINTS.includes(path.basename(id))
        );
        return byName ?? candidates[0];
    }

    /**
     * Is this file ever handed to CMake?
     * @param {string} fileId
     * @returns {{registered: boolean, asDefaultFor: string[], registration: import('./cmake').PresetRegistration|undefined}}
     */
    registrationStatus(fileId) {
        const registration = this.registrations.find((entry) => normalizeId(entry.filePath) === fileId);
        const asDefaultFor = [];
        for (const info of this.targets.values()) {
            if (info.defaultPresetFile === fileId && info.consumers.length > 0) {
                asDefaultFor.push(info.name);
            }
        }
        return { registered: registration !== undefined, asDefaultFor, registration };
    }

    /**
     * Ordered layer stack feeding one `(target, preset)` resolve.
     *
     * Honours the two ordering rules that make CMake's behaviour surprising: a
     * target only sees files registered before its own call, and the default
     * layer is always processed first.
     *
     * @param {string} targetName
     * @returns {{layers: {id: string, doc: import('./schema').PresetDocument}[], defaultLayerId: string|undefined, defaultPresetName: string}}
     */
    layersFor(targetName) {
        const info = this.targets.get(targetName);
        const defaultPresetName = info?.defaultPresetName ?? this.defaultPresetName;
        const defaultLayerId = info?.defaultPresetFile;

        /** @type {{id: string, doc: import('./schema').PresetDocument}[]} */
        const layers = [];

        if (defaultLayerId) {
            const file = this.files.get(defaultLayerId);
            if (file?.doc) {
                layers.push({ id: file.id, doc: file.doc });
            }
        }

        // Cut-off: a target only sees preset files registered BEFORE its own
        // skl_add_tune_header_to_target() call, because the generator runs at
        // that point with whatever SKL_TUNE_PRESETS_FILES holds.
        //
        // This only applies to a consumer at directory scope. When the call is
        // inside a function (skylake-core wraps its own in
        // skl_CreateSkylakeCoreLibTarget), the real call site is elsewhere and
        // the recorded index describes the definition, not the invocation.
        // Guessing there produced false "shadowed" reports, so treat the
        // ordering as unknown and let every file through.
        let cutoff = Number.POSITIVE_INFINITY;
        const ordered = (info?.consumers ?? []).filter((consumer) => consumer.orderKnown !== false);
        if (info && info.consumers.length > 0 && ordered.length === info.consumers.length) {
            cutoff = Math.max(...ordered.map((consumer) => consumer.order));
        }

        if (this.registrations.length > 0) {
            for (const registration of this.registrations) {
                if (registration.order >= cutoff) {
                    continue;
                }
                const id = normalizeId(registration.filePath);
                if (id === defaultLayerId) {
                    continue;
                }
                const file = this.files.get(id);
                if (file?.doc) {
                    layers.push({ id: file.id, doc: file.doc });
                }
            }
        } else {
            // No CMake information at all: approximate registration order with
            // ascending priority, which is what the files are designed around.
            const rest = this.allFiles()
                .filter((file) => file.doc && file.id !== defaultLayerId)
                .filter((file) =>
                    file.doc?.presets.some((preset) =>
                        preset.configs.some((config) => config.targetName === targetName)
                    )
                )
                .sort((a, b) => (a.doc?.priority ?? 0) - (b.doc?.priority ?? 0));
            for (const file of rest) {
                layers.push({ id: file.id, doc: /** @type {any} */ (file.doc) });
            }
        }

        return { layers, defaultLayerId, defaultPresetName };
    }

    /**
     * @param {string} targetName
     * @param {string} presetName
     * @returns {import('./resolver').ResolveResult}
     */
    resolve(targetName, presetName) {
        const { layers, defaultLayerId, defaultPresetName } = this.layersFor(targetName);
        return resolve({ targetName, presetName, defaultPresetName, layers, defaultLayerId });
    }

    /**
     * Resolve the stack UP TO but EXCLUDING one file, i.e. what a given file's
     * entries are overriding. Powers the "= default / ≠ default" badges.
     *
     * @param {string} targetName
     * @param {string} presetName
     * @param {string} excludeFileId
     * @returns {import('./resolver').ResolveResult}
     */
    resolveInherited(targetName, presetName, excludeFileId) {
        const { layers, defaultLayerId, defaultPresetName } = this.layersFor(targetName);
        const trimmed = [];
        for (const layer of layers) {
            if (layer.id === excludeFileId) {
                continue;
            }
            trimmed.push(layer);
        }
        return resolve({
            targetName,
            presetName,
            defaultPresetName,
            layers: trimmed,
            defaultLayerId
        });
    }

    /**
     * Every distinct key known for a target, across all presets.
     * @param {string} targetName
     * @returns {string[]}
     */
    keysFor(targetName) {
        /** @type {Set<string>} */
        const keys = new Set();
        for (const file of this.files.values()) {
            for (const preset of file.doc?.presets ?? []) {
                for (const config of preset.configs) {
                    if (config.targetName !== targetName) {
                        continue;
                    }
                    for (const entry of config.entries) {
                        keys.add(entry.key);
                    }
                }
            }
        }
        return [...keys].sort();
    }
}

/**
 * @param {vscode.Uri} uri
 * @param {string} id
 * @returns {Promise<IndexedFile|undefined>}
 */
async function readIndexedFile(uri, id) {
    let text;
    try {
        const open = vscode.workspace.textDocuments.find(
            (candidate) => normalizeId(candidate.uri.fsPath) === id
        );
        if (open) {
            text = open.getText();
        } else {
            text = Buffer.from(await vscode.workspace.fs.readFile(uri)).toString('utf8');
        }
    } catch {
        return undefined;
    }

    return buildIndexedFile(uri, id, text);
}

/**
 * @param {vscode.Uri} uri
 * @param {string} id
 * @param {string} text
 * @returns {IndexedFile|undefined}
 */
function buildIndexedFile(uri, id, text) {
    const style = detectStyle(text);
    const folder = vscode.workspace.getWorkspaceFolder(uri);

    /** @type {import('./jsonAst').AstNode|undefined} */
    let ast;
    /** @type {JsonParseError|undefined} */
    let parseError;
    try {
        ast = parseTree(text);
    } catch (error) {
        if (error instanceof JsonParseError) {
            parseError = error;
        } else {
            return undefined;
        }
    }

    if (!ast) {
        // Unparseable. Only keep it if the name strongly suggests a preset file,
        // so the editor can show the error instead of silently doing nothing.
        if (!/_presets?\.json$/i.test(id)) {
            return undefined;
        }
        return { uri, id, text, ast: undefined, doc: undefined, style, parseError, folder };
    }

    if (!looksLikePresetDocument(ast.value)) {
        return undefined;
    }

    return {
        uri,
        id,
        text,
        ast,
        doc: buildDocument(ast.value),
        style,
        parseError,
        folder
    };
}

/**
 * Windows paths reach us with mixed casing and separators; normalize so map
 * lookups from CMake strings and from VS Code URIs agree.
 * @param {string} fsPath
 */
function normalizeId(fsPath) {
    const normalized = path.normalize(fsPath).replace(/[\\/]+$/, '');
    return process.platform === 'win32' ? normalized.toLowerCase() : normalized;
}

module.exports = { WorkspaceIndex, normalizeId, buildIndexedFile };
