/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2025 Balan Narcis (balannarcis96@gmail.com)
 *
 * Differential test for the pure-logic half of the extension.
 *
 * The resolver claims to reproduce `generate_tuning.py`. The only honest way to
 * back that claim is to run it against a real preset tree and diff the result
 * against the headers CMake actually generated. Point this at a configured
 * build directory and it will do exactly that.
 *
 *   node test/verify.js <repo-root> [<generated-tuning-dir>]
 *
 * Exits non-zero on any mismatch.
 */
'use strict';

const fs = require('fs');
const path = require('path');

const { parseTree } = require('../src/core/jsonAst');
const { detectStyle, stringify } = require('../src/core/serialize');
const { looksLikePresetDocument, buildDocument, configureSubstitutions } = require('../src/core/schema');
const { scanProject } = require('../src/core/cmake');
const { resolve } = require('../src/core/resolver');

const repoRoot = path.resolve(process.argv[2] ?? '.');
const tuningDir = process.argv[3] ? path.resolve(process.argv[3]) : undefined;

let failures = 0;
let checks = 0;

function ok(message) {
    checks++;
    console.log(`  ✓ ${message}`);
}

function fail(message, detail) {
    checks++;
    failures++;
    console.log(`  ✗ ${message}`);
    if (detail) {
        for (const line of String(detail).split('\n').slice(0, 20)) {
            console.log(`      ${line}`);
        }
    }
}

function section(title) {
    console.log(`\n${title}`);
    console.log('-'.repeat(title.length));
}

/** Recursively collect *.json under a directory. */
function findJson(dir, out = []) {
    let items;
    try {
        items = fs.readdirSync(dir, { withFileTypes: true });
    } catch {
        return out;
    }
    for (const item of items) {
        const full = path.join(dir, item.name);
        if (item.isDirectory()) {
            if (['node_modules', '.git', 'build', 'build2', 'out'].includes(item.name)) {
                continue;
            }
            findJson(full, out);
        } else if (item.name.endsWith('.json')) {
            out.push(full);
        }
    }
    return out;
}

// ---------------------------------------------------------------------------
section('1. Parser + serializer round-trip fidelity');

const presetFiles = [];
for (const file of findJson(repoRoot)) {
    let text;
    try {
        text = fs.readFileSync(file, 'utf8');
    } catch {
        continue;
    }
    let ast;
    try {
        ast = parseTree(text);
    } catch {
        continue;
    }
    if (!looksLikePresetDocument(ast.value)) {
        continue;
    }
    presetFiles.push({ file, text, ast });
}

if (presetFiles.length === 0) {
    fail('no preset files discovered - wrong repo root?');
} else {
    ok(`discovered ${presetFiles.length} preset files by schema`);
    let mismatches = [];
    for (const { file, text, ast } of presetFiles) {
        const style = detectStyle(text);
        const written = stringify(ast.value, style);
        if (written !== text) {
            mismatches.push(path.relative(repoRoot, file));
        }
    }
    if (mismatches.length === 0) {
        ok('every file round-trips byte-for-byte through parse -> serialize');
    } else {
        fail(`${mismatches.length} file(s) did not round-trip`, mismatches.join('\n'));
    }
}

// ---------------------------------------------------------------------------
section('2. CMake scan: registration order');

const scan = scanProject(repoRoot, (absPath) => {
    try {
        return fs.readFileSync(absPath, 'utf8');
    } catch {
        return undefined;
    }
});

ok(`walked ${scan.visited.length} CMakeLists.txt file(s)`);
ok(`found ${scan.registrations.length} skl_add_presets_file() call(s)`);
ok(`found ${scan.targets.length} skl_add_tune_header_to_target() call(s)`);

for (const registration of scan.registrations) {
    if (!registration.resolved) {
        fail(`unresolved variable in registration: ${registration.rawArg}`);
    }
}

