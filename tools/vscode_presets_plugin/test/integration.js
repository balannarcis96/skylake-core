/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2025 Balan Narcis (balannarcis96@gmail.com)
 *
 * End-to-end exercise of the host-side modules against a real preset tree.
 *
 *   node test/integration.js <repo-root>
 *
 * Runs the workspace index, the editor view model, every mutation and the
 * diagnostics pass, then asserts the invariants that matter. Mutations are
 * applied to in-memory copies; nothing on disk is touched.
 */
'use strict';

const fs = require('fs');
const path = require('path');

const repoRoot = path.resolve(process.argv[2] ?? '.');
require('./vscode-stub').install([repoRoot]);

const { WorkspaceIndex } = require('../src/core/workspaceIndex');
const { buildEditorModel } = require('../src/core/editorModel');
const { DiagnosticsProvider } = require('../src/providers/diagnostics');
const { TuningTreeProvider } = require('../src/providers/tree');
const { parseTree } = require('../src/core/jsonAst');
const { detectStyle, stringify } = require('../src/core/serialize');
const mutations = require('../src/core/mutations');

let failures = 0;
let checks = 0;

function ok(message, detail) {
    checks++;
    console.log(`  ✓ ${message}${detail ? ` — ${detail}` : ''}`);
}

function fail(message, detail) {
    checks++;
    failures++;
    console.log(`  ✗ ${message}`);
    if (detail) {
        String(detail).split('\n').slice(0, 12).forEach((line) => console.log(`      ${line}`));
    }
}

function assert(condition, message, detail) {
    if (condition) {
        ok(message);
    } else {
        fail(message, detail);
    }
}

function section(title) {
    console.log(`\n${title}\n${'-'.repeat(title.length)}`);
}

