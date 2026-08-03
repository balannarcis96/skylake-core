/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2025 Balan Narcis (balannarcis96@gmail.com)
 *
 * Lightweight CMake reader.
 *
 * We are not trying to be CMake. We need three facts that cannot be recovered
 * from the JSON alone, and that silently change what the compiler sees:
 *
 *   1. The ORDER of `skl_add_presets_file()` calls. Priority ties are broken by
 *      registration order, and a target only ever sees files registered before
 *      its own `skl_add_tune_header_to_target()` call.
 *   2. Which files are registered at all. A preset file that nobody registers
 *      is dead weight that looks perfectly healthy in isolation.
 *   3. Each target's `DEFAULT_PRESET_FILE` / `PRESET_TARGET_NAME`, since the
 *      JSON `target_name` is a lookup key and need not match the CMake target.
 *
 * Conditionals are not evaluated. Calls inside `if()` blocks are still
 * reported, flagged `conditional` so the UI can say "depends on configuration"
 * instead of pretending to know.
 */
'use strict';

const path = require('path');

/**
 * @typedef {Object} CMakeCall
 * @property {string} name
 * @property {string[]} args
 * @property {string} file       Absolute path of the CMakeLists.txt.
 * @property {number} offset     Byte offset of the command name.
 * @property {boolean} conditional
 */

/**
 * @typedef {Object} PresetRegistration
 * @property {string} filePath   Absolute, normalized.
 * @property {string} rawArg
 * @property {string} declaredIn
 * @property {number} offset
 * @property {number} order      0-based registration index.
 * @property {boolean} conditional
 * @property {boolean} resolved  False when a variable could not be expanded.
 */

/**
 * @typedef {Object} TuneTarget
 * @property {string} cmakeTarget
 * @property {string} outputName
 * @property {string} presetTargetName
 * @property {string|undefined} defaultPresetFile
 * @property {string|undefined} defaultPresetName
 * @property {string|undefined} presetName
 * @property {string} declaredIn
 * @property {number} offset
 * @property {boolean} conditional
 * @property {number} order      Registration index at the point of the call.
 * @property {boolean} orderKnown False when the call sits in a function body,
 *           so `order` reflects the definition site rather than the call site.
 */

/**
 * @typedef {Object} CMakeScan
 * @property {PresetRegistration[]} registrations
 * @property {TuneTarget[]} targets
 * @property {string[]} visited
 */

