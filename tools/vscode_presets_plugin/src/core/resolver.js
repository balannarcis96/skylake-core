/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2025 Balan Narcis (balannarcis96@gmail.com)
 *
 * Faithful port of the merge performed by `cmake/tuning/generate_tuning.py`.
 *
 * Getting this exactly right is the whole point of the extension: the editor
 * claims to show what the compiler will see, so any divergence from the
 * generator is a lie. The rules that are easy to get wrong:
 *
 *   - Priority lives on the FILE, not the entry.
 *   - A later file overrides only when `existing.prio < new.prio`. Equal
 *     priorities keep the incumbent, so ties are won by registration order.
 *   - Files are scanned in registration order, and a file whose preset name or
 *     target_name does not match is skipped in silence.
 *   - Every `constexprs.<suffix>` bucket is flattened into ONE table, so a key
 *     may be declared under `constexprs.hwid` and overridden under plain
 *     `constexprs` with no ill effect.
 *   - The default layer is processed first, under its own preset name.
 *   - `max_prio` (reported as `Priority:` in the generated header) is the
 *     maximum priority of every file SCANNED, including files that contributed
 *     nothing. It is not the winning priority.
 */
'use strict';

const { isConstexprGroup, DEFINES_GROUP } = require('./schema');

/**
 * @typedef {Object} Contribution
 * @property {string} fileId
 * @property {number} priority
 * @property {string} presetName
 * @property {string} value
 * @property {boolean} applied      False when a lower/equal priority lost.
 * @property {string} [reason]      Why it lost.
 * @property {boolean} shorthand
 * @property {(string|number)[]} path
 * @property {boolean} isDefaultLayer
 */

/**
 * @typedef {Object} ResolvedEntry
 * @property {string} key
 * @property {'define'|'constexpr'} kind
 * @property {string} value
 * @property {number} priority
 * @property {string} fileId        File that won.
 * @property {string} group         Group of the winning contribution.
 * @property {string} [type]
 * @property {string} [desc]
 * @property {string} output        'public' | 'private'
 * @property {string} namespace
 * @property {Contribution[]} trace In scan order.
 * @property {boolean} overridden   True when more than one file contributed.
 */

/**
 * @typedef {Object} ResolveInput
 * @property {string} targetName          Value matched against `target_name`.
 * @property {string} presetName          Active preset, e.g. `prod`.
 * @property {string} defaultPresetName   Usually `default`.
 * @property {{id: string, doc: import('./schema').PresetDocument}[]} layers
 *           Default layer first, then registered files in registration order.
 * @property {string} [defaultLayerId]
 */

/**
 * @typedef {Object} ResolveResult
 * @property {Map<string, ResolvedEntry>} entries
 * @property {string} namespace
 * @property {number} maxPriority
 * @property {number} version
 * @property {{fileId: string, reason: string}[]} skipped
 * @property {{fileId: string, key: string, message: string}[]} errors
 */

const DEFAULT_OUTPUT_FALLBACK = 'private';

/**
 * Resolve the effective tuning table for one target under one preset.
 * @param {ResolveInput} input
 * @returns {ResolveResult}
 */
