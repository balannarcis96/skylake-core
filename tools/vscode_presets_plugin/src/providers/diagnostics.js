/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2025 Balan Narcis (balannarcis96@gmail.com)
 *
 * Problems-panel diagnostics for tuning preset files.
 *
 * These target the failure modes that a preset file cannot reveal on its own,
 * because every one of them is invisible until you cross-reference the file
 * with CMake and with the rest of the preset tree:
 *
 *   - the file is never registered, so CMake silently ignores it,
 *   - a new constexpr lacks `type`, which aborts the generator,
 *   - an override is shadowed by a higher-priority file and never lands,
 *   - a lower-priority file sits after a higher-priority one, so no key it
 *     ever gains will win,
 *   - an override restates the value it inherits.
 */
'use strict';

const path = require('path');
const vscode = require('vscode');

const { duplicateKeys, propertyAtPath } = require('../core/jsonAst');

const SOURCE = 'skylake-tuning';

class DiagnosticsProvider {
    /**
     * @param {vscode.ExtensionContext} context
     * @param {import('../core/workspaceIndex').WorkspaceIndex} index
     */
    constructor(context, index) {
        this.index = index;
        this.collection = vscode.languages.createDiagnosticCollection('skylakeTuning');
        context.subscriptions.push(this.collection);

        this._subscription = index.onDidChange(() => this.refresh());
        context.subscriptions.push({ dispose: () => this._subscription.dispose() });
    }

    dispose() {
        this.collection.dispose();
    }

    refresh() {
        const config = vscode.workspace.getConfiguration('skylakeTuning');
        this.collection.clear();
        if (!config.get('diagnostics.enabled', true)) {
            return;
        }

        const warnUnregistered = config.get('diagnostics.unregisteredFiles', true);
        const warnRedundant = config.get('diagnostics.redundantOverrides', true);

        for (const file of this.index.files.values()) {
            const diagnostics = this._forFile(file, { warnUnregistered, warnRedundant });
            if (diagnostics.length > 0) {
                this.collection.set(file.uri, diagnostics);
            }
        }
    }

    /**
     * @param {import('../core/workspaceIndex').IndexedFile} file
     * @param {{warnUnregistered: boolean, warnRedundant: boolean}} options
     * @returns {vscode.Diagnostic[]}
     */
    _forFile(file, options) {
        /** @type {vscode.Diagnostic[]} */
        const out = [];
        const at = makeRanger(file.text);

        if (file.parseError) {
            out.push(
                decorate(
                    new vscode.Diagnostic(
                        at(file.parseError.offset, file.parseError.offset + 1),
                        `Invalid JSON: ${file.parseError.message}`,
                        vscode.DiagnosticSeverity.Error
                    ),
                    'invalid-json'
                )
            );
            return out;
        }

        if (!file.doc || !file.ast) {
            return out;
        }

        // -- duplicate keys -------------------------------------------------
        for (const duplicate of duplicateKeys(file.ast)) {
            out.push(
                decorate(
                    new vscode.Diagnostic(
                        at(duplicate.property.keyOffset, duplicate.property.keyEnd),
                        `Duplicate key '${duplicate.key}'. Only the last occurrence survives json.load.`,
                        vscode.DiagnosticSeverity.Warning
                    ),
                    'duplicate-key'
                )
            );
        }

        // -- registration ---------------------------------------------------
        if (options.warnUnregistered && this.index.registrations.length > 0) {
            const status = this.index.registrationStatus(file.id);
            if (!status.registered && status.asDefaultFor.length === 0) {
                const versionProp = propertyAtPath(file.ast, ['presets']);
                const range = versionProp
                    ? at(versionProp.keyOffset, versionProp.keyEnd)
                    : at(0, Math.min(1, file.text.length));
                out.push(
                    decorate(
                        new vscode.Diagnostic(
                            range,
                            'This preset file is never passed to skl_add_presets_file() and is not any ' +
                                "target's DEFAULT_PRESET_FILE. CMake never reads it, so every value here is ignored.",
                            vscode.DiagnosticSeverity.Warning
                        ),
                        'unregistered-file'
                    )
                );
            }
        }

        // -- per (preset, target) -------------------------------------------
        for (const preset of file.doc.presets) {
            for (const config of preset.configs) {
                const targetInfo = this.index.targets.get(config.targetName);

                if (targetInfo && targetInfo.consumers.length === 0 && this.index.tuneTargets.length > 0) {
                    const property = propertyAtPath(file.ast, config.path.concat('target_name'));
                    if (property) {
                        out.push(
                            decorate(
                                new vscode.Diagnostic(
                                    at(property.value.offset, property.value.end),
                                    `No skl_add_tune_header_to_target() call uses the target name ` +
                                        `'${config.targetName}'. Nothing in the build consumes this config.`,
                                    vscode.DiagnosticSeverity.Information
                                ),
                                'target-without-consumer'
                            )
                        );
                    }
                }

                out.push(...this._forConfig(file, preset, config, at, options));
            }
        }

        out.push(...this._priorityOrdering(file, at));

        return out;
    }

