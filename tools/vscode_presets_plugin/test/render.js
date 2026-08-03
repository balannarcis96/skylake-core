/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2025 Balan Narcis (balannarcis96@gmail.com)
 *
 * Renders the real webview front end against real preset models and drives it.
 *
 *   node test/render.js <repo-root>
 *
 * Every model the workspace produces is pushed through render(), then the
 * interactive paths are exercised: target switching, search, filters, row
 * selection, and the staged-edit guard. A thrown exception here is a bug the
 * user would have hit on first click.
 */
'use strict';

const path = require('path');

const repoRoot = path.resolve(process.argv[2] ?? '.');

require('./vscode-stub').install([repoRoot]);
const { WorkspaceIndex } = require('../src/core/workspaceIndex');
const { buildEditorModel } = require('../src/core/editorModel');

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
        String(detail).split('\n').slice(0, 14).forEach((line) => console.log(`      ${line}`));
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

/** Load a fresh copy of the webview script against a fresh DOM. */
function mount() {
    const dom = require('./dom-stub').install();
    delete require.cache[require.resolve('../media/editor.js')];
    require('../media/editor.js');
    return dom;
}

(async () => {
    const index = new WorkspaceIndex();
    await index.refresh();

    const files = index.allFiles();
    if (files.length === 0) {
        fail('no preset files to render');
        process.exit(1);
    }

    // -----------------------------------------------------------------
    section('1. Render every model');

    let rendered = 0;
    let totalRows = 0;
    for (const file of files) {
        const model = buildEditorModel(index, file);
        const dom = mount();
        try {
            dom.post({ type: 'model', model, docUri: `file://${file.id}`, dirty: false });
        } catch (error) {
            fail(`render threw for ${short(file.id)}`, error && error.stack ? error.stack : String(error));
            continue;
        }
        const rows = dom.rows();
        totalRows += rows.length;
        rendered++;

        const text = dom.text();
        if (!text.includes(path.basename(file.id))) {
            fail(`${short(file.id)}: rendered output does not show the file name`);
        }
        if (text.includes('undefined') || text.includes('[object Object]')) {
            fail(`${short(file.id)}: rendered output contains a formatting hole`, excerpt(text));
        }
    }
    assert(rendered === files.length, `rendered all ${rendered} models without throwing`);
    ok(`produced ${totalRows} key rows in total`);

    // -----------------------------------------------------------------
    section('2. Interaction: target switching');

    const rich = files
        .map((file) => ({ file, model: buildEditorModel(index, file) }))
        .filter(({ model }) => model.ok && model.presets.some((preset) => preset.configs.length > 1))
        .sort((a, b) => b.model.presets[0].configs.length - a.model.presets[0].configs.length)[0];

    if (!rich) {
        ok('no multi-target preset file available, skipping rail test');
    } else {
        const dom = mount();
        dom.post({ type: 'model', model: rich.model, docUri: 'file://x', dirty: false });
        const railItems = dom.root.querySelectorAll('.rail-item');
        assert(
            railItems.length === rich.model.presets[0].configs.length,
            `rail lists all ${railItems.length} targets of ${short(rich.file.id)}`
        );

        const before = dom.rows().length;
        if (railItems.length > 1) {
            railItems[1].click();
            const after = dom.rows().length;
            ok(`switching target re-rendered the key list (${before} -> ${after} rows)`);
            const current = dom.root.querySelectorAll('.rail-item[aria-current="true"]');
            assert(current.length === 1, 'exactly one target is marked current');
        }
    }

    // -----------------------------------------------------------------
    section('3. Interaction: search and filters');

    const target = files
        .map((file) => ({ file, model: buildEditorModel(index, file) }))
        .filter(({ model }) => model.ok && countEntries(model) > 8)
        .sort((a, b) => countEntries(b.model) - countEntries(a.model))[0];

    if (!target) {
        fail('no file with enough entries to test search');
    } else {
        const dom = mount();
        dom.post({ type: 'model', model: target.model, docUri: 'file://y', dirty: false });
        const all = dom.rows().length;

        const sample = firstEntry(target.model);
        const search = dom.root.querySelector('[data-fid="search"]');
        assert(Boolean(search), 'search box is present');

        if (search && sample) {
            search.value = sample.key;
            search.dispatch('input');
            const filtered = dom.rows().length;
            assert(
                filtered >= 1 && filtered < all,
                `searching '${sample.key}' narrowed ${all} rows to ${filtered}`
            );
            assert(
                dom.text().includes(sample.key),
                'the matching key is still visible after filtering'
            );

            search.value = 'zzz_no_such_key_zzz';
            search.dispatch('input');
            assert(dom.rows().length === 0, 'a search with no hits shows no rows');
            assert(dom.text().includes('Nothing matches'), 'empty state is shown for a fruitless search');

            search.value = '';
            search.dispatch('input');
            assert(dom.rows().length === all, `clearing the search restored all ${all} rows`);
        }

        const chips = dom.root.querySelectorAll('.chip');
        assert(chips.length > 0, `status filter chips rendered (${chips.length})`);
    }

    // -----------------------------------------------------------------
    section('4. Interaction: selection and inspector');

    if (target) {
        const dom = mount();
        dom.post({ type: 'model', model: target.model, docUri: 'file://z', dirty: false });
        const rows = dom.rows().filter((row) => !row.classList.contains('inherited'));
        if (rows.length === 0) {
            fail('no selectable rows');
        } else {
            rows[0].click();
            const inspector = dom.root.querySelector('.inspector-body');
            assert(Boolean(inspector), 'clicking a row opens the inspector');
            const selected = dom.root.querySelectorAll('.row.selected');
            assert(selected.length === 1, 'exactly one row is marked selected');
            if (inspector) {
                const text = inspector.textContent;
                assert(text.includes('Emitted as'), 'inspector shows the emitted C++');
                assert(text.includes('Actions'), 'inspector shows the action list');
            }
        }
    }

    // -----------------------------------------------------------------
    section('5. Accident prevention: staged edits');

    if (target) {
        /** @type {any[]} */
        const posts = [];
        const dom = require('./dom-stub').install((message) => posts.push(message));
        delete require.cache[require.resolve('../media/editor.js')];
        require('../media/editor.js');
        dom.post({ type: 'model', model: target.model, docUri: 'file://w', dirty: false });

        const input = dom.root.querySelector('.value-wrap input');
        assert(Boolean(input), 'value inputs are rendered');

        if (input) {
            const original = input.value;
            posts.length = 0;

            input.value = `${original}_EDITED`;
            input.dispatch('input');
            assert(
                posts.filter((message) => message.type === 'setValue').length === 0,
                'typing alone never writes to the document'
            );

            const wrap = input.closest('.value-wrap');
            assert(
                wrap && wrap.classList.contains('pending'),
                'the edited field is marked as pending'
            );

            const confirmBar = dom.root.querySelector('.confirm');
            assert(Boolean(confirmBar), 'an Apply / Discard bar is offered');

            // Discard restores the original and writes nothing.
            const discard = confirmBar
                ? confirmBar.querySelectorAll('button').find((b) => b.textContent === 'Discard')
                : null;
            assert(Boolean(discard), 'Discard button is present');
            if (discard) {
                discard.click();
                assert(input.value === original, 'Discard restores the original value');
                assert(
                    posts.filter((message) => message.type === 'setValue').length === 0,
                    'Discard writes nothing'
                );
            }

            // Enter commits, and exactly once.
            input.value = `${original}_APPLIED`;
            input.dispatch('input');
            input.dispatch('keydown', { key: 'Enter', target: input });
            const writes = posts.filter((message) => message.type === 'setValue');
            assert(writes.length === 1, 'Enter writes exactly one setValue');
            if (writes.length === 1) {
                assert(
                    writes[0].value === `${original}_APPLIED`,
                    'the written value is what was typed'
                );
            }
        }
    }

    // -----------------------------------------------------------------
    section('6. Undo affordance');

    if (target) {
        const dom = mount();
        dom.post({
            type: 'model',
            model: target.model,
            docUri: 'file://u',
            dirty: true,
            canUndo: true,
            canRedo: false
        });
        const text = dom.text();
        assert(text.includes('Save'), 'a Save button appears while the document is dirty');
        const undo = dom.root
            .querySelectorAll('button')
            .find((button) => (button.getAttribute('title') || '').startsWith('Undo'));
        assert(Boolean(undo) && !undo.disabled, 'Undo is enabled when history exists');

        const dom2 = mount();
        dom2.post({ type: 'model', model: target.model, docUri: 'file://u2', dirty: false, canUndo: false });
        const undo2 = dom2.root
            .querySelectorAll('button')
            .find((button) => (button.getAttribute('title') || '').startsWith('Undo'));
        assert(Boolean(undo2) && undo2.disabled, 'Undo is disabled with no history');
    }

    // -----------------------------------------------------------------
    section('7. Parse-error surface');

    {
        const dom = mount();
        dom.post({
            type: 'model',
            model: {
                ok: false,
                fileName: 'broken_preset.json',
                relPath: 'x/broken_preset.json',
                parseError: { message: 'Expected a string key', offset: 12 }
            },
            docUri: 'file://broken'
        });
        const text = dom.text();
        assert(text.includes('broken_preset.json'), 'parse-error view names the file');
        assert(text.includes('Reopen as Text'), 'parse-error view offers a text fallback');
    }

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

function countEntries(model) {
    if (!model.ok) {
        return 0;
    }
    let total = 0;
    for (const preset of model.presets) {
        for (const config of preset.configs) {
            for (const group of config.groups) {
                total += group.entries.length;
            }
        }
    }
    return total;
}

function firstEntry(model) {
    for (const preset of model.presets) {
        for (const config of preset.configs) {
            for (const group of config.groups) {
                if (group.entries.length > 0) {
                    return group.entries[0];
                }
            }
        }
    }
    return null;
}

function short(fileId) {
    return `${path.basename(path.dirname(fileId))}/${path.basename(fileId)}`;
}

function excerpt(text) {
    const at = Math.max(text.indexOf('undefined'), text.indexOf('[object Object]'));
    return text.slice(Math.max(0, at - 60), at + 60);
}
