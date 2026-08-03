/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2025 Balan Narcis (balannarcis96@gmail.com)
 *
 * Domain model for Skylake tuning preset documents.
 *
 * Mirrors what `cmake/tuning/generate_tuning.py` accepts. The important and
 * non-obvious part is that entries come in two shapes:
 *
 *   "KEY": { "value": "...", "type": "u32", "desc": "..." }   full form
 *   "KEY": "..."                                             shorthand
 *
 * The generator only accepts the shorthand for a key that already exists in a
 * lower-priority layer; introducing a brand new key with a shorthand is a hard
 * `exit(-1)`. Diagnostics rely on that distinction, so it is preserved here
 * rather than normalized away.
 */
'use strict';

const DEFINES_GROUP = 'defines';
const CONSTEXPR_GROUP = 'constexprs';
const OUTPUTS = ['public', 'private'];

/**
 * @typedef {Object} PresetEntry
 * @property {string} key
 * @property {string} value
 * @property {boolean} shorthand   True when written as a bare string.
 * @property {string} [type]
 * @property {string} [desc]
 * @property {string} [output]
 * @property {string} [namespace]
 * @property {'define'|'constexpr'} kind
 * @property {string} group        Raw JSON group key, e.g. `constexprs.libsql`.
 * @property {(string|number)[]} path
 */

/**
 * @typedef {Object} PresetConfig
 * @property {string} targetName
 * @property {string|undefined} constexprNamespace
 * @property {string|undefined} defaultOutput
 * @property {string[]} groups     Group keys in document order.
 * @property {PresetEntry[]} entries
 * @property {(string|number)[]} path
 */

/**
 * @typedef {Object} PresetBlock
 * @property {string} name
 * @property {PresetConfig[]} configs
 * @property {(string|number)[]} path
 */

/**
 * @typedef {Object} PresetDocument
 * @property {number} version
 * @property {number} priority
 * @property {PresetBlock[]} presets
 * @property {*} raw            The plain parsed JSON, kept for round-tripping.
 */

/**
 * Is `group` one of the buckets the generator merges into the constexpr table?
 * Every `constexprs.<suffix>` group is flattened into one namespace, so the
 * suffix is presentation only.
 * @param {string} group
 */
function isConstexprGroup(group) {
    return group === CONSTEXPR_GROUP || group.startsWith(CONSTEXPR_GROUP + '.');
}

/** @param {string} group */
function isEntryGroup(group) {
    return group === DEFINES_GROUP || isConstexprGroup(group);
}

/**
 * Structural test used for workspace discovery. Deliberately permissive about
 * everything except the shape the generator actually indexes on, so that a
 * preset file with an unusual name is still picked up.
 * @param {*} raw
 * @returns {boolean}
 */
function looksLikePresetDocument(raw) {
    if (!raw || typeof raw !== 'object' || Array.isArray(raw)) {
        return false;
    }
    if (!Array.isArray(raw.presets) || raw.presets.length === 0) {
        return false;
    }
    return raw.presets.some(
        (preset) =>
            preset &&
            typeof preset === 'object' &&
            typeof preset.name === 'string' &&
            Array.isArray(preset.config) &&
            preset.config.some((cfg) => cfg && typeof cfg.target_name === 'string')
    );
}

/**
 * Does this value get rewritten by CMake after the generator emits it?
 *
 * `skl_add_tune_header_to_target` pipes the generated header through
 * `configure_file()` WITHOUT `@ONLY`, so both `@VAR@` and `${VAR}` are
 * substituted with CMake variables on the way to the final header. Upstream
 * relies on this (`CCurrentCppVersion` is literally `@CMAKE_CXX_STANDARD@`),
 * which also means a value that merely happens to contain those forms will be
 * silently rewritten.
 *
 * @param {string} value
 * @returns {string[]} Referenced CMake variable names, empty when none.
 */
function configureSubstitutions(value) {
    /** @type {string[]} */
    const out = [];
    const text = String(value ?? '');
    for (const match of text.matchAll(/@([A-Za-z0-9_-]+)@/g)) {
        out.push(match[1]);
    }
    for (const match of text.matchAll(/\$\{([A-Za-z0-9_-]+)\}/g)) {
        out.push(match[1]);
    }
    return [...new Set(out)];
}

/**
 * `"1"` and `1` both appear in the wild; the generator coerces with `int()`.
 * @param {*} value
 * @param {number} fallback
 */