function resolve(input) {
    const { targetName, presetName, defaultPresetName, layers, defaultLayerId } = input;

    /** @type {Map<string, ResolvedEntry>} */
    const entries = new Map();
    /** @type {{fileId: string, reason: string}[]} */
    const skipped = [];
    /** @type {{fileId: string, key: string, message: string}[]} */
    const errors = [];

    let namespaceName = '';
    let namespacePriority = 0;
    let maxPriority = 0;
    let version = 0;

    for (const layer of layers) {
        const { id: fileId, doc } = layer;
        const isDefaultLayer = defaultLayerId !== undefined && fileId === defaultLayerId;
        const wantedPreset = isDefaultLayer ? defaultPresetName : presetName;
        const priority = doc.priority;

        // Mirrors the generator: version/max_prio are bumped from the file
        // header before the preset lookup, so non-contributing files still move
        // them. This is why generated headers report a `Priority:` that no
        // contributing entry actually carries.
        if (doc.version > version) {
            version = doc.version;
        }
        if (priority > maxPriority) {
            maxPriority = priority;
        }

        const preset = doc.presets.find((candidate) => candidate.name === wantedPreset);
        if (!preset) {
            skipped.push({ fileId, reason: `no preset named '${wantedPreset}'` });
            continue;
        }
        const config = preset.configs.find((candidate) => candidate.targetName === targetName);
        if (!config) {
            skipped.push({ fileId, reason: `preset '${wantedPreset}' has no config for '${targetName}'` });
            continue;
        }

        // Namespace resolution has its own priority ladder: the first non-empty
        // value claims it, and only a strictly higher priority can change it.
        if (config.constexprNamespace !== undefined) {
            const candidate = config.constexprNamespace;
            if (namespaceName.trim() === '') {
                namespaceName = candidate;
                namespacePriority = priority;
            } else if (candidate !== namespaceName && namespacePriority < priority) {
                namespaceName = candidate;
                namespacePriority = priority;
            }
        }

        let currentOutput = config.defaultOutput ?? DEFAULT_OUTPUT_FALLBACK;
        if (currentOutput !== 'public' && currentOutput !== 'private') {
            currentOutput = DEFAULT_OUTPUT_FALLBACK;
        }

        for (const entry of config.entries) {
            const existing = entries.get(entry.key);

            /** @type {Contribution} */
            const contribution = {
                fileId,
                priority,
                presetName: wantedPreset,
                value: entry.value,
                applied: false,
                shorthand: entry.shorthand,
                path: entry.path,
                isDefaultLayer
            };

            if (!existing) {
                // A brand new constexpr must carry both `type` and `value`, or
                // the generator prints an error and exits with -1.
                if (entry.kind === 'constexpr' && (entry.shorthand || entry.type === undefined)) {
                    errors.push({
                        fileId,
                        key: entry.key,
                        message:
                            `Constexpr '${entry.key}' is introduced here but is not declared in any ` +
                            `lower-priority layer. New constexpr entries must be an object with ` +
                            `'type' and 'value' — the generator exits with -1 otherwise.`
                    });
                }

                contribution.applied = true;
                entries.set(entry.key, {
                    key: entry.key,
                    kind: entry.kind,
                    value: entry.value,
                    priority,
                    fileId,
                    group: entry.group,
                    type: entry.type,
                    desc: entry.desc,
                    output: entry.output === 'public' || entry.output === 'private'
                        ? entry.output
                        : currentOutput,
                    namespace: entry.namespace ?? '',
                    trace: [contribution],
                    overridden: false
                });
                continue;
            }

            if (existing.priority < priority) {
                contribution.applied = true;
                existing.value = entry.value;
                existing.priority = priority;
                existing.fileId = fileId;
                existing.group = entry.group;
                existing.overridden = true;
                if (!entry.shorthand) {
                    if (entry.desc !== undefined) {
                        existing.desc = entry.desc;
                    }
                    if (entry.type !== undefined) {
                        existing.type = entry.type;
                    }
                    if (entry.output === 'public' || entry.output === 'private') {
                        existing.output = entry.output;
                    }
                }
            } else {
                contribution.reason =
                    `priority ${priority} does not exceed the winning priority ${existing.priority}` +
                    (existing.priority === priority ? ' (ties keep the earlier file)' : '');
            }
            existing.trace.push(contribution);
        }
    }

    // Entries that never declared a namespace inherit the config-level one,
    // matching `_organize_by_namespace`.
    for (const entry of entries.values()) {
        if (entry.namespace.trim() === '') {
            entry.namespace = namespaceName;
        }
    }

    return { entries, namespace: namespaceName, maxPriority, version, skipped, errors };
}