    /**
     * @param {import('../core/workspaceIndex').IndexedFile} file
     * @param {import('../core/schema').PresetBlock} preset
     * @param {import('../core/schema').PresetConfig} config
     * @param {(from: number, to: number) => vscode.Range} at
     * @param {{warnRedundant: boolean}} options
     */
    _forConfig(file, preset, config, at, options) {
        /** @type {vscode.Diagnostic[]} */
        const out = [];

        const inherited = this.index.resolveInherited(config.targetName, preset.name, file.id);
        const effective = this.index.resolve(config.targetName, preset.name);

        for (const entry of config.entries) {
            const property = file.ast ? propertyAtPath(file.ast, entry.path) : undefined;
            if (!property) {
                continue;
            }
            const keyRange = at(property.keyOffset, property.keyEnd);
            const below = inherited.entries.get(entry.key);
            const winner = effective.entries.get(entry.key);

            if (!below && entry.kind === 'constexpr' && (entry.shorthand || !entry.type)) {
                out.push(
                    decorate(
                        new vscode.Diagnostic(
                            keyRange,
                            `'${entry.key}' is a new constexpr but has no "type". generate_tuning.py ` +
                                'rejects this with exit(-1), which surfaces at configure time as a ' +
                                'missing-input error from configure_file.',
                            vscode.DiagnosticSeverity.Error
                        ),
                        'constexpr-missing-type'
                    )
                );
            }

            if (winner && winner.fileId !== file.id) {
                const diagnostic = new vscode.Diagnostic(
                    keyRange,
                    `'${entry.key}' never reaches the compiler: ${label(winner.fileId)} ` +
                        `(priority ${winner.priority}) wins with '${winner.value}'.`,
                    vscode.DiagnosticSeverity.Warning
                );
                diagnostic.relatedInformation = [
                    new vscode.DiagnosticRelatedInformation(
                        new vscode.Location(vscode.Uri.file(winner.fileId), new vscode.Position(0, 0)),
                        `winning definition (priority ${winner.priority})`
                    )
                ];
                out.push(decorate(diagnostic, 'shadowed-override'));
            } else if (
                options.warnRedundant &&
                below &&
                String(below.value).trim() === String(entry.value).trim()
            ) {
                out.push(
                    decorate(
                        new vscode.Diagnostic(
                            keyRange,
                            `'${entry.key}' repeats the value it already inherits from ` +
                                `${label(below.fileId)} ('${below.value}'). The override changes nothing.`,
                            vscode.DiagnosticSeverity.Hint
                        ),
                        'redundant-override'
                    )
                );
            }
        }

        return out;
    }