function toInt(value, fallback) {
    if (typeof value === 'number' && Number.isFinite(value)) {
        return Math.trunc(value);
    }
    if (typeof value === 'string' && value.trim() !== '') {
        const parsed = Number.parseInt(value, 10);
        if (!Number.isNaN(parsed)) {
            return parsed;
        }
    }
    return fallback;
}

/**
 * Build the structured model from parsed JSON.
 * @param {*} raw
 * @returns {PresetDocument}
 */
function buildDocument(raw) {
    /** @type {PresetBlock[]} */
    const presets = [];

    const rawPresets = Array.isArray(raw.presets) ? raw.presets : [];
    rawPresets.forEach((rawPreset, presetIndex) => {
        if (!rawPreset || typeof rawPreset !== 'object') {
            return;
        }
        const presetPath = ['presets', presetIndex];
        /** @type {PresetConfig[]} */
        const configs = [];

        const rawConfigs = Array.isArray(rawPreset.config) ? rawPreset.config : [];
        rawConfigs.forEach((rawConfig, configIndex) => {
            if (!rawConfig || typeof rawConfig !== 'object') {
                return;
            }
            const configPath = presetPath.concat('config', configIndex);
            /** @type {PresetEntry[]} */
            const entries = [];
            /** @type {string[]} */
            const groups = [];

            for (const group of Object.keys(rawConfig)) {
                if (!isEntryGroup(group)) {
                    continue;
                }
                const bucket = rawConfig[group];
                if (!bucket || typeof bucket !== 'object' || Array.isArray(bucket)) {
                    continue;
                }
                groups.push(group);
                const kind = group === DEFINES_GROUP ? 'define' : 'constexpr';

                for (const key of Object.keys(bucket)) {
                    const rawEntry = bucket[key];
                    const path = configPath.concat(group, key);

                    if (typeof rawEntry === 'string') {
                        entries.push({ key, value: rawEntry, shorthand: true, kind, group, path });
                    } else if (rawEntry && typeof rawEntry === 'object' && !Array.isArray(rawEntry)) {
                        entries.push({
                            key,
                            value: rawEntry.value === undefined ? '' : String(rawEntry.value),
                            shorthand: false,
                            type: typeof rawEntry.type === 'string' ? rawEntry.type : undefined,
                            desc: typeof rawEntry.desc === 'string' ? rawEntry.desc : undefined,
                            output: typeof rawEntry.output === 'string' ? rawEntry.output : undefined,
                            namespace:
                                typeof rawEntry.namespace === 'string' ? rawEntry.namespace : undefined,
                            kind,
                            group,
                            path
                        });
                    } else {
                        // Numbers/booleans/null are accepted by json.load and then
                        // stringified by the generator; keep them visible.
                        entries.push({
                            key,
                            value: String(rawEntry),
                            shorthand: true,
                            kind,
                            group,
                            path
                        });
                    }
                }
            }

            configs.push({
                targetName: String(rawConfig.target_name ?? ''),
                constexprNamespace:
                    typeof rawConfig.constexpr_namespace === 'string'
                        ? rawConfig.constexpr_namespace
                        : undefined,
                defaultOutput:
                    typeof rawConfig.default_output === 'string' ? rawConfig.default_output : undefined,
                groups,
                entries,
                path: configPath
            });
        });

        presets.push({ name: String(rawPreset.name ?? ''), configs, path: presetPath });
    });

    return {
        version: toInt(raw.version, 0),
        priority: toInt(raw.priority, 0),
        presets,
        raw
    };
}

/**
 * @param {PresetDocument} doc
 * @param {string} presetName
 * @returns {PresetBlock|undefined}
 */
function findPreset(doc, presetName) {
    return doc.presets.find((preset) => preset.name === presetName);
}

/**
 * @param {PresetBlock|undefined} preset
 * @param {string} targetName
 * @returns {PresetConfig|undefined}
 */
function findConfig(preset, targetName) {
    return preset?.configs.find((config) => config.targetName === targetName);
}

/**
 * Every `(presetName, targetName)` pair a document contributes.
 * @param {PresetDocument} doc
 * @returns {{presetName: string, targetName: string}[]}
 */
function coverage(doc) {
    const out = [];
    for (const preset of doc.presets) {
        for (const config of preset.configs) {
            out.push({ presetName: preset.name, targetName: config.targetName });
        }
    }
    return out;
}

module.exports = {
    DEFINES_GROUP,
    CONSTEXPR_GROUP,
    OUTPUTS,
    isConstexprGroup,
    isEntryGroup,
    looksLikePresetDocument,
    buildDocument,
    findPreset,
    findConfig,
    coverage,
    configureSubstitutions,
    toInt
};
