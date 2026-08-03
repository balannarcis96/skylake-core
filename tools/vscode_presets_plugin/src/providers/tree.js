/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2025 Balan Narcis (balannarcis96@gmail.com)
 *
 * Sidebar tree.
 *
 * Organized target-first rather than file-first, because the question worth
 * answering is "what does this target compile with under this preset", and the
 * answer is spread across several files. The Files section is the inverse view
 * and mainly exists to make registration status visible at a glance.
 */
'use strict';

const path = require('path');
const vscode = require('vscode');

/**
 * @typedef {Object} Node
 * @property {'section'|'target'|'preset'|'group'|'key'|'file'} kind
 * @property {string} label
 * @property {string} [description]
 * @property {string} [tooltip]
 * @property {*} [data]
 */

class TuningTreeProvider {
    /**
     * @param {import('../core/workspaceIndex').WorkspaceIndex} index
     */
    constructor(index) {
        this.index = index;
        this._emitter = new vscode.EventEmitter();
        this.onDidChangeTreeData = this._emitter.event;
        this._subscription = index.onDidChange(() => this._emitter.fire());
    }

    dispose() {
        this._subscription.dispose();
        this._emitter.dispose();
    }

    /**
     * @param {Node} node
     * @returns {vscode.TreeItem}
     */
    getTreeItem(node) {
        const collapsible =
            node.kind === 'key'
                ? vscode.TreeItemCollapsibleState.None
                : node.kind === 'section'
                  ? vscode.TreeItemCollapsibleState.Expanded
                  : vscode.TreeItemCollapsibleState.Collapsed;

        const item = new vscode.TreeItem(node.label, collapsible);
        item.description = node.description;
        item.tooltip = node.tooltip ? new vscode.MarkdownString(node.tooltip) : undefined;
        item.contextValue = node.kind;

        switch (node.kind) {
            case 'section':
                item.iconPath = new vscode.ThemeIcon(node.data === 'files' ? 'files' : 'symbol-namespace');
                break;
            case 'target':
                item.iconPath = new vscode.ThemeIcon('package');
                break;
            case 'preset':
                item.iconPath = new vscode.ThemeIcon('settings-gear');
                break;
            case 'group':
                item.iconPath = new vscode.ThemeIcon('symbol-folder');
                break;
            case 'key':
                item.iconPath = new vscode.ThemeIcon(
                    node.data.kind === 'define' ? 'symbol-constant' : 'symbol-numeric'
                );
                item.command = {
                    command: 'skylakeTuning.revealNode',
                    title: 'Reveal',
                    arguments: [node]
                };
                break;
            case 'file': {
                item.resourceUri = vscode.Uri.file(node.data.id);
                item.iconPath = new vscode.ThemeIcon('json');
                item.command = {
                    command: 'skylakeTuning.openEditor',
                    title: 'Open',
                    arguments: [vscode.Uri.file(node.data.id)]
                };
                break;
            }
            default:
                break;
        }

        return item;
    }

    /**
     * @param {Node} [node]
     * @returns {Node[]}
     */
    getChildren(node) {
        if (!node) {
            if (this.index.files.size === 0) {
                return [];
            }
            return [
                { kind: 'section', label: 'Targets', data: 'targets' },
                { kind: 'section', label: 'Files', data: 'files' }
            ];
        }

        switch (node.kind) {
            case 'section':
                return node.data === 'files' ? this._files() : this._targets();
            case 'target':
                return this._presets(node.data);
            case 'preset':
                return this._groups(node.data.target, node.data.preset);
            case 'group':
                return this._keys(node.data.target, node.data.preset, node.data.group);
            default:
                return [];
        }
    }

    /** @returns {Node[]} */
    _targets() {
        return [...this.index.targets.values()]
            .sort((a, b) => a.name.localeCompare(b.name))
            .map((info) => {
                const consumers = info.consumers.map((consumer) => consumer.cmakeTarget).filter(Boolean);
                return {
                    kind: 'target',
                    label: info.name,
                    description:
                        consumers.length > 0
                            ? consumers.join(', ')
                            : info.fileIds.length > 0
                              ? 'no consumer'
                              : 'no config',
                    tooltip: [
                        `**${info.name}**`,
                        '',
                        consumers.length > 0
                            ? `Consumed by: ${consumers.join(', ')}`
                            : '⚠ No `skl_add_tune_header_to_target()` call uses this target name.',
                        `Declared in ${info.fileIds.length} preset file(s).`,
                        info.defaultPresetFile
                            ? `Default layer: \`${path.basename(info.defaultPresetFile)}\``
                            : 'No default layer identified.'
                    ].join('\n'),
                    data: info.name
                };
            });
    }

