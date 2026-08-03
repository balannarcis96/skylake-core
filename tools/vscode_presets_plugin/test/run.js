/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2025 Balan Narcis (balannarcis96@gmail.com)
 *
 * Test driver.
 *
 *   node test/run.js [<repo-root> ...]
 *
 * With no arguments it discovers candidate repositories by walking up from this
 * folder, so `npm test` works from a fresh clone. Each repository gets the
 * syntax check, the differential verification against any generated tuning
 * headers, and the integration pass.
 */
'use strict';

const fs = require('fs');
const path = require('path');
const { execFileSync } = require('child_process');

const here = __dirname;
const extensionRoot = path.dirname(here);

/** @returns {string[]} */
function discoverRepos() {
    const roots = [];

    // The repository this extension lives in.
    let cursor = extensionRoot;
    for (let i = 0; i < 6; i++) {
        cursor = path.dirname(cursor);
        if (fs.existsSync(path.join(cursor, 'CMakeLists.txt'))) {
            roots.push(cursor);
            break;
        }
    }

    // Sibling checkouts that also use SkylakeTuning, m2-server above all.
    const parent = path.dirname(roots[0] ?? extensionRoot);
    for (const candidate of ['m2-server/source', 'm2-server']) {
        const full = path.join(parent, candidate);
        if (
            fs.existsSync(path.join(full, 'CMakeLists.txt')) &&
            !roots.includes(full)
        ) {
            roots.push(full);
        }
    }

    return roots;
}

/** @param {string} repoRoot */
function tuningDirFor(repoRoot) {
    for (const candidate of [
        path.join(repoRoot, 'build', 'Extern', 'tuning'),
        path.join(repoRoot, 'build', 'tuning')
    ]) {
        if (fs.existsSync(candidate)) {
            return candidate;
        }
    }
    return undefined;
}

const repos = process.argv.slice(2).map((entry) => path.resolve(entry));
const targets = repos.length > 0 ? repos : discoverRepos();

let failed = 0;

function run(label, args) {
    console.log(`\n${'='.repeat(72)}\n${label}\n${'='.repeat(72)}`);
    try {
        const out = execFileSync(process.execPath, args, {
            cwd: extensionRoot,
            encoding: 'utf8',
            stdio: 'pipe'
        });
        process.stdout.write(out);
    } catch (error) {
        if (error.stdout) {
            process.stdout.write(error.stdout);
        }
        if (error.stderr) {
            process.stderr.write(error.stderr);
        }
        failed++;
    }
}

// 1. Syntax check every source file.
console.log(`${'='.repeat(72)}\nSyntax check\n${'='.repeat(72)}`);
const sources = [];
const collect = (dir) => {
    for (const item of fs.readdirSync(dir, { withFileTypes: true })) {
        const full = path.join(dir, item.name);
        if (item.isDirectory()) {
            collect(full);
        } else if (item.name.endsWith('.js')) {
            sources.push(full);
        }
    }
};
for (const dir of ['src', 'media', 'test']) {
    const full = path.join(extensionRoot, dir);
    if (fs.existsSync(full)) {
        collect(full);
    }
}
for (const source of sources) {
    try {
        execFileSync(process.execPath, ['--check', source], { stdio: 'pipe' });
        console.log(`  ✓ ${path.relative(extensionRoot, source)}`);
    } catch (error) {
        console.log(`  ✗ ${path.relative(extensionRoot, source)}`);
        process.stderr.write(error.stderr ? error.stderr.toString() : '');
        failed++;
    }
}

try {
    JSON.parse(fs.readFileSync(path.join(extensionRoot, 'package.json'), 'utf8'));
    console.log('  ✓ package.json');
} catch (error) {
    console.log(`  ✗ package.json — ${error.message}`);
    failed++;
}

// 2. Webview/host contract checks. Repository independent, so run once.
run('ui contract', [path.join(here, 'ui-contract.js')]);

if (targets.length === 0) {
    console.log('\nNo CMake repositories found to test against.');
}

for (const repoRoot of targets) {
    const tuningDir = tuningDirFor(repoRoot);
    run(
        `verify — ${repoRoot}${tuningDir ? '' : '  (no generated headers, header diff skipped)'}`,
        [path.join(here, 'verify.js'), repoRoot, ...(tuningDir ? [tuningDir] : [])]
    );
    run(`integration — ${repoRoot}`, [path.join(here, 'integration.js'), repoRoot]);
    run(`render — ${repoRoot}`, [path.join(here, 'render.js'), repoRoot]);
}

console.log(`\n${'='.repeat(72)}`);
if (failed > 0) {
    console.log(`${failed} SUITE(S) FAILED`);
    process.exit(1);
}
console.log('ALL SUITES PASSED');