(async () => {
    const index = new WorkspaceIndex();
    await index.refresh();

    // ---------------------------------------------------------------------
    section('1. Index');
    assert(index.files.size > 0, `discovered preset files (${index.files.size})`);
    assert(index.targets.size > 0, `discovered targets (${index.targets.size})`);
    ok('preset names', [...index.presetNames].sort().join(', '));
    ok(
        'targets',
        [...index.targets.keys()]
            .sort()
            .map((name) => {
                const info = index.targets.get(name);
                return `${name}(${info.consumers.length}c/${info.fileIds.length}f)`;
            })
            .join(' ')
    );

    // ---------------------------------------------------------------------
    section('2. Registration status');
    for (const file of index.allFiles()) {
        const status = index.registrationStatus(file.id);
        const tag = status.registered
            ? `#${status.registration.order}${status.registration.conditional ? ' (conditional)' : ''}`
            : status.asDefaultFor.length > 0
              ? `default layer for ${status.asDefaultFor.join(',')}`
              : 'UNREGISTERED';
        console.log(`      ${short(file.id).padEnd(42)} prio ${String(file.doc?.priority ?? '?').padEnd(2)} ${tag}`);
    }
    ok('registration status computed for every file');

    // ---------------------------------------------------------------------
    section('3. Editor view model');
    let modelled = 0;
    for (const file of index.allFiles()) {
        const model = buildEditorModel(index, file);
        if (!model.ok) {
            fail(`model failed for ${short(file.id)}`, model.parseError?.message);
            continue;
        }
        modelled++;

        for (const preset of model.presets) {
            for (const config of preset.configs) {
                for (const group of config.groups) {
                    for (const entry of group.entries) {
                        if (!['introduced', 'override', 'redundant', 'shadowed'].includes(entry.status)) {
                            fail(`bad status '${entry.status}' on ${entry.key} in ${short(file.id)}`);
                        }
                        if (entry.status !== 'introduced' && !entry.inherited) {
                            fail(`status ${entry.status} without an inherited value: ${entry.key}`);
                        }
                    }
                }
            }
        }
    }
    assert(modelled === index.files.size, `built a view model for all ${modelled} files`);

    // Statuses across the whole workspace, as a sanity read-out.
    const tally = { introduced: 0, override: 0, redundant: 0, shadowed: 0 };
    for (const file of index.allFiles()) {
        const model = buildEditorModel(index, file);
        for (const preset of model.presets ?? []) {
            for (const config of preset.configs) {
                for (const group of config.groups) {
                    for (const entry of group.entries) {
                        tally[entry.status]++;
                    }
                }
            }
        }
    }
    ok('entry statuses', Object.entries(tally).map(([k, v]) => `${k}=${v}`).join(' '));

    // ---------------------------------------------------------------------
    section('4. Resolution invariants');
    let resolveChecks = 0;
    for (const [targetName, info] of index.targets) {
        for (const presetName of info.presetNames) {
            const result = index.resolve(targetName, presetName);
            resolveChecks++;
            for (const entry of result.entries.values()) {
                const applied = entry.trace.filter((contribution) => contribution.applied);
                if (applied.length === 0) {
                    fail(`${targetName}/${presetName}: ${entry.key} has no applied contribution`);
                }
                const last = applied[applied.length - 1];
                if (last.value !== entry.value || last.fileId !== entry.fileId) {
                    fail(
                        `${targetName}/${presetName}: ${entry.key} winner disagrees with its trace`,
                        `winner=${entry.value}@${short(entry.fileId)} trace=${last.value}@${short(last.fileId)}`
                    );
                }
                // The generator's rule: a later contribution only wins with a
                // strictly greater priority.
                let running = -1;
                for (const contribution of entry.trace) {
                    if (contribution.applied) {
                        if (contribution.priority <= running) {
                            fail(
                                `${targetName}/${presetName}: ${entry.key} applied a non-increasing priority`,
                                `${running} -> ${contribution.priority}`
                            );
                        }
                        running = contribution.priority;
                    } else if (contribution.priority > running) {
                        fail(
                            `${targetName}/${presetName}: ${entry.key} skipped a higher priority`,
                            `running=${running} skipped=${contribution.priority}`
                        );
                    }
                }
            }
        }
    }
    ok(`resolution invariants hold across ${resolveChecks} (target, preset) pairs`);

    // ---------------------------------------------------------------------
    section('5. Mutations (in memory, style preserving)');
    const sample = index
        .allFiles()
        .find((file) => file.doc?.presets.some((preset) => preset.configs.some((config) => config.entries.length > 2)));

    if (!sample) {
        fail('no file with enough entries to exercise mutations');
    } else {
        const text = sample.text;
        const style = detectStyle(text);
        const raw = parseTree(text).value;
        const presetIndex = sample.doc.presets.findIndex((preset) =>
            preset.configs.some((config) => config.entries.length > 2)
        );
        const configIndex = sample.doc.presets[presetIndex].configs.findIndex(
            (config) => config.entries.length > 2
        );
        const config = sample.doc.presets[presetIndex].configs[configIndex];
        const entry = config.entries[0];

        ok(`sample: ${short(sample.id)} preset='${sample.doc.presets[presetIndex].name}' target='${config.targetName}' key='${entry.key}'`);

        // no-op write
        assert(stringify(raw, style) === text, 'identity write reproduces the file byte-for-byte');

        // value change
        const changed = mutations.setEntryValue(
            raw, presetIndex, configIndex, entry.group, entry.key, 'SENTINEL_VALUE'
        );
        const changedText = stringify(changed, style);
        assert(changedText !== text, 'value change produces different text');
        assert(
            countDiffLines(text, changedText) === 2,
            'value change touches exactly one line',
            `changed ${countDiffLines(text, changedText)} line halves`
        );
        assert(
            parseTree(changedText).value.presets[presetIndex].config[configIndex][entry.group][entry.key] !== undefined,
            'value change keeps the key present'
        );
        assert(raw.presets[presetIndex].config[configIndex][entry.group][entry.key] !== 'SENTINEL_VALUE'
            || typeof raw.presets[presetIndex].config[configIndex][entry.group][entry.key] === 'object',
            'mutation did not modify the input in place');

        // rename preserves position
        const renamed = mutations.renameEntry(
            raw, presetIndex, configIndex, entry.group, entry.key, 'RENAMED_KEY'
        );
        const renamedKeys = Object.keys(renamed.presets[presetIndex].config[configIndex][entry.group]);
        const originalKeys = Object.keys(raw.presets[presetIndex].config[configIndex][entry.group]);
        assert(
            renamedKeys.indexOf('RENAMED_KEY') === originalKeys.indexOf(entry.key),
            'rename keeps the key in its original position'
        );

        // add / delete round-trip
        const added = mutations.addEntry(
            raw, presetIndex, configIndex, entry.group, 'BRAND_NEW_KEY',
            mutations.makeNewEntry({ value: '1U', type: 'u32', desc: 'test', kind: 'constexpr' })
        );
        assert(
            'BRAND_NEW_KEY' in added.presets[presetIndex].config[configIndex][entry.group],
            'addEntry inserts the key'
        );
        const removed = mutations.deleteEntry(added, presetIndex, configIndex, entry.group, 'BRAND_NEW_KEY');
        assert(stringify(removed, style) === text, 'add followed by delete restores the exact original bytes');

        // meta promote and collapse
        const promoted = mutations.setEntryMeta(
            raw, presetIndex, configIndex, entry.group, entry.key, { desc: 'temporary' }
        );
        const promotedEntry = promoted.presets[presetIndex].config[configIndex][entry.group][entry.key];
        assert(typeof promotedEntry === 'object', 'setting metadata promotes a shorthand entry to object form');
        const collapsed = mutations.setEntryMeta(
            promoted, presetIndex, configIndex, entry.group, entry.key,
            { desc: null, type: null, output: null, namespace: null }
        );
        const collapsedEntry = collapsed.presets[presetIndex].config[configIndex][entry.group][entry.key];
        if (entry.shorthand) {
            assert(typeof collapsedEntry === 'string', 'clearing all metadata collapses back to shorthand');
        } else {
            ok('entry was already in object form, collapse rule not applicable');
        }

        // group move
        if (config.groups.length > 1) {
            const other = config.groups.find((group) => group !== entry.group);
            const moved = mutations.moveEntry(raw, presetIndex, configIndex, entry.group, other, entry.key);
            assert(
                !(entry.key in moved.presets[presetIndex].config[configIndex][entry.group]) &&
                    entry.key in moved.presets[presetIndex].config[configIndex][other],
                `moveEntry relocates the key from ${entry.group} to ${other}`
            );
        }

        // every mutated document must still be a valid preset document
        for (const [label, candidate] of [
            ['changed', changed], ['renamed', renamed], ['added', added], ['promoted', promoted]
        ]) {
            try {
                parseTree(stringify(candidate, style));
                ok(`'${label}' serializes to valid JSON`);
            } catch (error) {
                fail(`'${label}' produced invalid JSON`, String(error));
            }
        }
    }

    // ---------------------------------------------------------------------
    section('6. Diagnostics');
    const collections = [];
    const diagnostics = new DiagnosticsProvider(
        { subscriptions: { push: (...items) => collections.push(...items) } },
        index
    );
    diagnostics.refresh();

    const store = diagnostics.collection.all;
    let total = 0;
    /** @type {Map<string, number>} */
    const byCode = new Map();
    for (const [file, items] of store) {
        total += items.length;
        for (const item of items) {
            byCode.set(item.code, (byCode.get(item.code) ?? 0) + 1);
            if (item.severity === 0) {
                console.log(`      ERROR ${short(file)}: ${item.message.slice(0, 110)}`);
            }
        }
    }
    ok(`produced ${total} diagnostics across ${store.size} files`);
    for (const [code, count] of [...byCode].sort()) {
        console.log(`      ${String(count).padStart(3)}  ${code}`);
    }

    // ---------------------------------------------------------------------
    section('7. Tree');
    const tree = new TuningTreeProvider(index);
    const roots = tree.getChildren();
    assert(roots.length === 2, 'tree has Targets and Files sections');
    let nodes = 0;
    const walkTree = (node, depth) => {
        nodes++;
        tree.getTreeItem(node);
        if (depth > 3) {
            return;
        }
        for (const child of tree.getChildren(node)) {
            walkTree(child, depth + 1);
        }
    };
    for (const root of roots) {
        walkTree(root, 0);
    }
    ok(`walked ${nodes} tree nodes without throwing`);

    // ---------------------------------------------------------------------
    section('Summary');
    console.log(`${checks - failures}/${checks} checks passed`);
    if (failures > 0) {
        console.log(`${failures} FAILURE(S)`);
        process.exit(1);
    }
    console.log('all good');
})().catch((error) => {
    console.error('\nUNCAUGHT:', error && error.stack ? error.stack : error);
    process.exit(1);
});

/** @param {string} fileId */
function short(fileId) {
    return `${path.basename(path.dirname(fileId))}/${path.basename(fileId)}`;
}

/**
 * Count how many lines differ between two texts.
 * @param {string} a
 * @param {string} b
 */
function countDiffLines(a, b) {
    const left = a.split('\n');
    const right = b.split('\n');
    let count = 0;
    for (let i = 0; i < Math.max(left.length, right.length); i++) {
        if (left[i] !== right[i]) {
            count += 2;
        }
    }
    return count;
}

void fs;
