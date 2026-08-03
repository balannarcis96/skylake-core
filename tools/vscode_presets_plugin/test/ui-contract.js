/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2025 Balan Narcis (balannarcis96@gmail.com)
 *
 * Static contract checks between the webview and the extension host.
 *
 * The webview talks to the host through stringly-typed messages and renders
 * with stringly-typed class and icon names. None of that is checked by the
 * runtime: a typo turns a button into a no-op that still looks fine. These
 * checks close that gap.
 *
 *   node test/ui-contract.js
 */
'use strict';

const fs = require('fs');
const path = require('path');

const root = path.dirname(__dirname);
const editorJs = fs.readFileSync(path.join(root, 'media', 'editor.js'), 'utf8');
const editorCss = fs.readFileSync(path.join(root, 'media', 'editor.css'), 'utf8');
const providerJs = fs.readFileSync(path.join(root, 'src', 'providers', 'presetEditor.js'), 'utf8');
const manifest = JSON.parse(fs.readFileSync(path.join(root, 'package.json'), 'utf8'));

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
        String(detail).split('\n').slice(0, 15).forEach((line) => console.log(`      ${line}`));
    }
}

function section(title) {
    console.log(`\n${title}\n${'-'.repeat(title.length)}`);
}

// ---------------------------------------------------------------------------
section('1. Webview -> host message contract');

// Any message-shaped object literal in the webview. Matching `send({...})`
// directly would miss the ones built by helpers and passed on by reference.
const sent = new Set();
for (const match of editorJs.matchAll(/\btype:\s*'([A-Za-z]+)'/g)) {
    sent.add(match[1]);
}
for (const match of editorJs.matchAll(/type:\s*event\.shiftKey\s*\?\s*'([A-Za-z]+)'\s*:\s*'([A-Za-z]+)'/g)) {
    sent.add(match[1]);
    sent.add(match[2]);
}
// `message.type === 'model'` style comparisons are inbound, not outbound.
for (const match of editorJs.matchAll(/message\.type === '([A-Za-z]+)'/g)) {
    sent.delete(match[1]);
}
// `type:` is also an HTML attribute in el() calls; those are not messages.
for (const htmlType of ['text', 'search', 'checkbox', 'radio', 'number', 'button', 'submit']) {
    sent.delete(htmlType);
}

// Every `case 'x':` inside the host's message switch.
const handled = new Set();
const switchStart = providerJs.indexOf('switch (message?.type)');
if (switchStart === -1) {
    fail('could not locate the host message switch');
} else {
    const body = providerJs.slice(switchStart, providerJs.indexOf('\n    }', switchStart));
    for (const match of body.matchAll(/case '([A-Za-z]+)':/g)) {
        handled.add(match[1]);
    }
}

const unhandled = [...sent].filter((type) => !handled.has(type));
if (unhandled.length === 0) {
    ok(`all ${sent.size} message types have a host handler`, [...sent].sort().join(', '));
} else {
    fail('webview sends message types the host ignores', unhandled.join('\n'));
}

const unused = [...handled].filter((type) => !sent.has(type));
if (unused.length > 0) {
    ok(`host handles ${unused.length} type(s) the webview never sends`, unused.join(', '));
}

// ---------------------------------------------------------------------------
section('2. Host -> webview message contract');