const COMMAND_RE = /(^|[\s()])([A-Za-z_][A-Za-z0-9_]*)[ \t]*\(/g;

/**
 * Strip `#` comments that live outside quoted strings.
 * @param {string} text
 */
function stripComments(text) {
    let out = '';
    let inString = false;
    for (let i = 0; i < text.length; i++) {
        const ch = text[i];
        if (inString) {
            if (ch === '\\') {
                out += ch + (text[i + 1] ?? '');
                i++;
                continue;
            }
            if (ch === '"') {
                inString = false;
            }
            out += ch;
            continue;
        }
        if (ch === '"') {
            inString = true;
            out += ch;
            continue;
        }
        if (ch === '#') {
            // Preserve offsets so diagnostics still point at the right place.
            while (i < text.length && text[i] !== '\n') {
                out += ' ';
                i++;
            }
            out += '\n';
            continue;
        }
        out += ch;
    }
    return out;
}

/**
 * Split a command's argument text into individual arguments.
 * @param {string} text
 * @returns {string[]}
 */
function splitArgs(text) {
    /** @type {string[]} */
    const args = [];
    let current = '';
    let inString = false;
    let has = false;

    for (let i = 0; i < text.length; i++) {
        const ch = text[i];
        if (inString) {
            if (ch === '\\' && text[i + 1] !== undefined) {
                current += text[i + 1];
                i++;
                continue;
            }
            if (ch === '"') {
                inString = false;
                continue;
            }
            current += ch;
            continue;
        }
        if (ch === '"') {
            inString = true;
            has = true;
            continue;
        }
        if (/\s/.test(ch)) {
            if (has || current !== '') {
                args.push(current);
                current = '';
                has = false;
            }
            continue;
        }
        current += ch;
    }
    if (has || current !== '') {
        args.push(current);
    }
    return args;
}

/**
 * Parse every command invocation in a CMakeLists, in source order.
 * @param {string} text
 * @param {string} file
 * @returns {CMakeCall[]}
 */
function parseCommands(text, file) {
    const source = stripComments(text);
    /** @type {CMakeCall[]} */
    const calls = [];
    let depth = 0;
    let functionDepth = 0;

    COMMAND_RE.lastIndex = 0;
    let match;
    while ((match = COMMAND_RE.exec(source)) !== null) {
        const name = match[2].toLowerCase();
        const nameOffset = match.index + match[1].length;
        const open = COMMAND_RE.lastIndex - 1;

        // Read to the matching close paren, ignoring parens inside strings.
        let i = open + 1;
        let level = 1;
        let inString = false;
        while (i < source.length && level > 0) {
            const ch = source[i];
            if (inString) {
                if (ch === '\\') {
                    i += 2;
                    continue;
                }
                if (ch === '"') {
                    inString = false;
                }
            } else if (ch === '"') {
                inString = true;
            } else if (ch === '(') {
                level++;
            } else if (ch === ')') {
                level--;
            }
            i++;
        }
        if (level !== 0) {
            break;
        }

        const body = source.slice(open + 1, i - 1);

        if (name === 'if') {
            depth++;
        } else if (name === 'endif') {
            depth = Math.max(0, depth - 1);
        } else if (name === 'function' || name === 'macro') {
            functionDepth++;
        } else if (name === 'endfunction' || name === 'endmacro') {
            functionDepth = Math.max(0, functionDepth - 1);
        }

        calls.push({
            name,
            args: splitArgs(body),
            file,
            offset: nameOffset,
            // `if` itself is not inside a conditional; its body is.
            conditional: name === 'if' ? depth > 1 : depth > 0,
            // A call in a function body runs whenever that function is INVOKED,
            // which may be in another directory and at a completely different
            // point in the registration sequence. We record the fact so callers
            // can decline to reason about ordering instead of guessing wrong.
            inFunction: name === 'function' || name === 'macro' ? functionDepth > 1 : functionDepth > 0
        });

        COMMAND_RE.lastIndex = i;
    }

    return calls;
}

/**
 * Expand `${VAR}` references against a best-effort environment.
 * @param {string} value
 * @param {Map<string, string>} env
 * @returns {{text: string, resolved: boolean}}
 */
function expand(value, env) {
    let resolved = true;
    let out = value;
    for (let pass = 0; pass < 8; pass++) {
        const next = out.replace(/\$\{([A-Za-z0-9_-]+)\}/g, (whole, name) => {
            const found = env.get(name);
            if (found === undefined) {
                resolved = false;
                return whole;
            }
            return found;
        });
        if (next === out) {
            break;
        }
        out = next;
    }
    return { text: out, resolved: resolved && !out.includes('${') };
}

/**
 * Walk a CMake project from its root, following `add_subdirectory()`.
 *
 * @param {string} rootDir                Absolute path of the folder holding the root CMakeLists.txt.
 * @param {(absPath: string) => string|undefined} readFile
 * @returns {CMakeScan}
 */
function scanProject(rootDir, readFile) {
    /** @type {PresetRegistration[]} */
    const registrations = [];
    /** @type {TuneTarget[]} */
    const targets = [];
    /** @type {string[]} */
    const visited = [];
    const seen = new Set();

    /** @type {Map<string, string>} */
    const globalEnv = new Map();
    globalEnv.set('CMAKE_SOURCE_DIR', rootDir);

    // User-defined commands, captured at their `function()`/`macro()` and
    // replayed at each invocation. Without this, a tune call written inside a
    // helper (skylake-core's skl_CreateSkylakeCoreLibTarget, m2-server's
    // make_game_target) is attributed to the definition site: the wrong
    // directory, the wrong registration index, and an unresolvable
    // `${TARGET_NAME}`.
    /** @type {Map<string, {params: string[], body: CMakeCall[]}>} */
    const functions = new Map();

    /**
     * @param {string} dir
     * @param {Map<string, string>} inherited
     */
    const walk = (dir, inherited) => {
        const listsPath = path.join(dir, 'CMakeLists.txt');
        if (seen.has(listsPath)) {
            return;
        }
        seen.add(listsPath);

        const text = readFile(listsPath);
        if (text === undefined) {
            return;
        }
        visited.push(listsPath);

        // Directory scope: child dirs inherit, but their writes do not escape.
        const env = new Map(inherited);
        env.set('CMAKE_CURRENT_SOURCE_DIR', dir);
        env.set('CMAKE_CURRENT_LIST_DIR', dir);

        runCalls(parseCommands(text, listsPath), env, listsPath, dir, 0);
    };

    /**
     * @param {CMakeCall[]} calls
     * @param {Map<string, string>} env
     * @param {string} listsPath
     * @param {string} dir
     * @param {number} depth   Guards against mutual recursion between helpers.
     */
    const runCalls = (calls, env, listsPath, dir, depth) => {
        for (let cursor = 0; cursor < calls.length; cursor++) {
            const call = calls[cursor];

            // Capture a definition wholesale and skip past its body.
            if (call.name === 'function' || call.name === 'macro') {
                const name = (call.args[0] ?? '').toLowerCase();
                const params = call.args.slice(1);
                const closer = call.name === 'function' ? 'endfunction' : 'endmacro';
                let level = 1;
                let end = cursor + 1;
                for (; end < calls.length; end++) {
                    if (calls[end].name === call.name) {
                        level++;
                    } else if (calls[end].name === closer) {
                        level--;
                        if (level === 0) {
                            break;
                        }
                    }
                }
                if (name) {
                    functions.set(name, { params, body: calls.slice(cursor + 1, end) });
                }
                cursor = end;
                continue;
            }

            const defined = functions.get(call.name);
            if (defined && depth < 8) {
                // Bind positional parameters, then replay the body here so
                // registrations land in call order.
                const callEnv = new Map(env);
                const args = call.args.map((arg) => expand(arg, env).text);
                defined.params.forEach((param, position) => {
                    callEnv.set(param, args[position] ?? '');
                });
                callEnv.set('ARGC', String(args.length));
                args.forEach((arg, position) => callEnv.set(`ARGV${position}`, arg));
                runCalls(defined.body, callEnv, listsPath, dir, depth + 1);
                continue;
            }

            switch (call.name) {
                case 'project': {
                    const name = expand(call.args[0] ?? '', env).text;
                    if (name) {
                        env.set('PROJECT_SOURCE_DIR', dir);
                        env.set('PROJECT_NAME', name);
                        env.set(`${name}_SOURCE_DIR`, dir);
                        // Project dir vars are visible to the whole tree below
                        // AND to siblings added later, so promote them.
                        globalEnv.set(`${name}_SOURCE_DIR`, dir);
                    }
                    break;
                }
                case 'set': {
                    const name = call.args[0];
                    if (name && call.args.length > 1) {
                        const value = expand(call.args[1], env).text;
                        env.set(name, value);
                        // Only directory-scope assignments are promoted. A
                        // `set()` inside a function body is local to the call,
                        // and leaking it would corrupt expansions elsewhere.
                        if (depth === 0) {
                            globalEnv.set(name, value);
                        }
                    }
                    break;
                }
                case 'skl_add_presets_file': {
                    const raw = call.args[0] ?? '';
                    const { text: expanded, resolved } = expand(raw, env);
                    const filePath = resolved ? path.normalize(expanded) : expanded;

                    // The same file can legitimately appear in two mutually
                    // exclusive branches (skylake-core registers its presets in
                    // both arms of `if(PROJECT_IS_TOP_LEVEL)`). We do not
                    // evaluate conditions, so we would otherwise record it
                    // twice and desynchronize every later registration index.
                    // Keeping the first occurrence is also semantically free:
                    // a repeated file contributes at an equal priority, and
                    // ties keep the incumbent, so the second pass is a no-op.
                    const duplicate = registrations.find(
                        (entry) => entry.filePath === filePath && entry.resolved === resolved
                    );
                    if (duplicate) {
                        duplicate.conditional = duplicate.conditional && call.conditional;
                        break;
                    }

                    registrations.push({
                        filePath,
                        rawArg: raw,
                        declaredIn: listsPath,
                        offset: call.offset,
                        order: registrations.length,
                        conditional: call.conditional,
                        resolved
                    });
                    break;
                }
                case 'skl_add_tune_header_to_target': {
                    const args = call.args.map((arg) => expand(arg, env).text);
                    const cmakeTarget = args[0] ?? '';
                    const outputName = args[1] ?? '';
                    /** @type {Record<string, string|undefined>} */
                    const options = {};
                    for (let i = 2; i < args.length - 1; i++) {
                        const key = args[i];
                        if (
                            key === 'PRESET_NAME' ||
                            key === 'PRESET_TARGET_NAME' ||
                            key === 'DEFAULT_PRESET_FILE' ||
                            key === 'DEFAULT_PRESET_NAME'
                        ) {
                            options[key] = args[i + 1];
                        }
                    }
                    targets.push({
                        cmakeTarget,
                        outputName,
                        presetTargetName: options.PRESET_TARGET_NAME || cmakeTarget,
                        defaultPresetFile: options.DEFAULT_PRESET_FILE
                            ? path.normalize(options.DEFAULT_PRESET_FILE)
                            : undefined,
                        defaultPresetName: options.DEFAULT_PRESET_NAME,
                        presetName: options.PRESET_NAME || undefined,
                        declaredIn: listsPath,
                        offset: call.offset,
                        conditional: call.conditional,
                        // Function bodies are replayed at their invocation, so
                        // this counts the files registered by the time the
                        // generator actually runs for this target.
                        order: registrations.length,
                        orderKnown: true
                    });
                    break;
                }
                case 'add_subdirectory': {
                    const raw = call.args[0] ?? '';
                    const { text: expanded, resolved } = expand(raw, env);
                    if (!resolved || expanded === '') {
                        break;
                    }
                    const childDir = path.isAbsolute(expanded)
                        ? expanded
                        : path.join(dir, expanded);
                    walk(childDir, env);
                    break;
                }
                default:
                    break;
            }
        }
    };

    walk(rootDir, globalEnv);
    return { registrations, targets, visited };
}

module.exports = { scanProject, parseCommands, splitArgs, stripComments, expand };
