/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2025 Balan Narcis (balannarcis96@gmail.com)
 *
 * Command palette surface.
 *
 * Report-style output is rendered as a read-only virtual markdown document
 * rather than an untitled buffer, so nothing ever prompts the user to save a
 * generated report.
 */
'use strict';

const path = require('path');
const vscode = require('vscode');

const { VIEW_TYPE } = require('../providers/presetEditor');

const REPORT_SCHEME = 'skylake-tuning';

/**
 * @param {vscode.ExtensionContext} context
 * @param {import('../core/workspaceIndex').WorkspaceIndex} index
 * @param {import('../providers/presetEditor').PresetEditorProvider} editor
 */
function registerCommands(context, index, editor) {
    /** @type {Map<string, string>} */
    const reports = new Map();

    const contentProvider = {
        provideTextDocumentContent(uri) {
            return reports.get(uri.toString()) ?? '# Not found\n\nThe report expired. Run the command again.';
        }
    };
    context.subscriptions.push(
        vscode.workspace.registerTextDocumentContentProvider(REPORT_SCHEME, contentProvider)
    );

    /**
     * @param {string} title
     * @param {string} markdown
     */
    const showReport = async (title, markdown) => {
        const uri = vscode.Uri.parse(
            `${REPORT_SCHEME}:${encodeURIComponent(title)}.md?${Date.now().toString(36)}`
        );
        reports.set(uri.toString(), markdown);
        const document = await vscode.workspace.openTextDocument(uri);
        await vscode.languages.setTextDocumentLanguage(document, 'markdown');
        await vscode.commands.executeCommand('markdown.showPreview', uri);
    };

    /** @type {[string, (...args: any[]) => any][]} */
    const commands = [
        [
            'skylakeTuning.openEditor',
            async (arg) => {
                const uri = toUri(arg);
                if (!uri) {
                    vscode.window.showInformationMessage('Open a JSON file first.');
                    return;
                }
                await vscode.commands.executeCommand('vscode.openWith', uri, VIEW_TYPE);
            }
        ],
        [
            'skylakeTuning.reopenAsText',
            async (arg) => {
                const uri = toUri(arg);
                if (uri) {
                    await vscode.commands.executeCommand('workbench.action.reopenTextEditor', uri);
                }
            }
        ],
        [
            'skylakeTuning.refreshIndex',
            async () => {
                await index.refresh();
                vscode.window.setStatusBarMessage(
                    `Skylake Tuning: indexed ${index.files.size} preset file(s), ${index.targets.size} target(s)`,
                    4000
                );
            }
        ],
        ['skylakeTuning.searchKeys', () => searchKeys(index, showReport)],
        ['skylakeTuning.resolveKey', (arg) => resolveKeyCommand(index, showReport, arg)],
        ['skylakeTuning.comparePresets', () => comparePresets(index, showReport)],
        ['skylakeTuning.showReport', () => workspaceReport(index, showReport)],
        [
            'skylakeTuning.validateAll',
            async () => {
                await index.refresh();
                await vscode.commands.executeCommand('workbench.actions.view.problems');
            }
        ],
        [
            'skylakeTuning.revealNode',
            async (node) => {
                if (!node) {
                    return;
                }
                if (node.kind === 'file') {
                    await vscode.commands.executeCommand(
                        'vscode.openWith',
                        vscode.Uri.file(node.data.id),
                        VIEW_TYPE
                    );
                    return;
                }
                if (node.kind === 'key' && node.data?.fileId) {
                    await editor.openKeyIn(node.data.fileId, node.data.key);
                }
            }
        ],

        // Webview context-menu targets.
        ['skylakeTuning.ctx.editValue', (arg) => editor.ctxEditValue(arg)],
        ['skylakeTuning.ctx.editMeta', (arg) => editor.ctxEditMeta(arg)],
        ['skylakeTuning.ctx.toggleOutput', (arg) => editor.ctxToggleOutput(arg)],
        ['skylakeTuning.ctx.overrideIn', (arg) => editor.ctxOverrideIn(arg)],
        ['skylakeTuning.ctx.moveToGroup', (arg) => editor.ctxMoveToGroup(arg)],
        ['skylakeTuning.ctx.renameKey', (arg) => editor.ctxRenameKey(arg)],
        ['skylakeTuning.ctx.duplicateKey', (arg) => editor.ctxDuplicateKey(arg)],
        ['skylakeTuning.ctx.deleteKey', (arg) => editor.ctxDeleteKey(arg)],
        ['skylakeTuning.ctx.gotoDefault', (arg) => editor.ctxGotoDefault(arg)],
        [
            'skylakeTuning.ctx.resolveKey',
            (arg) => {
                const context = editor.resolveContext(arg);
                if (!context) {
                    return undefined;
                }
                const model = editor.modelFor(context.document);
                const config = model?.presets?.[context.presetIndex]?.configs?.[context.configIndex];
                return resolveKeyCommand(index, showReport, {
                    target: config?.targetName,
                    key: context.key
                });
            }
        ],
        ['skylakeTuning.ctx.copyKey', (arg) => editor.ctxCopyKey(arg)],
        ['skylakeTuning.ctx.copyValue', (arg) => editor.ctxCopyValue(arg)],
        ['skylakeTuning.ctx.revealInText', (arg) => editor.ctxRevealInText(arg)]
    ];

    for (const [id, handler] of commands) {
        context.subscriptions.push(vscode.commands.registerCommand(id, handler));
    }
}