const posted = new Set();
for (const match of providerJs.matchAll(/postMessage\(\s*\{\s*type:\s*'([A-Za-z]+)'/g)) {
    posted.add(match[1]);
}
const received = new Set();
for (const match of editorJs.matchAll(/message\.type === '([A-Za-z]+)'/g)) {
    received.add(match[1]);
}
const dropped = [...posted].filter((type) => !received.has(type));
if (dropped.length === 0) {
    ok(`webview handles all ${posted.size} posted message types`, [...posted].sort().join(', '));
} else {
    fail('host posts message types the webview drops', dropped.join('\n'));
}

// ---------------------------------------------------------------------------
section('3. Icons');

const iconBlock = /const ICONS = \{([\s\S]*?)\n {4}\};/.exec(editorJs);
if (!iconBlock) {
    fail('could not locate the ICONS table');
} else {
    const defined = new Set();
    for (const match of iconBlock[1].matchAll(/^\s{8}([A-Za-z]+):/gm)) {
        defined.add(match[1]);
    }
    const used = new Set();
    for (const match of editorJs.matchAll(/\bicon\('([A-Za-z]+)'/g)) {
        used.add(match[1]);
    }
    // The inspector routes icon names through an `act(label, iconName, …)`
    // helper, so they never appear as a literal argument to icon().
    for (const match of editorJs.matchAll(/\bact\(\s*'[^']*',\s*'([A-Za-z]+)'/g)) {
        used.add(match[1]);
    }
    const missing = [...used].filter((name) => !defined.has(name));
    if (missing.length === 0) {
        ok(`all ${used.size} referenced icons are defined`);
    } else {
        fail('icon() called with undefined names', missing.join(', '));
    }
    const unusedIcons = [...defined].filter((name) => !used.has(name));
    if (unusedIcons.length > 0) {
        fail('icons defined but never used (dead weight)', unusedIcons.join(', '));
    } else {
        ok('no unused icons');
    }
}

// ---------------------------------------------------------------------------
section('4. CSS classes');

// Class names the stylesheet defines.
const cssClasses = new Set();
for (const match of editorCss.matchAll(/\.([a-zA-Z][a-zA-Z0-9_-]*)/g)) {
    cssClasses.add(match[1]);
}

// Class names the webview applies, from `class:` attributes and className maths.
const usedClasses = new Set();
for (const match of editorJs.matchAll(/class:\s*'([^']*)'/g)) {
    for (const name of match[1].split(/\s+/).filter(Boolean)) {
        usedClasses.add(name);
    }
}
for (const match of editorJs.matchAll(/class:\s*`([^`]*)`/g)) {
    // Mark interpolations so a token that is only half-literal (`s-${status}`)
    // is dropped rather than recorded as the meaningless prefix `s-`.
    const marked = match[1].replace(/\$\{[^}]*\}/g, '\u0001INTERP');
    for (const name of marked.split(/\s+/).filter(Boolean)) {
        if (!name.includes('\u0001INTERP')) {
            usedClasses.add(name);
        }
    }
}
for (const match of editorJs.matchAll(/classList\.(?:add|toggle)\('([^']+)'/g)) {
    usedClasses.add(match[1]);
}
// Status modifiers are built as `s-${entry.status}`.
for (const status of ['introduced', 'override', 'redundant', 'shadowed']) {
    usedClasses.add(`s-${status}`);
}

const undefinedClasses = [...usedClasses].filter((name) => !cssClasses.has(name));
if (undefinedClasses.length === 0) {
    ok(`all ${usedClasses.size} applied classes exist in the stylesheet`);
} else {
    fail('classes applied by the webview with no stylesheet rule', undefinedClasses.join(', '));
}

// ---------------------------------------------------------------------------
section('5. Commands declared vs registered');

const declared = new Set(manifest.contributes.commands.map((command) => command.command));
const commandsJs = fs.readFileSync(path.join(root, 'src', 'commands', 'index.js'), 'utf8');
const registered = new Set();
for (const match of commandsJs.matchAll(/'(skylakeTuning\.[A-Za-z.]+)'/g)) {
    registered.add(match[1]);
}

const notRegistered = [...declared].filter((command) => !registered.has(command));
if (notRegistered.length === 0) {
    ok(`all ${declared.size} declared commands are registered`);
} else {
    fail('commands declared in package.json but never registered', notRegistered.join('\n'));
}

const notDeclared = [...registered].filter((command) => !declared.has(command));
if (notDeclared.length === 0) {
    ok('no commands registered without being declared');
} else {
    fail('commands registered but missing from package.json', notDeclared.join('\n'));
}

// Menu entries must reference real commands.
const menuCommands = new Set();
for (const entries of Object.values(manifest.contributes.menus)) {
    for (const entry of entries) {
        if (entry.command) {
            menuCommands.add(entry.command);
        }
    }
}
const danglingMenu = [...menuCommands].filter((command) => !declared.has(command));
if (danglingMenu.length === 0) {
    ok(`all ${menuCommands.size} menu entries reference declared commands`);
} else {
    fail('menu entries referencing undeclared commands', danglingMenu.join('\n'));
}

// ---------------------------------------------------------------------------
section('6. Webview assets and CSP');

for (const asset of ['media/editor.js', 'media/editor.css', 'media/icon.svg']) {
    if (fs.existsSync(path.join(root, asset))) {
        ok(`${asset} present`);
    } else {
        fail(`${asset} missing`);
    }
}

if (/Content-Security-Policy/.test(providerJs) && /nonce-\$\{nonce\}/.test(providerJs)) {
    ok('webview HTML sets a CSP with a script nonce');
} else {
    fail('webview HTML is missing a nonce-based CSP');
}
if (/randomBytes/.test(providerJs)) {
    ok('nonce comes from the CSPRNG');
} else {
    fail('nonce is not generated from a CSPRNG');
}

// ---------------------------------------------------------------------------
section('Summary');
console.log(`${checks - failures}/${checks} checks passed`);
if (failures > 0) {
    console.log(`${failures} FAILURE(S)`);
    process.exit(1);
}
console.log('all good');