    /**
     * @param {string} targetName
     * @returns {Node[]}
     */
    _presets(targetName) {
        const info = this.index.targets.get(targetName);
        const names = new Set(info?.presetNames ?? []);
        const defaultName = info?.defaultPresetName ?? this.index.defaultPresetName;
        names.delete(defaultName);

        /** @type {Node[]} */
        const out = [];
        for (const name of [defaultName, ...[...names].sort()]) {
            const result = this.index.resolve(targetName, name);
            out.push({
                kind: 'preset',
                label: name,
                description: `${result.entries.size} values`,
                tooltip: [
                    `**${targetName}** under preset **${name}**`,
                    '',
                    `Resolved values: ${result.entries.size}`,
                    `Namespace: \`${result.namespace || '(global)'}\``,
                    `Reported Priority: ${result.maxPriority} _(max scanned, not the winning one)_`
                ].join('\n'),
                data: { target: targetName, preset: name }
            });
        }
        return out;
    }

    /**
     * @param {string} targetName
     * @param {string} presetName
     * @returns {Node[]}
     */
    _groups(targetName, presetName) {
        const result = this.index.resolve(targetName, presetName);
        /** @type {Map<string, number>} */
        const groups = new Map();
        for (const entry of result.entries.values()) {
            groups.set(entry.group, (groups.get(entry.group) ?? 0) + 1);
        }
        return [...groups.entries()]
            .sort((a, b) => a[0].localeCompare(b[0]))
            .map(([group, count]) => ({
                kind: 'group',
                label: group,
                description: String(count),
                data: { target: targetName, preset: presetName, group }
            }));
    }

    /**
     * @param {string} targetName
     * @param {string} presetName
     * @param {string} group
     * @returns {Node[]}
     */
    _keys(targetName, presetName, group) {
        const result = this.index.resolve(targetName, presetName);
        return [...result.entries.values()]
            .filter((entry) => entry.group === group)
            .sort((a, b) => a.key.localeCompare(b.key))
            .map((entry) => ({
                kind: 'key',
                label: entry.key,
                description: entry.value,
                tooltip: [
                    `**${entry.key}** = \`${entry.value}\``,
                    '',
                    entry.desc ? `${entry.desc}\n` : '',
                    `Kind: ${entry.kind}${entry.type ? ` (\`${entry.type}\`)` : ''}`,
                    `Header: ${entry.output}`,
                    `Winner: \`${label(entry.fileId)}\` at priority ${entry.priority}`,
                    entry.overridden
                        ? `\nLayers:\n${entry.trace
                              .map(
                                  (contribution) =>
                                      `- ${contribution.applied ? '**' : ''}${label(contribution.fileId)}` +
                                      `${contribution.applied ? '**' : ''} (prio ${contribution.priority}) → \`${contribution.value}\`` +
                                      `${contribution.applied ? '' : ` — ${contribution.reason ?? 'not applied'}`}`
                              )
                              .join('\n')}`
                        : '\nOnly one layer defines this key.'
                ].join('\n'),
                data: { ...entry, target: targetName, preset: presetName }
            }));
    }

    /** @returns {Node[]} */
    _files() {
        return this.index.allFiles().map((file) => {
            const status = this.index.registrationStatus(file.id);
            const presets = (file.doc?.presets ?? []).map((preset) => preset.name).join(', ');
            const badge = status.registered
                ? `#${status.registration?.order}`
                : status.asDefaultFor.length > 0
                  ? 'default layer'
                  : '⚠ unregistered';
            return {
                kind: 'file',
                label: path.basename(file.id),
                description: `${path.basename(path.dirname(file.id))} · prio ${file.doc?.priority ?? '?'} · ${badge}`,
                tooltip: [
                    `\`${file.id}\``,
                    '',
                    `Priority: ${file.doc?.priority}`,
                    `Presets: ${presets || '(none)'}`,
                    status.registered
                        ? `Registered as #${status.registration?.order} in \`${status.registration?.declaredIn}\``
                        : status.asDefaultFor.length > 0
                          ? `Used as DEFAULT_PRESET_FILE for: ${status.asDefaultFor.join(', ')}`
                          : '⚠ Never registered — CMake does not read this file.'
                ].join('\n'),
                data: { id: file.id }
            };
        });
    }
}

/** @param {string} fileId */
function label(fileId) {
    return `${path.basename(path.dirname(fileId))}/${path.basename(fileId)}`;
}

module.exports = { TuningTreeProvider };