/** @param {*} arg */
function toUri(arg) {
    if (arg instanceof vscode.Uri) {
        return arg;
    }
    if (arg && arg.resourceUri instanceof vscode.Uri) {
        return arg.resourceUri;
    }
    return vscode.window.activeTextEditor?.document.uri;
}

/**
 * Fuzzy search over every key in the workspace.
 * @param {import('../core/workspaceIndex').WorkspaceIndex} index
 * @param {(title: string, markdown: string) => Promise<void>} showReport
 */
async function searchKeys(index, showReport) {
    /** @type {(vscode.QuickPickItem & {_target?: string, _key?: string})[]} */
    const items = [];

    for (const targetName of [...index.targets.keys()].sort()) {
        const info = index.targets.get(targetName);
        if (!info || info.fileIds.length === 0) {
            continue;
        }
        const presetNames = info.presetNames.length > 0 ? info.presetNames : [index.defaultPresetName];
        /** @type {Map<string, Map<string, string>>} */
        const byKey = new Map();

        for (const presetName of presetNames) {
            const result = index.resolve(targetName, presetName);
            for (const entry of result.entries.values()) {
                const perPreset = byKey.get(entry.key) ?? new Map();
                perPreset.set(presetName, entry.value);
                byKey.set(entry.key, perPreset);
            }
        }

        for (const [key, perPreset] of byKey) {
            const distinct = new Set(perPreset.values());
            const summary = [...perPreset.entries()]
                .map(([preset, value]) => `${preset}=${value}`)
                .join('  ');
            items.push({
                label: key,
                description: targetName,
                detail: distinct.size > 1 ? `⚠ differs per preset — ${summary}` : summary,
                _target: targetName,
                _key: key
            });
        }
    }

    if (items.length === 0) {
        vscode.window.showInformationMessage('No tuning keys indexed yet.');
        return;
    }

    items.sort((a, b) => a.label.localeCompare(b.label) || a.description.localeCompare(b.description));

    const picked = await vscode.window.showQuickPick(items, {
        title: `Search ${items.length} tuning keys`,
        placeHolder: 'Key name, target, or value',
        matchOnDescription: true,
        matchOnDetail: true
    });
    if (picked?._target && picked._key) {
        await resolveKeyCommand(index, showReport, { target: picked._target, key: picked._key });
    }
}

/**
 * @param {import('../core/workspaceIndex').WorkspaceIndex} index
 * @param {(title: string, markdown: string) => Promise<void>} showReport
 * @param {{target?: string, key?: string}} [preset]
 */