    /**
     * Flag a file that is registered AFTER a file with a higher priority for
     * the same (preset, target). No key it ever gains can win, because the
     * override test is strictly `existing.prio < new.prio`.
     *
     * @param {import('../core/workspaceIndex').IndexedFile} file
     * @param {(from: number, to: number) => vscode.Range} at
     */
    _priorityOrdering(file, at) {
        /** @type {vscode.Diagnostic[]} */
        const out = [];
        if (!file.doc || !file.ast || this.index.registrations.length === 0) {
            return out;
        }

        const self = this.index.registrations.find(
            (registration) => normalize(registration.filePath) === file.id
        );
        if (!self) {
            return out;
        }

        const property = propertyAtPath(file.ast, ['priority']);
        if (!property) {
            return out;
        }

        /** @type {Map<string, {other: string, priority: number, preset: string, target: string}>} */
        const blockers = new Map();

        for (const registration of this.index.registrations) {
            if (registration.order >= self.order) {
                continue;
            }
            const other = this.index.files.get(normalize(registration.filePath));
            if (!other?.doc || other.doc.priority <= file.doc.priority) {
                continue;
            }
            for (const preset of file.doc.presets) {
                const match = other.doc.presets.find((candidate) => candidate.name === preset.name);
                if (!match) {
                    continue;
                }
                for (const config of preset.configs) {
                    const shared = match.configs.find(
                        (candidate) => candidate.targetName === config.targetName
                    );
                    if (!shared) {
                        continue;
                    }
                    blockers.set(`${preset.name}/${config.targetName}`, {
                        other: other.id,
                        priority: other.doc.priority,
                        preset: preset.name,
                        target: config.targetName
                    });
                }
            }
        }

        for (const blocker of blockers.values()) {
            const diagnostic = new vscode.Diagnostic(
                at(property.value.offset, property.value.end),
                `Priority ${file.doc.priority} is lower than ${label(blocker.other)} (priority ` +
                    `${blocker.priority}), which is registered earlier and also configures ` +
                    `'${blocker.target}' under preset '${blocker.preset}'. Nothing declared here can ` +
                    'ever override it.',
                vscode.DiagnosticSeverity.Warning
            );
            diagnostic.relatedInformation = [
                new vscode.DiagnosticRelatedInformation(
                    new vscode.Location(vscode.Uri.file(blocker.other), new vscode.Position(0, 0)),
                    `higher-priority file registered earlier (priority ${blocker.priority})`
                )
            ];
            out.push(decorate(diagnostic, 'priority-inversion'));
        }

        return out;
    }
}

/**
 * Build an offset -> Range mapper for a text blob, without needing a
 * TextDocument (files may not be open).
 * @param {string} text
 */
function makeRanger(text) {
    /** @type {number[]} */
    const lineStarts = [0];
    for (let i = 0; i < text.length; i++) {
        if (text[i] === '\n') {
            lineStarts.push(i + 1);
        }
    }

    /** @param {number} offset */
    const toPosition = (offset) => {
        const clamped = Math.max(0, Math.min(offset, text.length));
        let low = 0;
        let high = lineStarts.length - 1;
        while (low < high) {
            const mid = Math.ceil((low + high) / 2);
            if (lineStarts[mid] <= clamped) {
                low = mid;
            } else {
                high = mid - 1;
            }
        }
        return new vscode.Position(low, clamped - lineStarts[low]);
    };

    return (from, to) => new vscode.Range(toPosition(from), toPosition(to));
}

/** @param {string} fileId */
function label(fileId) {
    return `${path.basename(path.dirname(fileId))}/${path.basename(fileId)}`;
}

/** @param {string} fsPath */
function normalize(fsPath) {
    const normalized = path.normalize(fsPath).replace(/[\\/]+$/, '');
    return process.platform === 'win32' ? normalized.toLowerCase() : normalized;
}

/**
 * @param {vscode.Diagnostic} diagnostic
 * @param {string} code
 */
function decorate(diagnostic, code) {
    diagnostic.source = SOURCE;
    diagnostic.code = code;
    return diagnostic;
}

module.exports = { DiagnosticsProvider };
