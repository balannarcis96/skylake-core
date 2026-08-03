/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2025 Balan Narcis (balannarcis96@gmail.com)
 *
 * Builds the view model handed to the custom editor webview.
 *
 * The value this adds over showing the raw JSON is context: for every entry it
 * answers "what would this key be without me?" and "does my value actually
 * survive the merge?". Both questions need the whole workspace, which is why
 * this lives next to the index rather than inside the webview.
 */
'use strict';

const path = require('path');
const {
    isConstexprGroup,
    DEFINES_GROUP,
    CONSTEXPR_GROUP,
    configureSubstitutions
} = require('./schema');

/**
 * @typedef {'introduced'|'override'|'redundant'|'shadowed'|'inherited'} EntryStatus
 */

/**
 * @param {import('./workspaceIndex').WorkspaceIndex} index
 * @param {import('./workspaceIndex').IndexedFile} file
 * @returns {*}
 */
function buildEditorModel(index, file) {
    const showInherited = true;
    const relPath = file.folder
        ? path.relative(file.folder.uri.fsPath, file.uri.fsPath)
        : file.uri.fsPath;

    if (!file.doc) {
        return {
            ok: false,
            fileName: path.basename(file.uri.fsPath),
            relPath,
            parseError: file.parseError
                ? { message: file.parseError.message, offset: file.parseError.offset }
                : { message: 'Not a Skylake tuning preset document.', offset: 0 }
        };
    }

    const status = index.registrationStatus(file.id);

    /** @type {any[]} */
    const presets = [];

    file.doc.presets.forEach((preset, presetIndex) => {
        /** @type {any[]} */
        const configs = [];

        preset.configs.forEach((config, configIndex) => {
            const targetName = config.targetName;
            const targetInfo = index.targets.get(targetName);
            const isDefaultLayer = index.targets.get(targetName)?.defaultPresetFile === file.id;

            // What this target resolves to WITHOUT this file, i.e. what each
            // entry here is actually overriding.
            const inherited = index.resolveInherited(targetName, preset.name, file.id);
            // And the real answer, including this file, so we can tell when an
            // entry here loses to something with a higher priority.
            const effective = index.resolve(targetName, preset.name);

            /** @type {Map<string, any[]>} */
            const groupMap = new Map();
            for (const group of config.groups) {
                groupMap.set(group, []);
            }

            for (const entry of config.entries) {
                const below = inherited.entries.get(entry.key);
                const winner = effective.entries.get(entry.key);
                const isWinner = winner?.fileId === file.id;

                /** @type {EntryStatus} */
                let entryStatus;
                if (!below) {
                    entryStatus = 'introduced';
                } else if (!isWinner) {
                    entryStatus = 'shadowed';
                } else if (String(below.value).trim() === String(entry.value).trim()) {
                    entryStatus = 'redundant';
                } else {
                    entryStatus = 'override';
                }

                /** @type {string[]} */
                const problems = [];
                if (entry.kind === 'constexpr' && !below && (entry.shorthand || !entry.type)) {
                    problems.push(
                        'Introduces a new constexpr without a "type". The generator exits with -1.'
                    );
                }
                if (entry.shorthand && !below && entry.kind === 'define') {
                    problems.push('Introduces a new define with no description.');
                }

                const substitutions = configureSubstitutions(entry.value);

                const bucket = groupMap.get(entry.group) ?? [];
                bucket.push({
                    key: entry.key,
                    value: entry.value,
                    substitutions,
                    shorthand: entry.shorthand,
                    type: entry.type ?? (below ? below.type : undefined),
                    desc: entry.desc ?? (below ? below.desc : undefined),
                    ownType: entry.type,
                    ownDesc: entry.desc,
                    output:
                        entry.output ??
                        (winner ? winner.output : config.defaultOutput ?? 'private'),
                    ownOutput: entry.output,
                    namespace: entry.namespace,
                    kind: entry.kind,
                    group: entry.group,
                    presetIndex,
                    configIndex,
                    status: entryStatus,
                    inherited: below
                        ? {
                              value: below.value,
                              priority: below.priority,
                              file: labelFor(index, below.fileId),
                              fileId: below.fileId
                          }
                        : null,
                    effective: winner
                        ? {
                              value: winner.value,
                              priority: winner.priority,
                              file: labelFor(index, winner.fileId),
                              fileId: winner.fileId,
                              isThisFile: isWinner
                          }
                        : null,
                    problems
                });
                groupMap.set(entry.group, bucket);
            }

            /** @type {any[]} */
            const inheritedOnly = [];
            if (showInherited && !isDefaultLayer) {
                const own = new Set(config.entries.map((entry) => entry.key));
                for (const entry of inherited.entries.values()) {
                    if (own.has(entry.key)) {
                        continue;
                    }
                    inheritedOnly.push({
                        key: entry.key,
                        value: entry.value,
                        type: entry.type,
                        desc: entry.desc,
                        kind: entry.kind,
                        output: entry.output,
                        group: entry.group,
                        file: labelFor(index, entry.fileId),
                        fileId: entry.fileId,
                        priority: entry.priority
                    });
                }
                inheritedOnly.sort((a, b) => a.key.localeCompare(b.key));
            }

            const groups = [...groupMap.entries()].map(([name, entries]) => ({
                name,
                kind: name === DEFINES_GROUP ? 'define' : 'constexpr',
                entries
            }));

            configs.push({
                index: configIndex,
                presetIndex,
                targetName,
                namespace: config.constexprNamespace,
                defaultOutput: config.defaultOutput,
                groups,
                inheritedOnly,
                isDefaultLayer,
                consumers: (targetInfo?.consumers ?? []).map((consumer) => ({
                    cmakeTarget: consumer.cmakeTarget,
                    outputName: consumer.outputName,
                    declaredIn: shortPath(index, consumer.declaredIn),
                    conditional: consumer.conditional
                })),
                resolvedNamespace: effective.namespace,
                totalResolved: effective.entries.size
            });
        });

        presets.push({
            index: presetIndex,
            name: preset.name,
            configs
        });
    });

    /** @type {string[]} */
    const groupSuggestions = new Set();
    for (const other of index.files.values()) {
        for (const preset of other.doc?.presets ?? []) {
            for (const config of preset.configs) {
                for (const group of config.groups) {
                    groupSuggestions.add(group);
                }
            }
        }
    }

    return {
        ok: true,
        fileName: path.basename(file.uri.fsPath),
        relPath,
        version: file.doc.version,
        priority: file.doc.priority,
        presets,
        registration: {
            registered: status.registered,
            order: status.registration?.order,
            conditional: status.registration?.conditional ?? false,
            declaredIn: status.registration ? shortPath(index, status.registration.declaredIn) : undefined,
            asDefaultFor: status.asDefaultFor,
            cmakeKnown: index.registrations.length > 0
        },
        knownTargets: [...index.targets.keys()].sort(),
        knownPresets: [...index.presetNames].sort(),
        groupSuggestions: [...groupSuggestions].sort(),
        siblingFiles: index
            .allFiles()
            .filter((other) => other.id !== file.id && other.doc)
            .map((other) => ({ id: other.id, label: labelFor(index, other.id) })),
        defaultGroups: [DEFINES_GROUP, CONSTEXPR_GROUP]
    };
}

/**
 * A short, stable label: `<parent-dir>/<file>`, which is enough to tell
 * `core/dev_preset.json` from `game/dev_preset.json` without the full path.
 * @param {import('./workspaceIndex').WorkspaceIndex} index
 * @param {string} fileId
 */
function labelFor(index, fileId) {
    if (!fileId) {
        return '';
    }
    return `${path.basename(path.dirname(fileId))}/${path.basename(fileId)}`;
}

/**
 * @param {import('./workspaceIndex').WorkspaceIndex} index
 * @param {string} absPath
 */
function shortPath(index, absPath) {
    for (const file of index.files.values()) {
        if (file.folder && absPath.startsWith(file.folder.uri.fsPath)) {
            return path.relative(file.folder.uri.fsPath, absPath);
        }
    }
    return absPath;
}

module.exports = { buildEditorModel, labelFor, shortPath, isConstexprGroup };