async function resolveKeyCommand(index, showReport, preset) {
    let targetName = preset?.target;
    if (!targetName) {
        const picked = await vscode.window.showQuickPick(
            [...index.targets.keys()].sort().map((name) => ({ label: name })),
            { title: 'Resolve key — pick a target' }
        );
        if (!picked) {
            return;
        }
        targetName = picked.label;
    }

    let key = preset?.key;
    if (!key) {
        const keys = index.keysFor(targetName);
        if (keys.length === 0) {
            vscode.window.showInformationMessage(`No keys declared for target '${targetName}'.`);
            return;
        }
        const picked = await vscode.window.showQuickPick(
            keys.map((name) => ({ label: name })),
            { title: `Resolve key in '${targetName}'` }
        );
        if (!picked) {
            return;
        }
        key = picked.label;
    }

    const info = index.targets.get(targetName);
    const presetNames = info && info.presetNames.length > 0
        ? [...info.presetNames].sort()
        : [index.defaultPresetName];

    const lines = [`# \`${key}\``, '', `Target **${targetName}**`, ''];

    lines.push('| Preset | Effective value | Winning file | Priority |');
    lines.push('| --- | --- | --- | --- |');
    for (const presetName of presetNames) {
        const result = index.resolve(targetName, presetName);
        const entry = result.entries.get(key);
        lines.push(
            entry
                ? `| \`${presetName}\` | \`${entry.value}\` | \`${label(entry.fileId)}\` | ${entry.priority} |`
                : `| \`${presetName}\` | _not defined_ | — | — |`
        );
    }

    for (const presetName of presetNames) {
        const result = index.resolve(targetName, presetName);
        const entry = result.entries.get(key);
        if (!entry || entry.trace.length <= 1) {
            continue;
        }
        lines.push('', `## Layers under \`${presetName}\``, '');
        lines.push('| # | File | Priority | Value | Outcome |');
        lines.push('| --- | --- | --- | --- | --- |');
        entry.trace.forEach((contribution, position) => {
            lines.push(
                `| ${position + 1} | \`${label(contribution.fileId)}\`${contribution.isDefaultLayer ? ' _(default layer)_' : ''} ` +
                    `| ${contribution.priority} | \`${contribution.value}\` | ` +
                    `${contribution.applied ? '**applied**' : `ignored — ${contribution.reason ?? 'lost'}`} |`
            );
        });
    }

    const first = index.resolve(targetName, presetNames[0]).entries.get(key);
    if (first) {
        lines.push(
            '',
            '## Emitted as',
            '',
            '```cpp',
            first.kind === 'define'
                ? `#define ${first.key} ${first.value}`
                : `${first.namespace ? `namespace ${first.namespace} { ` : ''}constexpr ${first.type ?? 'u32'} ${first.key} = ${first.value};${first.namespace ? ' }' : ''}`,
            '```',
            '',
            `Header: \`tune_*_${first.output}.h\``
        );
    }

    await showReport(`resolve-${key}`, lines.join('\n'));
}

/**
 * @param {import('../core/workspaceIndex').WorkspaceIndex} index
 * @param {(title: string, markdown: string) => Promise<void>} showReport
 */
async function comparePresets(index, showReport) {
    const targetPick = await vscode.window.showQuickPick(
        [...index.targets.keys()].sort().map((name) => ({ label: name })),
        { title: 'Compare presets — pick a target' }
    );
    if (!targetPick) {
        return;
    }
    const targetName = targetPick.label;
    const info = index.targets.get(targetName);
    const names = [...new Set([index.defaultPresetName, ...(info?.presetNames ?? [])])].sort();

    const left = await vscode.window.showQuickPick(
        names.map((name) => ({ label: name })),
        { title: 'Baseline preset' }
    );
    if (!left) {
        return;
    }
    const right = await vscode.window.showQuickPick(
        names.filter((name) => name !== left.label).map((name) => ({ label: name })),
        { title: `Compare against '${left.label}'` }
    );
    if (!right) {
        return;
    }

    const a = index.resolve(targetName, left.label);
    const b = index.resolve(targetName, right.label);
    const keys = [...new Set([...a.entries.keys(), ...b.entries.keys()])].sort();

    const lines = [
        `# \`${targetName}\`: \`${left.label}\` vs \`${right.label}\``,
        '',
        `| Key | \`${left.label}\` | \`${right.label}\` | |`,
        '| --- | --- | --- | --- |'
    ];

    let differences = 0;
    for (const key of keys) {
        const leftEntry = a.entries.get(key);
        const rightEntry = b.entries.get(key);
        const leftValue = leftEntry ? String(leftEntry.value) : undefined;
        const rightValue = rightEntry ? String(rightEntry.value) : undefined;
        if (leftValue === rightValue) {
            continue;
        }
        differences++;
        const marker = leftValue === undefined ? 'only in right' : rightValue === undefined ? 'only in left' : '≠';
        lines.push(
            `| \`${key}\` | ${leftValue === undefined ? '—' : `\`${leftValue}\``} | ` +
                `${rightValue === undefined ? '—' : `\`${rightValue}\``} | ${marker} |`
        );
    }

    if (differences === 0) {
        lines.push('| _identical_ | | | |');
    }
    lines.push('', `${differences} of ${keys.length} keys differ.`);

    await showReport(`compare-${targetName}`, lines.join('\n'));
}

/**
 * The workspace-wide health view: registration order, dead files, orphan
 * targets and priority inversions in one place.
 *
 * @param {import('../core/workspaceIndex').WorkspaceIndex} index
 * @param {(title: string, markdown: string) => Promise<void>} showReport
 */
async function workspaceReport(index, showReport) {
    const lines = ['# Skylake Tuning — workspace report', ''];

    lines.push(
        `Indexed **${index.files.size}** preset files, **${index.targets.size}** targets, ` +
            `**${index.registrations.length}** registrations across **${index.scannedLists.length}** CMakeLists.txt files.`,
        ''
    );

    lines.push('## Registration order', '');
    if (index.registrations.length === 0) {
        lines.push('_No `skl_add_presets_file()` calls were found._', '');
    } else {
        lines.push('Ties in priority are broken by this order — earlier wins.', '');
        lines.push('| # | File | Priority | Presets | Notes |');
        lines.push('| --- | --- | --- | --- | --- |');
        for (const registration of index.registrations) {
            const file = index.files.get(normalize(registration.filePath));
            lines.push(
                `| ${registration.order} | \`${label(registration.filePath)}\` | ` +
                    `${file?.doc?.priority ?? '?'} | ` +
                    `${(file?.doc?.presets ?? []).map((preset) => preset.name).join(', ') || '—'} | ` +
                    `${registration.conditional ? 'inside a conditional block' : ''} |`
            );
        }
        lines.push('');
    }

    const unregistered = index
        .allFiles()
        .filter((file) => {
            const status = index.registrationStatus(file.id);
            return !status.registered && status.asDefaultFor.length === 0;
        });

    lines.push('## Files CMake never reads', '');
    if (index.registrations.length === 0) {
        lines.push('_Skipped: no CMake information._', '');
    } else if (unregistered.length === 0) {
        lines.push('None — every preset file is registered or used as a default layer.', '');
    } else {
        for (const file of unregistered) {
            lines.push(`- \`${label(file.id)}\` — priority ${file.doc?.priority}, never registered`);
        }
        lines.push('');
    }

    const orphans = [...index.targets.values()].filter(
        (info) => info.consumers.length === 0 && info.fileIds.length > 0
    );
    lines.push('## Targets with no consumer', '');
    if (index.tuneTargets.length === 0) {
        lines.push('_Skipped: no `skl_add_tune_header_to_target()` calls found._', '');
    } else if (orphans.length === 0) {
        lines.push('None — every configured target is consumed by a build target.', '');
    } else {
        for (const info of orphans) {
            lines.push(
                `- \`${info.name}\` — configured in ${info.fileIds.length} file(s), consumed by nothing`
            );
        }
        lines.push('');
    }

    lines.push('## Targets', '');
    lines.push('| Target | Consumers | Default layer | Presets | Resolved keys |');
    lines.push('| --- | --- | --- | --- | --- |');
    for (const info of [...index.targets.values()].sort((a, b) => a.name.localeCompare(b.name))) {
        const presetName = info.presetNames[0] ?? index.defaultPresetName;
        const resolved = index.resolve(info.name, presetName);
        lines.push(
            `| \`${info.name}\` | ${info.consumers.map((consumer) => consumer.cmakeTarget).join(', ') || '—'} | ` +
                `${info.defaultPresetFile ? `\`${label(info.defaultPresetFile)}\`` : '—'} | ` +
                `${info.presetNames.join(', ') || '—'} | ${resolved.entries.size} |`
        );
    }

    await showReport('workspace-report', lines.join('\n'));
}

/** @param {string} fileId */
function label(fileId) {
    return `${path.basename(path.dirname(fileId))}/${path.basename(fileId)}`;
}

/** @param {string} fsPath */
function normalize(fsPath) {
    const normalized = path.normalize(fsPath).replace(/[\\/]+$/, '');
    return process.platform === 'win32' ? normalized.toLowerCase() : normalized;
}

module.exports = { registerCommands };
