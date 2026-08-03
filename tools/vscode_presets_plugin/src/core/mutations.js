/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2025 Balan Narcis (balannarcis96@gmail.com)
 *
 * Pure edit operations over a parsed preset document.
 *
 * Each function takes the plain JSON value, returns a NEW value, and never
 * mutates its input, so the caller can diff old against new and skip a
 * workspace edit when nothing changed.
 *
 * Key ORDER is load-bearing here. It has no semantic meaning to the generator,
 * but it is what the reviewer of the resulting diff sees, so every operation
 * preserves position: renaming a key leaves it where it was rather than moving
 * it to the end, and inserting keeps the surrounding block stable.
 */
'use strict';

const { isEntryGroup, CONSTEXPR_GROUP, DEFINES_GROUP } = require('./schema');

/**
 * Structured clone that keeps plain objects plain and preserves key order.
 * @template T
 * @param {T} value
 * @returns {T}
 */
function clone(value) {
    if (Array.isArray(value)) {
        return /** @type {any} */ (value.map(clone));
    }
    if (value && typeof value === 'object') {
        /** @type {Record<string, any>} */
        const out = {};
        for (const key of Object.keys(value)) {
            out[key] = clone(/** @type {any} */ (value)[key]);
        }
        return /** @type {any} */ (out);
    }
    return value;
}

/**
 * Rebuild an object with `oldKey` renamed to `newKey`, in place.
 * @param {Record<string, any>} obj
 * @param {string} oldKey
 * @param {string} newKey
 * @returns {Record<string, any>}
 */
function renameKeyPreservingOrder(obj, oldKey, newKey) {
    /** @type {Record<string, any>} */
    const out = {};
    for (const key of Object.keys(obj)) {
        if (key === oldKey) {
            out[newKey] = obj[oldKey];
        } else {
            out[key] = obj[key];
        }
    }
    return out;
}

/**
 * Insert `key` immediately after `afterKey`, or append when absent.
 * @param {Record<string, any>} obj
 * @param {string} key
 * @param {*} value
 * @param {string} [afterKey]
 * @returns {Record<string, any>}
 */
function insertAfter(obj, key, value, afterKey) {
    if (afterKey === undefined || !(afterKey in obj)) {
        return { ...obj, [key]: value };
    }
    /** @type {Record<string, any>} */
    const out = {};
    for (const existing of Object.keys(obj)) {
        if (existing === key) {
            continue;
        }
        out[existing] = obj[existing];
        if (existing === afterKey) {
            out[key] = value;
        }
    }
    return out;
}

/**
 * Navigate to a config block, returning the mutable clone and the block.
 * @param {*} raw
 * @param {number} presetIndex
 * @param {number} configIndex
 */
function locateConfig(raw, presetIndex, configIndex) {
    const next = clone(raw);
    const preset = next?.presets?.[presetIndex];
    const config = preset?.config?.[configIndex];
    return { next, preset, config };
}

/**
 * @param {*} raw
 * @param {{version?: number, priority?: number}} patch
 */
function setHeader(raw, patch) {
    const next = clone(raw);
    // Preserve the string-vs-number spelling already used by the file; the
    // generator coerces with int() either way, but a gratuitous type flip
    // shows up as noise in the diff.
    if (patch.version !== undefined) {
        next.version = typeof next.version === 'number' ? patch.version : String(patch.version);
    }
    if (patch.priority !== undefined) {
        next.priority = typeof next.priority === 'number' ? patch.priority : String(patch.priority);
    }
    return next;
}

/**
 * @param {*} raw
 * @param {number} presetIndex
 * @param {string} name
 */
function setPresetName(raw, presetIndex, name) {
    const next = clone(raw);
    if (next?.presets?.[presetIndex]) {
        next.presets[presetIndex].name = name;
    }
    return next;
}

/**
 * @param {*} raw
 * @param {number} presetIndex
 * @param {number} configIndex
 * @param {'target_name'|'constexpr_namespace'|'default_output'} field
 * @param {string|undefined} value
 */
function setConfigField(raw, presetIndex, configIndex, field, value) {
    const { next, config } = locateConfig(raw, presetIndex, configIndex);
    if (!config) {
        return next;
    }
    if (value === undefined) {
        delete config[field];
    } else {
        config[field] = value;
    }
    return next;
}

/**
 * Change an entry's value, keeping its current shape (shorthand vs full form).
 * @param {*} raw
 * @param {number} presetIndex
 * @param {number} configIndex
 * @param {string} group
 * @param {string} key
 * @param {string} value
 */