/**
 * Split a resolved table into the two generated headers.
 * @param {ResolveResult} result
 * @returns {{public: ResolvedEntry[], private: ResolvedEntry[]}}
 */
function splitByOutput(result) {
    /** @type {{public: ResolvedEntry[], private: ResolvedEntry[]}} */
    const out = { public: [], private: [] };
    for (const entry of result.entries.values()) {
        out[entry.output === 'public' ? 'public' : 'private'].push(entry);
    }
    return out;
}

/**
 * Reproduce the header a given resolve would generate, for preview purposes.
 * @param {ResolveResult} result
 * @param {{targetName: string, presetName: string, defaultPresetName: string, output: 'public'|'private', fileName: string}} meta
 * @returns {string}
 */
function renderHeader(result, meta) {
    const { targetName, presetName, defaultPresetName, output, fileName } = meta;
    const guard = `SKL_${targetName.toUpperCase().replace(/-/g, '_')}_${presetName.toUpperCase()}_${output.toUpperCase()}_H`;
    const presetDefine = `SKL_CURRENT_PRESET_${presetName.toUpperCase()}`;

    const selected = [...result.entries.values()].filter(
        (entry) => (entry.output === 'public' ? 'public' : 'private') === output
    );
    const defines = selected.filter((entry) => entry.kind === 'define');
    const constexprs = selected.filter((entry) => entry.kind === 'constexpr');

    const lines = [];
    lines.push('/*');
    lines.push(`    Tuning file for ${targetName}`);
    lines.push('');
    lines.push('    ! This file is generated, do not modify !');
    lines.push('');
    lines.push(`    File:           ${fileName}`);
    lines.push(`    Preset:         ${presetName}`);
    lines.push(`    Default Preset: ${defaultPresetName}`);
    lines.push(`    Target:         ${targetName}`);
    lines.push(`    Version:        ${result.version}`);
    lines.push(`    Priority:       ${result.maxPriority}`);
    lines.push('*/');
    lines.push('');
    lines.push(`#ifndef ${guard}`);
    lines.push(`#define ${guard}`);
    lines.push('');
    lines.push(`#ifndef ${presetDefine}`);
    lines.push(`#define ${presetDefine} 1`);
    lines.push('#endif');
    lines.push('');

    if (constexprs.length > 0) {
        lines.push('#include <skl_int>');
        lines.push('');
    }

    if (defines.length > 0) {
        lines.push('/* DEFINES */');
        for (const entry of defines) {
            lines.push('');
            lines.push(`/* ${entry.desc ? `${entry.key} - ${entry.desc}` : entry.key} */`);
            lines.push(`#define ${entry.key} ${entry.value}`);
        }
    }

    if (defines.length > 0 && constexprs.length > 0) {
        lines.push('');
    }

    if (constexprs.length > 0) {
        /** @type {Map<string, ResolvedEntry[]>} */
        const byNamespace = new Map();
        for (const entry of constexprs) {
            const bucket = byNamespace.get(entry.namespace) ?? [];
            bucket.push(entry);
            byNamespace.set(entry.namespace, bucket);
        }
        let written = 0;
        for (const [namespace, bucket] of byNamespace) {
            written++;
            if (namespace.trim() !== '') {
                if (written > 1) {
                    lines.push('');
                }
                lines.push(`namespace ${namespace} {`);
            }
            lines.push('');
            lines.push('/* CONSTANTS */');
            for (const entry of bucket) {
                lines.push('');
                lines.push(`/* ${entry.desc ? `${entry.key} - ${entry.desc}` : entry.key} */`);
                lines.push(`constexpr ${entry.type ?? 'u32'} ${entry.key} = ${entry.value};`);
            }
            if (namespace.trim() !== '') {
                lines.push('');
                lines.push(`} /* ${namespace} */`);
            }
        }
    }

    lines.push('');
    lines.push(`#endif // ${guard}`);
    return lines.join('\n');
}

module.exports = { resolve, splitByOutput, renderHeader, DEFINES_GROUP, isConstexprGroup };