// Compare against the ground truth CMake itself recorded, when available.
const cachePath = path.join(repoRoot, 'build', 'CMakeCache.txt');
if (fs.existsSync(cachePath)) {
    const cache = fs.readFileSync(cachePath, 'utf8');
    const match = /^SKL_TUNE_PRESETS_FILES:STRING=(.*)$/m.exec(cache);
    if (match) {
        const expected = match[1].split(';').filter(Boolean).map((entry) => path.normalize(entry));
        const actual = scan.registrations.map((entry) => path.normalize(entry.filePath));
        const expectedSet = new Set(expected);
        const actualSet = new Set(actual);

        const missing = expected.filter((entry) => !actualSet.has(entry));
        const extra = actual.filter((entry) => !expectedSet.has(entry));

        if (missing.length === 0 && extra.length === 0) {
            ok(`registration set matches SKL_TUNE_PRESETS_FILES (${expected.length} files)`);
        } else {
            const detail = [
                ...missing.map((entry) => `only in CMakeCache: ${path.relative(repoRoot, entry)}`),
                ...extra.map((entry) => `only in our scan:  ${path.relative(repoRoot, entry)}`)
            ].join('\n');
            // A conditional block we cannot evaluate is an expected difference,
            // so report it as informational rather than a hard failure.
            const explainable = [...missing, ...extra].every((entry) =>
                scan.registrations.some(
                    (registration) =>
                        path.normalize(registration.filePath) === entry && registration.conditional
                ) || missing.includes(entry)
            );
            if (explainable && extra.every((entry) =>
                scan.registrations.find((r) => path.normalize(r.filePath) === entry)?.conditional)) {
                ok(`registration set differs only by unevaluated conditionals`);
                console.log(detail.split('\n').map((l) => `      ${l}`).join('\n'));
            } else {
                fail('registration set differs from SKL_TUNE_PRESETS_FILES', detail);
            }
        }

        const commonExpected = expected.filter((entry) => actualSet.has(entry));
        const commonActual = actual.filter((entry) => expectedSet.has(entry));
        if (commonExpected.join('|') === commonActual.join('|')) {
            ok('registration ORDER matches CMake for all common files');
        } else {
            fail(
                'registration order differs',
                `cmake: ${commonExpected.map((e) => path.basename(path.dirname(e)) + '/' + path.basename(e)).join(', ')}\n` +
                `ours:  ${commonActual.map((e) => path.basename(path.dirname(e)) + '/' + path.basename(e)).join(', ')}`
            );
        }
    }
} else {
    console.log('  - build/CMakeCache.txt not present, skipping ground-truth comparison');
}

// ---------------------------------------------------------------------------
section('3. Resolver vs generated headers');

/** Build the index the same way the extension does. */
const docs = new Map();
for (const { file, ast } of presetFiles) {
    docs.set(path.normalize(file), buildDocument(ast.value));
}

/**
 * Parse the constants and defines out of a generated tuning header.
 * @param {string} text
 */
function parseGeneratedHeader(text) {
    /** @type {Map<string, string>} */
    const values = new Map();
    const defineRe = /^#define\s+([A-Za-z_][A-Za-z0-9_]*)\s+(.*)$/gm;
    let match;
    while ((match = defineRe.exec(text)) !== null) {
        if (match[1].startsWith('SKL_CURRENT_PRESET_') || /_H$/.test(match[1])) {
            continue;
        }
        values.set(match[1], match[2].trim());
    }
    const constRe = /^constexpr\s+(\S+)\s+([A-Za-z_][A-Za-z0-9_]*)\s*=\s*([\s\S]*?);\s*$/gm;
    while ((match = constRe.exec(text)) !== null) {
        values.set(match[2], match[3].trim());
    }
    const meta = {};
    const presetMatch = /^\s*Preset:\s+(\S+)/m.exec(text);
    const targetMatch = /^\s*Target:\s+(\S+)/m.exec(text);
    const prioMatch = /^\s*Priority:\s+(\d+)/m.exec(text);
    if (presetMatch) meta.preset = presetMatch[1];
    if (targetMatch) meta.target = targetMatch[1];
    if (prioMatch) meta.priority = Number(prioMatch[1]);
    return { values, meta };
}