function setEntryValue(raw, presetIndex, configIndex, group, key, value) {
    const { next, config } = locateConfig(raw, presetIndex, configIndex);
    const bucket = config?.[group];
    if (!bucket || !(key in bucket)) {
        return next;
    }
    const current = bucket[key];
    if (current && typeof current === 'object' && !Array.isArray(current)) {
        current.value = value;
    } else {
        bucket[key] = value;
    }
    return next;
}

/**
 * Patch an entry's metadata. Setting any of these on a shorthand entry
 * promotes it to the full object form, which is what the generator needs in
 * order to honour them.
 *
 * @param {*} raw
 * @param {number} presetIndex
 * @param {number} configIndex
 * @param {string} group
 * @param {string} key
 * @param {{type?: string|null, desc?: string|null, output?: string|null, namespace?: string|null}} patch
 */
function setEntryMeta(raw, presetIndex, configIndex, group, key, patch) {
    const { next, config } = locateConfig(raw, presetIndex, configIndex);
    const bucket = config?.[group];
    if (!bucket || !(key in bucket)) {
        return next;
    }

    let entry = bucket[key];
    if (typeof entry !== 'object' || entry === null || Array.isArray(entry)) {
        entry = { value: String(entry) };
        bucket[key] = entry;
    }

    for (const field of /** @type {const} */ (['type', 'desc', 'output', 'namespace'])) {
        const value = patch[field];
        if (value === undefined) {
            continue;
        }
        if (value === null || value === '') {
            delete entry[field];
        } else {
            entry[field] = value;
        }
    }

    // Collapse back to shorthand when nothing but `value` survives, so the file
    // stays in the terse style the override presets use.
    const remaining = Object.keys(entry);
    if (remaining.length === 1 && remaining[0] === 'value') {
        bucket[key] = String(entry.value);
    }
    return next;
}

/**
 * @param {*} raw
 * @param {number} presetIndex
 * @param {number} configIndex
 * @param {string} group
 * @param {string} key
 * @param {string} newKey
 */
function renameEntry(raw, presetIndex, configIndex, group, key, newKey) {
    const { next, config } = locateConfig(raw, presetIndex, configIndex);
    const bucket = config?.[group];
    if (!bucket || !(key in bucket) || key === newKey) {
        return next;
    }
    config[group] = renameKeyPreservingOrder(bucket, key, newKey);
    return next;
}

/**
 * @param {*} raw
 * @param {number} presetIndex
 * @param {number} configIndex
 * @param {string} group
 * @param {string} key
 */
function deleteEntry(raw, presetIndex, configIndex, group, key) {
    const { next, config } = locateConfig(raw, presetIndex, configIndex);
    const bucket = config?.[group];
    if (bucket) {
        delete bucket[key];
    }
    return next;
}

/**
 * Add an entry, creating the group if needed.
 * @param {*} raw
 * @param {number} presetIndex
 * @param {number} configIndex
 * @param {string} group
 * @param {string} key
 * @param {string|Record<string, any>} entry
 * @param {string} [afterKey]
 */
function addEntry(raw, presetIndex, configIndex, group, key, entry, afterKey) {
    const { next, config } = locateConfig(raw, presetIndex, configIndex);
    if (!config) {
        return next;
    }
    if (!config[group] || typeof config[group] !== 'object' || Array.isArray(config[group])) {
        config[group] = {};
    }
    config[group] = insertAfter(config[group], key, entry, afterKey);
    return next;
}

/**
 * Move an entry between groups, preserving its shape.
 * @param {*} raw
 * @param {number} presetIndex
 * @param {number} configIndex
 * @param {string} fromGroup
 * @param {string} toGroup
 * @param {string} key
 */
function moveEntry(raw, presetIndex, configIndex, fromGroup, toGroup, key) {
    const { next, config } = locateConfig(raw, presetIndex, configIndex);
    const source = config?.[fromGroup];
    if (!config || !source || !(key in source) || fromGroup === toGroup) {
        return next;
    }
    const value = source[key];
    delete source[key];
    if (!config[toGroup] || typeof config[toGroup] !== 'object' || Array.isArray(config[toGroup])) {
        config[toGroup] = {};
    }
    config[toGroup][key] = value;
    return next;
}

/**
 * @param {*} raw
 * @param {number} presetIndex
 * @param {number} configIndex
 * @param {string} group
 */
function addGroup(raw, presetIndex, configIndex, group) {
    const { next, config } = locateConfig(raw, presetIndex, configIndex);
    if (config && !config[group]) {
        config[group] = {};
    }
    return next;
}

/**
 * @param {*} raw
 * @param {number} presetIndex
 * @param {number} configIndex
 * @param {string} group
 */
function deleteGroup(raw, presetIndex, configIndex, group) {
    const { next, config } = locateConfig(raw, presetIndex, configIndex);
    if (config && isEntryGroup(group)) {
        delete config[group];
    }
    return next;
}

/**
 * @param {*} raw
 * @param {number} presetIndex
 * @param {number} configIndex
 * @param {string} oldGroup
 * @param {string} newGroup
 */
function renameGroup(raw, presetIndex, configIndex, oldGroup, newGroup) {
    const { next, config } = locateConfig(raw, presetIndex, configIndex);
    if (!config || !config[oldGroup] || oldGroup === newGroup) {
        return next;
    }
    if (config[newGroup]) {
        // Merge rather than clobber: both buckets flatten into the same table
        // anyway, so a collision is harmless as long as nothing is dropped.
        config[newGroup] = { ...config[newGroup], ...config[oldGroup] };
        delete config[oldGroup];
        return next;
    }
    const replacement = renameKeyPreservingOrder(config, oldGroup, newGroup);
    for (const key of Object.keys(config)) {
        delete config[key];
    }
    Object.assign(config, replacement);
    return next;
}

/**
 * @param {*} raw
 * @param {number} presetIndex
 * @param {{targetName: string, namespace?: string, defaultOutput?: string, withGroups?: boolean}} spec
 */
function addConfig(raw, presetIndex, spec) {
    const next = clone(raw);
    const preset = next?.presets?.[presetIndex];
    if (!preset) {
        return next;
    }
    if (!Array.isArray(preset.config)) {
        preset.config = [];
    }
    /** @type {Record<string, any>} */
    const config = { target_name: spec.targetName };
    if (spec.namespace !== undefined) {
        config.constexpr_namespace = spec.namespace;
    }
    if (spec.defaultOutput !== undefined) {
        config.default_output = spec.defaultOutput;
    }
    if (spec.withGroups !== false) {
        config[DEFINES_GROUP] = {};
        config[CONSTEXPR_GROUP] = {};
    }
    preset.config.push(config);
    return next;
}

/**
 * @param {*} raw
 * @param {number} presetIndex
 * @param {number} configIndex
 */
function deleteConfig(raw, presetIndex, configIndex) {
    const next = clone(raw);
    const preset = next?.presets?.[presetIndex];
    if (Array.isArray(preset?.config)) {
        preset.config.splice(configIndex, 1);
    }
    return next;
}

/**
 * @param {*} raw
 * @param {string} name
 */
function addPreset(raw, name) {
    const next = clone(raw);
    if (!Array.isArray(next.presets)) {
        next.presets = [];
    }
    next.presets.push({ name, config: [] });
    return next;
}

/**
 * @param {*} raw
 * @param {number} presetIndex
 */
function deletePreset(raw, presetIndex) {
    const next = clone(raw);
    if (Array.isArray(next.presets)) {
        next.presets.splice(presetIndex, 1);
    }
    return next;
}

/**
 * Build the entry payload to write when overriding an inherited key into this
 * file. Overrides use the shorthand form, matching the existing dev/qa/prod
 * files, because the full form is only required to INTRODUCE a key.
 *
 * @param {string} value
 * @returns {string}
 */
function makeOverrideEntry(value) {
    return value;
}

/**
 * Build the entry payload for a brand new key. Constexprs must carry `type`
 * and `value` or the generator aborts.
 *
 * @param {{value: string, type?: string, desc?: string, kind: 'define'|'constexpr', output?: string}} spec
 * @returns {string|Record<string, any>}
 */
function makeNewEntry(spec) {
    if (spec.kind === 'define' && !spec.desc && !spec.output) {
        return { value: spec.value, desc: spec.desc ?? '' };
    }
    /** @type {Record<string, any>} */
    const entry = { value: spec.value };
    if (spec.kind === 'constexpr') {
        entry.type = spec.type || 'u32';
    }
    if (spec.desc) {
        entry.desc = spec.desc;
    }
    if (spec.output) {
        entry.output = spec.output;
    }
    return entry;
}

module.exports = {
    clone,
    renameKeyPreservingOrder,
    insertAfter,
    setHeader,
    setPresetName,
    setConfigField,
    setEntryValue,
    setEntryMeta,
    renameEntry,
    deleteEntry,
    addEntry,
    moveEntry,
    addGroup,
    deleteGroup,
    renameGroup,
    addConfig,
    deleteConfig,
    addPreset,
    deletePreset,
    makeOverrideEntry,
    makeNewEntry
};