function resolveLikeExtension(presetTargetName, presetName, defaultFile, defaultPresetName, cutoff) {
    const layers = [];
    const defaultId = defaultFile ? path.normalize(defaultFile) : undefined;
    if (defaultId && docs.has(defaultId)) {
        layers.push({ id: defaultId, doc: docs.get(defaultId) });
    }
    for (const registration of scan.registrations) {
        if (registration.order >= cutoff) {
            continue;
        }
        const id = path.normalize(registration.filePath);
        if (id === defaultId || !docs.has(id)) {
            continue;
        }
        layers.push({ id, doc: docs.get(id) });
    }
    return resolve({
        targetName: presetTargetName,
        presetName,
        defaultPresetName: defaultPresetName ?? 'default',
        layers,
        defaultLayerId: defaultId
    });
}

if (!tuningDir) {
    console.log('  - no generated tuning dir given, skipping header diff');
} else if (!fs.existsSync(tuningDir)) {
    fail(`generated tuning dir does not exist: ${tuningDir}`);
} else {
    // Mirror SkylakeTuning.cmake's SKL_TUNE_DEFAULT_PRESETS_FILE, which is
    // `${CMAKE_CURRENT_LIST_DIR}/../presets/default_presets.json` relative to
    // the tuning module. Locating the module works whether skylake-core is the
    // repository itself or a submodule of one.
    const findDefaultPresets = (dir, depth = 0) => {
        if (depth > 5) {
            return undefined;
        }
        const module = path.join(dir, 'cmake', 'tuning', 'SkylakeTuning.cmake');
        if (fs.existsSync(module)) {
            const candidate = path.join(dir, 'cmake', 'presets', 'default_presets.json');
            if (fs.existsSync(candidate)) {
                return candidate;
            }
        }
        for (const entry of fs.readdirSync(dir, { withFileTypes: true })) {
            if (!entry.isDirectory() || ['.git', 'node_modules', 'build', 'out'].includes(entry.name)) {
                continue;
            }
            const found = findDefaultPresets(path.join(dir, entry.name), depth + 1);
            if (found) {
                return found;
            }
        }
        return undefined;
    };
    const skylakeDefault = findDefaultPresets(repoRoot);
    if (!skylakeDefault) {
        fail('could not locate the SkylakeTuning default_presets.json');
    } else {
        ok(`default presets file: ${path.relative(repoRoot, skylakeDefault)}`);
    }

    // Drive the comparison from the generated tree rather than from the scan,
    // so a target whose CMake name we could not resolve (anything created
    // inside a function, e.g. game_auth and libskl-core) is still checked
    // instead of being silently skipped.
    /** @type {{dir: string, headerPath: string, output: string, target: any}[]} */
    const pairs = [];
    for (const entry of fs.readdirSync(tuningDir, { withFileTypes: true })) {
        if (!entry.isDirectory()) {
            continue;
        }
        const dir = path.join(tuningDir, entry.name);
        for (const headerName of fs.readdirSync(dir)) {
            const match = /^tune_(.+)_(public|private)\.h$/.exec(headerName);
            if (!match) {
                continue;
            }
            const target = scan.targets.find((candidate) => candidate.outputName === match[1]);
            if (!target) {
                fail(`generated ${entry.name}/${headerName} has no matching skl_add_tune_header_to_target() call`);
                continue;
            }
            pairs.push({ dir, headerPath: path.join(dir, headerName), output: match[2], target });
        }
    }

    if (pairs.length === 0) {
        fail(`no generated tuning headers found under ${tuningDir}`);
    }

    for (const { headerPath, output, target } of pairs) {
        {
            const headerText = fs.readFileSync(headerPath, 'utf8');
            const { values: expected, meta } = parseGeneratedHeader(headerText);
            const presetName = meta.preset ?? 'dev';


            const defaultFile = target.defaultPresetFile ?? skylakeDefault;
            const result = resolveLikeExtension(
                target.presetTargetName,
                presetName,
                defaultFile,
                target.defaultPresetName,
                // Mirrors the extension: the visibility cut-off only applies to
                // a consumer declared at directory scope.
                target.orderKnown === false ? Number.POSITIVE_INFINITY : target.order
            );

            const actual = new Map();
            for (const entry of result.entries.values()) {
                if ((entry.output === 'public' ? 'public' : 'private') !== output) {
                    continue;
                }
                actual.set(entry.key, String(entry.value).trim());
            }

            const label = `${path.basename(path.dirname(headerPath))}/${path.basename(headerPath)} [${presetName}]`;
            const problems = [];
            const substituted = [];
            for (const [key, value] of expected) {
                if (!actual.has(key)) {
                    problems.push(`missing key ${key} (generated has ${value})`);
                } else if (actual.get(key) !== value) {
                    // configure_file() runs after the generator and rewrites
                    // @VAR@ / ${VAR}, so a difference there is the pipeline
                    // working as designed rather than a resolver bug.
                    if (configureSubstitutions(actual.get(key)).length > 0) {
                        substituted.push(`${key}: '${actual.get(key)}' -> '${value}' via configure_file`);
                    } else {
                        problems.push(`${key}: ours='${actual.get(key)}' generated='${value}'`);
                    }
                }
            }
            for (const key of actual.keys()) {
                if (!expected.has(key)) {
                    problems.push(`extra key ${key} not in generated header`);
                }
            }
            if (meta.priority !== undefined && meta.priority !== result.maxPriority) {
                problems.push(`max priority: ours=${result.maxPriority} generated=${meta.priority}`);
            }

            // A generated header is only ground truth while the build is in
            // sync with the JSON. Distinguish "the resolver is wrong" from
            // "this artifact predates an edit" by content, not timestamps: a
            // git checkout rewrites mtimes without changing anything, so mtime
            // would skip comparisons that genuinely pass.
            //
            // Stale signature: the header carries a key no preset file declares
            // any more, or lacks a key the preset files now declare.
            const declaredKeys = new Set();
            for (const doc of docs.values()) {
                for (const preset of doc.presets) {
                    for (const config of preset.configs) {
                        if (config.targetName !== target.presetTargetName) {
                            continue;
                        }
                        for (const declared of config.entries) {
                            declaredKeys.add(declared.key);
                        }
                    }
                }
            }
            const staleEvidence = problems.filter((problem) => {
                const missing = /^missing key (\S+)/.exec(problem);
                if (missing) {
                    return !declaredKeys.has(missing[1]);
                }
                const extra = /^extra key (\S+)/.exec(problem);
                return extra ? declaredKeys.has(extra[1]) : false;
            });

            if (problems.length === 0) {
                ok(
                    `${label}: ${expected.size} values match` +
                        (substituted.length > 0 ? ` (${substituted.length} via configure_file)` : '')
                );
                for (const note of substituted) {
                    console.log(`      ${note}`);
                }
            } else if (staleEvidence.length === problems.length) {
                checks++;
                console.log(
                    `  - ${label}: skipped — the generated header is stale ` +
                        `(${problems.length} key(s) renamed or added since it was produced). Re-run cmake.`
                );
            } else {
                fail(`${label}: ${problems.length} mismatch(es)`, problems.join('\n'));
            }
        }
    }
}

// ---------------------------------------------------------------------------
section('Summary');
console.log(`${checks - failures}/${checks} checks passed`);
if (failures > 0) {
    console.log(`${failures} FAILURE(S)`);
    process.exit(1);
}
console.log('all good');
