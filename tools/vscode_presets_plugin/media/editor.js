/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2025 Balan Narcis (balannarcis96@gmail.com)
 *
 * Webview front end for the tuning preset editor.
 *
 * Plain DOM, no framework and no build step. The view is a pure function of
 * (model, uiState); every mutation is sent to the extension host, which edits
 * the TextDocument and pushes a fresh model back.
 *
 * Two rules drive the layout:
 *
 *   1. Show one target at a time. A preset file can hold several target configs
 *      with dozens of keys each; putting them all on one page turns the whole
 *      thing into noise. The rail picks the target, the middle column shows it,
 *      the inspector explains one key in depth.
 *
 *   2. Never write silently. A typed value is staged, not applied: it lands
 *      only on Enter or an explicit Apply, and clicking away offers Apply or
 *      Discard rather than guessing. Everything that does get written is
 *      reversible from the toast or the Undo button.
 */
/* eslint-env browser */
'use strict';

(function () {
    const vscode = acquireVsCodeApi();
    const root = document.getElementById('root');

    /** @type {any} */
    let model = null;
    let docUri = null;
    let dirty = false;
    let canUndo = false;
    let canRedo = false;

    /** Staged edits awaiting confirmation, keyed by row id. */
    /** @type {Map<string, string>} */
    const pending = new Map();

    const persisted = vscode.getState() || {};
    const ui = {
        search: persisted.search || '',
        filters: new Set(persisted.filters || []),
        collapsed: new Set(persisted.collapsed || []),
        activePreset: persisted.activePreset || 0,
        activeConfig: persisted.activeConfig || 0,
        showInherited: persisted.showInherited === true,
        density: persisted.density || 'comfortable',
        railHidden: persisted.railHidden === true,
        inspectorOpen: persisted.inspectorOpen !== false,
        /** @type {{group: string, key: string, inherited?: boolean}|null} */
        selected: persisted.selected || null,
        searchCursor: 0,
        scrollTop: 0
    };

    function saveState() {
        vscode.setState({
            search: ui.search,
            filters: [...ui.filters],
            collapsed: [...ui.collapsed],
            activePreset: ui.activePreset,
            activeConfig: ui.activeConfig,
            showInherited: ui.showInherited,
            density: ui.density,
            railHidden: ui.railHidden,
            inspectorOpen: ui.inspectorOpen,
            selected: ui.selected
        });
    }

    // ================================================================= utils

    /**
     * @param {string} tag
     * @param {Record<string, any>} [attrs]
     * @param {(Node|string|null|undefined|false)[]} [children]
     */
    function el(tag, attrs, children) {
        const node = document.createElement(tag);
        for (const [key, value] of Object.entries(attrs || {})) {
            if (value === undefined || value === null || value === false) {
                continue;
            }
            if (key === 'class') {
                node.className = value;
            } else if (key === 'text') {
                node.textContent = String(value);
            } else if (key.startsWith('on') && typeof value === 'function') {
                node.addEventListener(key.slice(2).toLowerCase(), value);
            } else {
                node.setAttribute(key, value === true ? '' : String(value));
            }
        }
        for (const child of children || []) {
            if (child === null || child === undefined || child === false) {
                continue;
            }
            node.appendChild(typeof child === 'string' ? document.createTextNode(child) : child);
        }
        return node;
    }

    const ICONS = {
        search: 'M7 12a5 5 0 1 0 0-10 5 5 0 0 0 0 10ZM14.5 14.5 10.6 10.6',
        chevron: 'M6 3.5 10.5 8 6 12.5',
        add: 'M8 3.2v9.6M3.2 8h9.6',
        trash: 'M2.8 4.4h10.4M6.2 4.4V2.8h3.6v1.6M4.2 4.4l.6 8.8h6.4l.6-8.8',
        text: 'M2.8 4h10.4M2.8 8h10.4M2.8 12h6',
        undo: 'M4 7.5h6.2a3 3 0 0 1 0 6H6M4 7.5 6.8 4.7M4 7.5l2.8 2.8',
        redo: 'M12 7.5H5.8a3 3 0 0 0 0 6H10M12 7.5 9.2 4.7M12 7.5l-2.8 2.8',
        save: 'M3 3h8l2 2v8H3V3ZM5.5 3v4h5V3M5.5 13v-3.5h5V13',
        warn: 'M8 6v3.4M8 11.6v.1M6.9 2.6 1.7 12a1.2 1.2 0 0 0 1.1 1.9h10.4A1.2 1.2 0 0 0 14.3 12L9.1 2.6a1.2 1.2 0 0 0-2.2 0Z',
        info: 'M8 7.4v4.2M8 4.6v.1M8 14.4A6.4 6.4 0 1 0 8 1.6a6.4 6.4 0 0 0 0 12.8Z',
        check: 'M3 8.4 6.2 11.6 13 4.8',
        close: 'M4 4l8 8M12 4l-8 8',
        panel: 'M2 3h12v10H2V3ZM6 3v10',
        target: 'M8 14.4A6.4 6.4 0 1 0 8 1.6a6.4 6.4 0 0 0 0 12.8ZM8 11a3 3 0 1 0 0-6 3 3 0 0 0 0 6Z',
        up: 'M8 12V4M4.5 7.5 8 4l3.5 3.5',
        down: 'M8 4v8M4.5 8.5 8 12l3.5-3.5',
        goto: 'M6.5 9.5 13 3M9 3h4v4M13 9.5V13H3V3h3.5'
    };

    /**
     * @param {keyof typeof ICONS} name
     * @param {boolean} [small]
     */
    function icon(name, small) {
        const svg = document.createElementNS('http://www.w3.org/2000/svg', 'svg');
        svg.setAttribute('class', small ? 'i sm' : 'i');
        svg.setAttribute('viewBox', '0 0 16 16');
        svg.setAttribute('aria-hidden', 'true');
        const p = document.createElementNS('http://www.w3.org/2000/svg', 'path');
        p.setAttribute('d', ICONS[name]);
        svg.appendChild(p);
        return svg;
    }

    /**
     * @param {string} text
     * @param {string} query
     */
    function highlight(text, query) {
        const value = String(text ?? '');
        if (!query) {
            return document.createTextNode(value);
        }
        const frag = document.createDocumentFragment();
        const hay = value.toLowerCase();
        const needle = query.toLowerCase();
        let from = 0;
        for (;;) {
            const at = hay.indexOf(needle, from);
            if (at === -1) {
                break;
            }
            if (at > from) {
                frag.appendChild(document.createTextNode(value.slice(from, at)));
            }
            frag.appendChild(el('mark', { text: value.slice(at, at + needle.length) }));
            from = at + needle.length;
        }
        frag.appendChild(document.createTextNode(value.slice(from)));
        return frag;
    }

    /** @param {any} message */
    function send(message) {
        vscode.postMessage(message);
    }

    /**
     * @param {string} text
     * @param {{label: string, run: () => void}} [action]
     */
    function toast(text, action) {
        let host = document.querySelector('.toasts');
        if (!host) {
            host = el('div', { class: 'toasts' });
            document.body.appendChild(host);
        }
        const node = el('div', { class: 'toast', role: 'status' }, [
            el('span', { class: 'txt', text }),
            action &&
                el('button', {
                    class: 'btn sm',
                    text: action.label,
                    onclick: () => {
                        action.run();
                        node.remove();
                    }
                }),
            el('button', { class: 'btn ghost icon sm', title: 'Dismiss', onclick: () => node.remove() }, [
                icon('close', true)
            ])
        ]);
        host.appendChild(node);
        setTimeout(() => node.remove(), 7000);
    }

    // =============================================================== filters

    const FILTERS = [
        { id: 'override', label: 'Overrides', status: 'override', color: 'var(--override)' },
        { id: 'introduced', label: 'New', status: 'introduced', color: 'var(--introduced)' },
        { id: 'redundant', label: 'Redundant', status: 'redundant', color: 'var(--redundant)' },
        { id: 'shadowed', label: 'Shadowed', status: 'shadowed', color: 'var(--shadowed)' },
        { id: 'problem', label: 'Problems', status: null, color: 'var(--danger)' }
    ];

    function matchesFilters(entry) {
        if (ui.filters.size === 0) {
            return true;
        }
        for (const filter of ui.filters) {
            if (filter === 'problem') {
                if (entry.problems && entry.problems.length > 0) {
                    return true;
                }
            } else if (entry.status === filter) {
                return true;
            }
        }
        return false;
    }

    function matchesSearch(entry) {
        if (!ui.search) {
            return true;
        }
        const needle = ui.search.toLowerCase();
        return (
            String(entry.key).toLowerCase().includes(needle) ||
            String(entry.value ?? '').toLowerCase().includes(needle) ||
            String(entry.desc ?? '').toLowerCase().includes(needle) ||
            String(entry.type ?? '').toLowerCase().includes(needle) ||
            String(entry.group ?? '').toLowerCase().includes(needle)
        );
    }

    // ============================================================== accessors

    function activePreset() {
        return model?.presets?.[ui.activePreset] ?? model?.presets?.[0] ?? null;
    }

    function activeConfig() {
        const preset = activePreset();
        if (!preset) {
            return null;
        }
        return preset.configs[ui.activeConfig] ?? preset.configs[0] ?? null;
    }

    function configCounts(config) {
        const counts = { total: 0, override: 0, introduced: 0, redundant: 0, shadowed: 0, problem: 0 };
        for (const group of config.groups) {
            for (const entry of group.entries) {
                counts.total++;
                if (counts[entry.status] !== undefined) {
                    counts[entry.status]++;
                }
                if (entry.problems?.length) {
                    counts.problem++;
                }
            }
        }
        return counts;
    }

    /** Rows currently visible under search + filters, in display order. */
    function visibleRows() {
        const config = activeConfig();
        if (!config) {
            return [];
        }
        const out = [];
        for (const group of config.groups) {
            if (ui.collapsed.has(groupId(group.name))) {
                continue;
            }
            for (const entry of group.entries) {
                if (matchesSearch(entry) && matchesFilters(entry)) {
                    out.push({ group: group.name, key: entry.key });
                }
            }
        }
        if (ui.showInherited && ui.filters.size === 0) {
            for (const entry of config.inheritedOnly) {
                if (matchesSearch(entry)) {
                    out.push({ group: '__inherited', key: entry.key, inherited: true });
                }
            }
        }
        return out;
    }

    function groupId(name) {
        return `${ui.activePreset}:${ui.activeConfig}:${name}`;
    }

    function rowId(group, key) {
        return `${ui.activePreset}:${ui.activeConfig}:${group}:${key}`;
    }

    function findEntry(group, key) {
        const config = activeConfig();
        const bucket = config?.groups.find((candidate) => candidate.name === group);
        return bucket?.entries.find((candidate) => candidate.key === key);
    }

    function findInherited(key) {
        return activeConfig()?.inheritedOnly.find((candidate) => candidate.key === key);
    }

    // ================================================================ render

    function render() {
        const previous = root.querySelector('.scroll');
        const keepScroll = previous ? previous.scrollTop : 0;
        const activeId = document.activeElement?.dataset?.fid ?? null;
        const caret =
            document.activeElement && 'selectionStart' in document.activeElement
                ? document.activeElement.selectionStart
                : null;

        document.body.className = `density-${ui.density}`;
        root.textContent = '';

        if (!model) {
            root.appendChild(el('div', { class: 'loading', text: 'Loading…' }));
            return;
        }
        if (!model.ok) {
            root.appendChild(renderParseError());
            return;
        }

        const shell = el('div', {
            class:
                'shell' +
                (ui.railHidden ? ' rail-hidden' : '') +
                (ui.inspectorOpen && ui.selected ? ' inspector-open' : '')
        });

        shell.appendChild(renderTopbar());
        shell.appendChild(renderPresetTabs());

        const main = el('div', { class: 'main' });
        main.appendChild(renderRail());

        const content = el('div', { class: 'content' });
        content.appendChild(renderToolbar());

        const scroll = el('div', { class: 'scroll' });
        const config = activeConfig();
        if (!config) {
            scroll.appendChild(renderNoConfig());
        } else {
            scroll.appendChild(renderTargetHead(config));
            const rows = visibleRows();
            if (rows.length === 0 && (ui.search || ui.filters.size > 0)) {
                scroll.appendChild(renderNoMatches());
            }
            for (const group of config.groups) {
                scroll.appendChild(renderGroup(config, group));
            }
            scroll.appendChild(
                el('div', { style: 'padding:10px 8px' }, [
                    el(
                        'button',
                        {
                            class: 'btn ghost',
                            onclick: () =>
                                send({
                                    type: 'addGroup',
                                    presetIndex: ui.activePreset,
                                    configIndex: ui.activeConfig
                                })
                        },
                        [icon('add'), 'Add group']
                    )
                ])
            );
            if (ui.showInherited) {
                scroll.appendChild(renderInherited(config));
            }
            scroll.appendChild(renderLegend());
        }
        content.appendChild(scroll);
        main.appendChild(content);
        main.appendChild(renderInspector());

        shell.appendChild(main);
        root.appendChild(shell);

        scroll.scrollTop = keepScroll;

        if (activeId) {
            const restored = root.querySelector(`[data-fid="${cssEscape(activeId)}"]`);
            if (restored) {
                restored.focus();
                if (caret !== null && 'setSelectionRange' in restored) {
                    try {
                        restored.setSelectionRange(caret, caret);
                    } catch {
                        /* not a text input */
                    }
                }
            }
        }
        saveState();
    }

    function cssEscape(value) {
        return String(value).replace(/["\\]/g, '\\$&');
    }

    // ---------------------------------------------------------------- topbar

    function renderTopbar() {
        const reg = model.registration || {};
        const badges = [];

        if (!reg.cmakeKnown) {
            badges.push(
                el('span', { class: 'badge mute', title: 'No CMakeLists.txt was scanned in this workspace' }, [
                    'CMake unknown'
                ])
            );
        } else if (reg.registered) {
            badges.push(
                el(
                    'span',
                    {
                        class: 'badge ok',
                        title: `Registered by skl_add_presets_file() as #${reg.order} in ${reg.declaredIn}${
                            reg.conditional ? '\nInside a conditional block, so it may not always apply.' : ''
                        }`
                    },
                    [el('span', { class: 'swatch' }), `#${reg.order}${reg.conditional ? '?' : ''}`]
                )
            );
        } else if (reg.asDefaultFor?.length) {
            badges.push(
                el(
                    'span',
                    { class: 'badge ok', title: `DEFAULT_PRESET_FILE for: ${reg.asDefaultFor.join(', ')}` },
                    [el('span', { class: 'swatch' }), 'base layer']
                )
            );
        } else {
            badges.push(
                el('span', { class: 'badge err', title: 'CMake never reads this file' }, [
                    el('span', { class: 'swatch' }),
                    'unregistered'
                ])
            );
        }

        return el('header', { class: 'topbar' }, [
            el('div', { class: 'identity' }, [
                dirty ? el('span', { class: 'dirty-dot', title: 'Unsaved changes' }) : null,
                el('h1', { text: model.fileName }),
                el('span', { class: 'path', text: model.relPath, title: model.relPath })
            ]),
            ...badges,
            el('span', { class: 'grow' }),
            stepper('Priority', model.priority, 'priority'),
            stepper('Version', model.version, 'version'),
            el('div', { style: 'display:flex;gap:4px' }, [
                el(
                    'button',
                    {
                        class: 'btn ghost icon',
                        title: 'Undo last change  (Alt+Z)',
                        disabled: !canUndo,
                        onclick: () => send({ type: 'undo' })
                    },
                    [icon('undo')]
                ),
                el(
                    'button',
                    {
                        class: 'btn ghost icon',
                        title: 'Redo  (Alt+Shift+Z)',
                        disabled: !canRedo,
                        onclick: () => send({ type: 'redo' })
                    },
                    [icon('redo')]
                ),
                el(
                    'button',
                    {
                        class: dirty ? 'btn primary' : 'btn ghost icon',
                        title: 'Save  (Ctrl+S)',
                        disabled: !dirty,
                        onclick: () => send({ type: 'save' })
                    },
                    dirty ? [icon('save'), 'Save'] : [icon('save')]
                ),
                el(
                    'button',
                    {
                        class: 'btn ghost icon',
                        title: 'Open the raw JSON',
                        onclick: () => send({ type: 'reopenAsText' })
                    },
                    [icon('text')]
                )
            ])
        ]);
    }

    /**
     * @param {string} label
     * @param {number} value
     * @param {'priority'|'version'} field
     */
    function stepper(label, value, field) {
        const commit = (next) => {
            if (next !== value) {
                send({ type: 'setHeader', patch: { [field]: next } });
            }
        };
        const input = el('input', {
            type: 'text',
            value: String(value),
            'aria-label': label,
            'data-fid': `hdr-${field}`,
            onkeydown: (event) => {
                if (event.key === 'Enter') {
                    event.preventDefault();
                    input.blur();
                } else if (event.key === 'Escape') {
                    input.value = String(value);
                    input.blur();
                } else if (event.key === 'ArrowUp' || event.key === 'ArrowDown') {
                    event.preventDefault();
                    input.value = String(
                        Math.max(0, (Number(input.value) || 0) + (event.key === 'ArrowUp' ? 1 : -1))
                    );
                }
            },
            onblur: () => {
                const next = Number(input.value);
                if (Number.isFinite(next) && Math.trunc(next) !== value) {
                    commit(Math.max(0, Math.trunc(next)));
                } else {
                    input.value = String(value);
                }
            }
        });
        const bump = (delta) => commit(Math.max(0, value + delta));

        return el('div', { class: 'stepper' }, [
            el('label', { text: label }),
            el('div', { class: 'box' }, [
                el('button', { text: '−', title: `Decrease ${label}`, onclick: () => bump(-1) }),
                input,
                el('button', { text: '+', title: `Increase ${label}`, onclick: () => bump(1) })
            ])
        ]);
    }

    // ------------------------------------------------------------ preset bar

    function renderPresetTabs() {
        const bar = el('div', { class: 'presetbar', role: 'tablist' });
        model.presets.forEach((preset, index) => {
            const count = preset.configs.reduce(
                (sum, config) => sum + config.groups.reduce((n, g) => n + g.entries.length, 0),
                0
            );
            const active = index === ui.activePreset;
            bar.appendChild(
                el(
                    'button',
                    {
                        class: 'preset-tab',
                        role: 'tab',
                        'aria-selected': String(active),
                        title:
                            `Preset '${preset.name}' — selected by -DSKL_TUNE_PRESET=${preset.name}\n` +
                            'Double-click to rename.',
                        onclick: () => {
                            if (!confirmDiscardPending()) {
                                return;
                            }
                            ui.activePreset = index;
                            ui.activeConfig = 0;
                            ui.selected = null;
                            render();
                        },
                        ondblclick: () =>
                            send({ type: 'renamePreset', presetIndex: index, name: preset.name })
                    },
                    [
                        preset.name || '(unnamed)',
                        el('span', { class: 'n', text: String(count) }),
                        active
                            ? el('span', {
                                  class: 'tab-x',
                                  role: 'button',
                                  text: '×',
                                  title: `Delete the '${preset.name}' preset block`,
                                  onclick: (event) => {
                                      event.stopPropagation();
                                      send({
                                          type: 'deletePreset',
                                          presetIndex: index,
                                          name: preset.name
                                      });
                                  }
                              })
                            : null
                    ]
                )
            );
        });
        bar.appendChild(
            el(
                'button',
                { class: 'preset-tab', title: 'Add a preset block', onclick: () => send({ type: 'addPreset' }) },
                [icon('add', true)]
            )
        );
        return bar;
    }

    // ------------------------------------------------------------------ rail

    function renderRail() {
        const preset = activePreset();
        const rail = el('aside', { class: 'rail', 'aria-label': 'Targets' });

        rail.appendChild(el('div', { class: 'rail-title', text: 'Targets in this preset' }));

        (preset?.configs ?? []).forEach((config, index) => {
            const counts = configCounts(config);
            const flagged = counts.shadowed + counts.problem;
            rail.appendChild(
                el(
                    'button',
                    {
                        class: 'rail-item',
                        'aria-current': String(index === ui.activeConfig),
                        title:
                            `${config.targetName}\n${counts.total} keys` +
                            (flagged ? `\n${flagged} need attention` : ''),
                        onclick: () => {
                            if (!confirmDiscardPending()) {
                                return;
                            }
                            ui.activeConfig = index;
                            ui.selected = null;
                            render();
                        }
                    },
                    [
                        el('span', { class: 'name', text: config.targetName || '(unnamed)' }),
                        flagged > 0 ? el('span', { class: 'warn-dot' }) : null,
                        el('span', { class: 'n', text: String(counts.total) })
                    ]
                )
            );
        });

        rail.appendChild(
            el(
                'button',
                {
                    class: 'btn ghost sm',
                    style: 'width:100%;justify-content:flex-start;margin-top:6px',
                    onclick: () => send({ type: 'addConfig', presetIndex: ui.activePreset })
                },
                [icon('add', true), 'Add target']
            )
        );

        return rail;
    }

    // --------------------------------------------------------------- toolbar

    function renderToolbar() {
        const config = activeConfig();
        const counts = config ? configCounts(config) : { total: 0 };
        const rows = visibleRows();

        const input = el('input', {
            type: 'text',
            placeholder: 'Search keys, values, descriptions…',
            value: ui.search,
            'aria-label': 'Search',
            'data-fid': 'search',
            oninput: () => {
                ui.search = input.value;
                ui.searchCursor = 0;
                render();
            },
            onkeydown: (event) => {
                if (event.key === 'Escape') {
                    ui.search = '';
                    render();
                } else if (event.key === 'Enter') {
                    event.preventDefault();
                    stepSearch(event.shiftKey ? -1 : 1);
                }
            }
        });

        const chips = el('div', { class: 'chips' });
        for (const filter of FILTERS) {
            const n = filter.id === 'problem' ? counts.problem : counts[filter.status] ?? 0;
            if (n === 0 && !ui.filters.has(filter.id)) {
                continue;
            }
            chips.appendChild(
                el(
                    'button',
                    {
                        class: 'chip',
                        'aria-pressed': String(ui.filters.has(filter.id)),
                        title: `Show only: ${filter.label}`,
                        onclick: () => {
                            if (ui.filters.has(filter.id)) {
                                ui.filters.delete(filter.id);
                            } else {
                                ui.filters.add(filter.id);
                            }
                            render();
                        }
                    },
                    [
                        el('span', { class: 'swatch', style: `background:${filter.color}` }),
                        filter.label,
                        el('span', { class: 'n', text: String(n) })
                    ]
                )
            );
        }

        return el('div', { class: 'toolbar' }, [
            el(
                'button',
                {
                    class: 'btn ghost icon',
                    title: ui.railHidden ? 'Show target list' : 'Hide target list',
                    onclick: () => {
                        ui.railHidden = !ui.railHidden;
                        render();
                    }
                },
                [icon('panel')]
            ),
            el('div', { class: 'search' }, [
                el('span', { class: 'lead' }, [icon('search')]),
                input,
                el('div', { class: 'tail' }, [
                    ui.search
                        ? el('span', {
                              class: 'matches',
                              text: rows.length ? `${Math.min(ui.searchCursor + 1, rows.length)}/${rows.length}` : '0'
                          })
                        : null,
                    ui.search
                        ? el(
                              'button',
                              {
                                  class: 'btn ghost icon sm',
                                  title: 'Previous match  (Shift+Enter)',
                                  onclick: () => stepSearch(-1)
                              },
                              [icon('up', true)]
                          )
                        : null,
                    ui.search
                        ? el(
                              'button',
                              { class: 'btn ghost icon sm', title: 'Next match  (Enter)', onclick: () => stepSearch(1) },
                              [icon('down', true)]
                          )
                        : null,
                    ui.search
                        ? el(
                              'button',
                              {
                                  class: 'btn ghost icon sm',
                                  title: 'Clear  (Esc)',
                                  onclick: () => {
                                      ui.search = '';
                                      render();
                                  }
                              },
                              [icon('close', true)]
                          )
                        : null
                ])
            ]),
            chips,
            el('span', { class: 'grow' }),
            el(
                'button',
                {
                    class: 'chip',
                    'aria-pressed': String(ui.showInherited),
                    title: 'Also list keys this preset inherits without overriding',
                    onclick: () => {
                        ui.showInherited = !ui.showInherited;
                        render();
                    }
                },
                ['Inherited']
            ),
            el(
                'button',
                {
                    class: 'chip',
                    'aria-pressed': String(ui.density === 'compact'),
                    title: 'Toggle compact row height',
                    onclick: () => {
                        ui.density = ui.density === 'compact' ? 'comfortable' : 'compact';
                        render();
                    }
                },
                ['Compact']
            )
        ]);
    }

    /** @param {number} delta */
    function stepSearch(delta) {
        const rows = visibleRows();
        if (rows.length === 0) {
            return;
        }
        ui.searchCursor = (ui.searchCursor + delta + rows.length) % rows.length;
        const target = rows[ui.searchCursor];
        ui.selected = target;
        render();
        const node = root.querySelector(`[data-row="${cssEscape(rowId(target.group, target.key))}"]`);
        if (node) {
            node.classList.add('search-current');
            node.scrollIntoView({ block: 'center' });
        }
    }

    // ----------------------------------------------------------- target head

    function renderTargetHead(config) {
        const counts = configCounts(config);
        const wrap = el('div', {});

        const badges = [];
        if (config.consumers?.length) {
            badges.push(
                el(
                    'span',
                    {
                        class: 'badge mute',
                        title:
                            'Built by:\n' +
                            config.consumers
                                .map((c) => `  ${c.cmakeTarget}  (${c.declaredIn})`)
                                .join('\n')
                    },
                    [config.consumers.map((c) => c.cmakeTarget).filter(Boolean).join(', ')]
                )
            );
        } else {
            badges.push(
                el(
                    'span',
                    {
                        class: 'badge warn',
                        title:
                            'No skl_add_tune_header_to_target() call uses this target name, ' +
                            'so nothing in the build reads this config.'
                    },
                    [icon('warn', true), 'no consumer']
                )
            );
        }
        if (config.isDefaultLayer) {
            badges.push(
                el(
                    'span',
                    {
                        class: 'badge info',
                        title: 'This file is the DEFAULT_PRESET_FILE for the target: it defines the base values every preset inherits.'
                    },
                    ['base layer']
                )
            );
        }

        wrap.appendChild(
            el('div', { class: 'target-head' }, [
                el('div', { class: 'line1' }, [
                    icon('target'),
                    el('h2', { text: config.targetName || '(no target_name)' }),
                    ...badges,
                    el('span', { class: 'grow' }),
                    el('span', {
                        class: 'note',
                        text: `${counts.total} key${counts.total === 1 ? '' : 's'} here · ${config.totalResolved} resolved`
                    }),
                    el(
                        'button',
                        {
                            class: 'btn ghost icon',
                            title: 'Remove this target config from the preset',
                            onclick: () =>
                                send({
                                    type: 'deleteConfig',
                                    presetIndex: ui.activePreset,
                                    configIndex: ui.activeConfig,
                                    targetName: config.targetName
                                })
                        },
                        [icon('trash')]
                    )
                ]),
                el('div', { class: 'line2' }, [
                    el('div', { class: 'prop' }, [
                        el('label', { text: 'Namespace' }),
                        textField(
                            config.namespace ?? '',
                            '(global)',
                            (value) =>
                                send({
                                    type: 'setConfigField',
                                    presetIndex: ui.activePreset,
                                    configIndex: ui.activeConfig,
                                    field: 'constexpr_namespace',
                                    value
                                }),
                            'ns'
                        )
                    ]),
                    el('div', { class: 'prop' }, [
                        el('label', { text: 'Default output' }),
                        selectField(
                            config.defaultOutput || 'private',
                            ['public', 'private'],
                            (value) =>
                                send({
                                    type: 'setConfigField',
                                    presetIndex: ui.activePreset,
                                    configIndex: ui.activeConfig,
                                    field: 'default_output',
                                    value
                                }),
                            'out'
                        )
                    ])
                ])
            ])
        );

        if (config.isDefaultLayer) {
            wrap.appendChild(
                el('div', { class: 'banner info' }, [
                    icon('info'),
                    el('div', {}, [
                        el('b', { class: 'banner-title', text: 'This is the base layer' }),
                        'Every preset for ',
                        el('code', { text: config.targetName }),
                        ' inherits these values. A change here affects dev, qa and prod at once.'
                    ])
                ])
            );
        }

        if (counts.shadowed > 0) {
            wrap.appendChild(
                el('div', { class: 'banner' }, [
                    icon('warn'),
                    el('div', {}, [
                        el('b', {
                            class: 'banner-title',
                            text: `${counts.shadowed} value${counts.shadowed === 1 ? '' : 's'} never reach the compiler`
                        }),
                        'A higher-priority preset file overrides them. Select a row marked ',
                        el('em', { text: 'shadowed' }),
                        ' to see which file wins.'
                    ])
                ])
            );
        }

        return wrap;
    }

    // ---------------------------------------------------------------- groups

    function renderGroup(config, group) {
        const id = groupId(group.name);
        const collapsed = ui.collapsed.has(id);
        const visible = group.entries.filter((entry) => matchesSearch(entry) && matchesFilters(entry));

        if ((ui.search || ui.filters.size > 0) && visible.length === 0) {
            return document.createDocumentFragment();
        }

        const head = el(
            'button',
            {
                class: 'group-head',
                'aria-expanded': String(!collapsed),
                onclick: (event) => {
                    if (event.target.closest('.actions')) {
                        return;
                    }
                    if (collapsed) {
                        ui.collapsed.delete(id);
                    } else {
                        ui.collapsed.add(id);
                    }
                    render();
                }
            },
            [
                el('span', { class: 'chev' }, [icon('chevron', true)]),
                el('span', { class: 'gname', text: group.name }),
                el('span', {
                    class: 'n',
                    text:
                        visible.length === group.entries.length
                            ? `${group.entries.length}`
                            : `${visible.length} of ${group.entries.length}`
                }),
                group.name.startsWith('constexprs.')
                    ? el('span', {
                          class: 'badge mute',
                          text: 'cosmetic',
                          title:
                              'The generator flattens every constexprs.<suffix> bucket into one table. ' +
                              'The suffix only groups the JSON for humans.'
                      })
                    : null,
                el('span', { class: 'actions' }, [
                    el(
                        'span',
                        {
                            class: 'btn ghost icon sm',
                            role: 'button',
                            title: `Add an entry to ${group.name}`,
                            onclick: (event) => {
                                event.stopPropagation();
                                send({
                                    type: 'addEntry',
                                    presetIndex: ui.activePreset,
                                    configIndex: ui.activeConfig,
                                    group: group.name
                                });
                            }
                        },
                        [icon('add', true)]
                    ),
                    el(
                        'span',
                        {
                            class: 'btn ghost icon sm danger',
                            role: 'button',
                            title: `Delete the ${group.name} group`,
                            onclick: (event) => {
                                event.stopPropagation();
                                send({
                                    type: 'deleteGroup',
                                    presetIndex: ui.activePreset,
                                    configIndex: ui.activeConfig,
                                    group: group.name,
                                    count: group.entries.length
                                });
                            }
                        },
                        [icon('trash', true)]
                    )
                ])
            ]
        );

        const rows = el('div', { class: 'rows' });
        for (const entry of visible) {
            rows.appendChild(renderRow(group, entry));
        }
        if (visible.length === 0) {
            rows.appendChild(
                el('div', { class: 'note', style: 'padding:8px 12px', text: 'No entries in this group yet.' })
            );
        }

        return el('div', { class: `group${collapsed ? ' collapsed' : ''}` }, [head, rows]);
    }

    // ------------------------------------------------------------------ rows

    function renderRow(group, entry) {
        const id = rowId(group.name, entry.key);
        const selected = ui.selected?.group === group.name && ui.selected?.key === entry.key;
        const hasProblem = entry.problems?.length > 0;
        const staged = pending.get(id);

        const context = {
            webviewSection: 'key',
            preventDefaultContextMenuItems: true,
            docUri,
            key: entry.key,
            group: group.name,
            presetIndex: ui.activePreset,
            configIndex: ui.activeConfig
        };

        const valueInput = el('input', {
            value: staged !== undefined ? staged : entry.value,
            spellcheck: 'false',
            'aria-label': `Value of ${entry.key}`,
            'data-fid': `v:${id}`,
            title: entry.value,
            onclick: (event) => event.stopPropagation(),
            oninput: () => {
                if (valueInput.value === entry.value) {
                    pending.delete(id);
                } else {
                    pending.set(id, valueInput.value);
                }
                updatePendingUi();
            },
            onkeydown: (event) => {
                event.stopPropagation();
                if (event.key === 'Enter') {
                    event.preventDefault();
                    applyPending(id, group.name, entry);
                } else if (event.key === 'Escape') {
                    event.preventDefault();
                    pending.delete(id);
                    valueInput.value = entry.value;
                    updatePendingUi();
                }
            }
        });

        const wrap = el('div', { class: 'value-wrap' + (staged !== undefined ? ' pending' : '') }, [valueInput]);

        const updatePendingUi = () => {
            const isPending = pending.has(id);
            wrap.classList.toggle('pending', isPending);
            confirmBar.style.display = isPending ? 'flex' : 'none';
        };

        const confirmBar = el(
            'div',
            { class: 'confirm', style: staged !== undefined ? '' : 'display:none' },
            [
                icon('warn', true),
                el('span', { class: 'txt' }, [
                    'Not applied yet — press ',
                    el('span', { class: 'kbd', text: 'Enter' }),
                    ' or choose:'
                ]),
                el(
                    'button',
                    {
                        class: 'btn primary sm',
                        onclick: (event) => {
                            event.stopPropagation();
                            applyPending(id, group.name, entry);
                        }
                    },
                    [icon('check', true), 'Apply']
                ),
                el(
                    'button',
                    {
                        class: 'btn sm',
                        text: 'Discard',
                        onclick: (event) => {
                            event.stopPropagation();
                            pending.delete(id);
                            valueInput.value = entry.value;
                            updatePendingUi();
                        }
                    }
                )
            ]
        );

        /** @type {Node} */
        let note;
        if (entry.status === 'introduced') {
            note = el('span', {
                class: 'note',
                text: 'new key',
                title: 'Not defined by any lower-priority layer'
            });
        } else if (entry.status === 'shadowed') {
            note = el('span', {
                class: 'note warn',
                text: `overridden by ${entry.effective?.file ?? '?'}`,
                title: 'A higher-priority file wins, so this value never reaches the compiler.'
            });
        } else if (entry.inherited) {
            note = el(
                'span',
                {
                    class: 'note',
                    title: `Inherited '${entry.inherited.value}' from ${entry.inherited.file} (priority ${entry.inherited.priority})`
                },
                [
                    document.createTextNode(entry.status === 'redundant' ? 'same as' : 'was'),
                    el('span', { class: 'arrow', text: '·' }),
                    document.createTextNode(entry.inherited.value)
                ]
            );
        } else {
            note = el('span', { class: 'note' });
        }

        const tags = el('div', { class: 'tags' }, [
            entry.substitutions?.length
                ? el('span', {
                      class: 'badge warn',
                      text: '@',
                      title:
                          `CMake rewrites this at configure time: ${entry.substitutions.join(', ')}.\n` +
                          'configure_file() runs without @ONLY, so @VAR@ and ${VAR} are both substituted.'
                  })
                : null,
            entry.type ? el('span', { class: 'badge mute', text: entry.type }) : null,
            el('button', {
                class: 'badge mute toggle',
                text: entry.output === 'public' ? 'public' : 'private',
                title:
                    `Emitted into the ${entry.output} header.\n` +
                    `Click to switch to ${entry.output === 'public' ? 'private' : 'public'}.`,
                onclick: (event) => {
                    event.stopPropagation();
                    send({
                        type: 'setMeta',
                        presetIndex: ui.activePreset,
                        configIndex: ui.activeConfig,
                        group: group.name,
                        key: entry.key,
                        patch: { output: entry.output === 'public' ? 'private' : 'public' }
                    });
                }
            })
        ]);

        const body = el('div', { class: 'body' }, [
            el('div', { class: 'top' }, [
                el('span', { class: 'kname' }, [highlight(entry.key, ui.search)]),
                tags
            ]),
            el('div', { class: 'bottom' }, [wrap, note]),
            confirmBar,
            ...(hasProblem
                ? entry.problems.map((problem) =>
                      el('div', { class: 'problem' }, [icon('warn', true), el('span', { text: problem })])
                  )
                : [])
        ]);

        return el(
            'div',
            {
                class: `row s-${entry.status}${hasProblem ? ' has-problem' : ''}${selected ? ' selected' : ''}`,
                tabindex: '0',
                'data-row': id,
                'data-vscode-context': JSON.stringify(context),
                onclick: () => {
                    ui.selected = { group: group.name, key: entry.key };
                    ui.inspectorOpen = true;
                    render();
                },
                ondblclick: () =>
                    send({
                        type: 'revealInText',
                        path: [
                            'presets',
                            ui.activePreset,
                            'config',
                            ui.activeConfig,
                            group.name,
                            entry.key
                        ]
                    })
            },
            [el('span', { class: 'stripe' }), body]
        );
    }

    /**
     * @param {string} id
     * @param {string} group
     * @param {any} entry
     */
    function applyPending(id, group, entry) {
        const value = pending.get(id);
        if (value === undefined) {
            return;
        }
        pending.delete(id);
        send({
            type: 'setValue',
            presetIndex: ui.activePreset,
            configIndex: ui.activeConfig,
            group,
            key: entry.key,
            value
        });
    }

    /**
     * Navigating away never discards staged edits.
     *
     * `window.confirm` is unreliable inside VS Code's sandboxed webview, and a
     * blocking prompt on every tab switch would be worse than the problem. The
     * staging map is keyed by preset/config/group/key, so an edit simply stays
     * put and reappears when the user comes back; a toast makes that visible
     * rather than leaving them wondering where the value went.
     *
     * @returns {boolean} always true — kept as a call site for the reminder
     */
    function confirmDiscardPending() {
        if (pending.size > 0) {
            toast(
                `${pending.size} unapplied change${pending.size === 1 ? '' : 's'} kept — they are still staged where you left them.`
            );
        }
        return true;
    }

    // ------------------------------------------------------------- inherited

    function renderInherited(config) {
        const visible = config.inheritedOnly.filter((entry) => matchesSearch(entry));
        if (visible.length === 0) {
            return document.createDocumentFragment();
        }
        const id = groupId('__inherited');
        const collapsed = ui.collapsed.has(id);

        const head = el(
            'button',
            {
                class: 'group-head',
                'aria-expanded': String(!collapsed),
                onclick: () => {
                    if (collapsed) {
                        ui.collapsed.delete(id);
                    } else {
                        ui.collapsed.add(id);
                    }
                    render();
                }
            },
            [
                el('span', { class: 'chev' }, [icon('chevron', true)]),
                el('span', { class: 'gname', text: 'Inherited, not overridden here' }),
                el('span', { class: 'n', text: String(visible.length) })
            ]
        );

        const rows = el('div', { class: 'rows' });
        for (const entry of visible) {
            rows.appendChild(
                el('div', { class: 'row inherited', tabindex: '0' }, [
                    el('span', { class: 'stripe' }),
                    el('div', { class: 'body' }, [
                        el('div', { class: 'top' }, [
                            el('span', { class: 'kname' }, [highlight(entry.key, ui.search)]),
                            el('div', { class: 'tags' }, [
                                entry.type ? el('span', { class: 'badge mute', text: entry.type }) : null,
                                el('span', { class: 'badge mute', text: entry.output })
                            ])
                        ]),
                        el('div', { class: 'bottom' }, [
                            el('span', { class: 'note', text: entry.value, title: entry.value }),
                            el('span', { class: 'note', text: `from ${entry.file}` }),
                            el('span', { class: 'grow' }),
                            el(
                                'button',
                                {
                                    class: 'btn sm',
                                    title: `Add an override for ${entry.key} to this preset`,
                                    onclick: () =>
                                        send({
                                            type: 'adoptInherited',
                                            presetIndex: ui.activePreset,
                                            configIndex: ui.activeConfig,
                                            group:
                                                entry.kind === 'define'
                                                    ? 'defines'
                                                    : (
                                                          config.groups.find((g) =>
                                                              g.name.startsWith('constexprs')
                                                          ) || { name: 'constexprs' }
                                                      ).name,
                                            key: entry.key,
                                            value: entry.value
                                        })
                                },
                                [icon('add', true), 'Override here']
                            )
                        ])
                    ])
                ])
            );
        }

        return el('div', { class: `group${collapsed ? ' collapsed' : ''}` }, [head, rows]);
    }

    // ------------------------------------------------------------- inspector

    function renderInspector() {
        const aside = el('aside', { class: 'inspector', 'aria-label': 'Key details' });
        if (!ui.selected) {
            return aside;
        }
        const entry = findEntry(ui.selected.group, ui.selected.key);
        if (!entry) {
            return aside;
        }

        aside.appendChild(
            el('div', { class: 'inspector-head' }, [
                el('h3', { text: 'Details' }),
                el('span', { class: 'grow' }),
                el(
                    'button',
                    {
                        class: 'btn ghost icon sm',
                        title: 'Close details',
                        onclick: () => {
                            ui.inspectorOpen = false;
                            render();
                        }
                    },
                    [icon('close', true)]
                )
            ])
        );

        const body = el('div', { class: 'inspector-body' });
        body.appendChild(el('div', { class: 'key-title', text: entry.key }));
        if (entry.desc) {
            body.appendChild(el('div', { class: 'desc', text: entry.desc }));
        }

        const statusText = {
            introduced: 'Introduced here — no lower-priority layer declares it.',
            override: 'Overrides an inherited value.',
            redundant: 'Identical to the inherited value, so this entry changes nothing.',
            shadowed: 'A higher-priority file wins — this value never reaches the compiler.'
        };
        body.appendChild(
            el('div', { class: `banner ${entry.status === 'shadowed' ? '' : 'info'}`, style: 'margin-top:0' }, [
                icon(entry.status === 'shadowed' ? 'warn' : 'info'),
                el('span', { text: statusText[entry.status] ?? '' })
            ])
        );

        const facts = el('dl', { class: 'kv' }, [
            el('dt', { text: 'Value' }),
            el('dd', { text: entry.value }),
            el('dt', { text: 'Kind' }),
            el('dd', { text: entry.kind === 'define' ? '#define' : 'constexpr' }),
            entry.type ? el('dt', { text: 'Type' }) : null,
            entry.type ? el('dd', { text: entry.type }) : null,
            el('dt', { text: 'Header' }),
            el('dd', { text: `tune_*_${entry.output}.h` }),
            el('dt', { text: 'Group' }),
            el('dd', { text: entry.group }),
            entry.namespace ? el('dt', { text: 'Namespace' }) : null,
            entry.namespace ? el('dd', { text: entry.namespace }) : null
        ]);
        body.appendChild(el('div', { class: 'insp-section' }, [el('h4', { text: 'Facts' }), facts]));

        body.appendChild(
            el('div', { class: 'insp-section' }, [
                el('h4', { text: 'Emitted as' }),
                el('div', {
                    class: 'emit',
                    text:
                        entry.kind === 'define'
                            ? `#define ${entry.key} ${entry.value}`
                            : `constexpr ${entry.type ?? 'u32'} ${entry.key} = ${entry.value};`
                })
            ])
        );

        if (entry.inherited || entry.effective) {
            const trace = el('div', { class: 'trace' });
            if (entry.inherited) {
                trace.appendChild(
                    el('div', { class: 'trace-item' }, [
                        el('span', { class: 'num', text: '↓' }),
                        el('div', { class: 'detail' }, [
                            el('div', { class: 'val', text: entry.inherited.value }),
                            el('div', { class: 'file', text: entry.inherited.file }),
                            el('div', { class: 'why', text: `inherited at priority ${entry.inherited.priority}` })
                        ])
                    ])
                );
            }
            trace.appendChild(
                el('div', { class: 'trace-item ' + (entry.effective?.isThisFile ? 'winner' : 'applied') }, [
                    el('span', { class: 'num', text: '→' }),
                    el('div', { class: 'detail' }, [
                        el('div', { class: 'val', text: entry.effective?.value ?? entry.value }),
                        el('div', { class: 'file', text: entry.effective?.file ?? model.fileName }),
                        el('div', {
                            class: 'why',
                            text: entry.effective?.isThisFile
                                ? 'this file wins'
                                : `wins at priority ${entry.effective?.priority}`
                        })
                    ])
                ])
            );
            body.appendChild(
                el('div', { class: 'insp-section' }, [el('h4', { text: 'Resolution' }), trace])
            );
        }

        const act = (label, iconName, message, danger) =>
            el(
                'button',
                {
                    class: `btn${danger ? ' danger' : ''}`,
                    onclick: () => send(message)
                },
                [icon(iconName, true), label]
            );

        body.appendChild(
            el('div', { class: 'insp-section' }, [
                el('h4', { text: 'Actions' }),
                el('div', { class: 'insp-actions' }, [
                    act('Reveal in raw JSON', 'goto', {
                        type: 'revealInText',
                        path: [
                            'presets',
                            ui.activePreset,
                            'config',
                            ui.activeConfig,
                            entry.group,
                            entry.key
                        ]
                    }),
                    act('Resolve across presets', 'info', {
                        type: 'command',
                        command: 'skylakeTuning.ctx.resolveKey',
                        args: [
                            {
                                docUri,
                                key: entry.key,
                                group: entry.group,
                                presetIndex: ui.activePreset,
                                configIndex: ui.activeConfig
                            }
                        ]
                    }),
                    act('Delete this entry', 'trash', {
                        type: 'deleteEntry',
                        presetIndex: ui.activePreset,
                        configIndex: ui.activeConfig,
                        group: entry.group,
                        key: entry.key
                    }, true),
                    el('div', { class: 'note', style: 'margin-top:6px', text: 'Right-click the row for more.' })
                ])
            ])
        );

        aside.appendChild(body);
        return aside;
    }

    // ---------------------------------------------------------- empty states

    function renderNoConfig() {
        return el('div', { class: 'empty' }, [
            icon('target'),
            el('div', { class: 'big', text: 'This preset has no target configs' }),
            el('div', { text: 'A config maps a target_name to the values the generator should emit for it.' }),
            el(
                'button',
                {
                    class: 'btn primary',
                    onclick: () => send({ type: 'addConfig', presetIndex: ui.activePreset })
                },
                [icon('add', true), 'Add target config']
            )
        ]);
    }

    function renderNoMatches() {
        return el('div', { class: 'empty' }, [
            icon('search'),
            el('div', { class: 'big', text: 'Nothing matches' }),
            el('div', {
                text: ui.search
                    ? `No key, value or description contains “${ui.search}” in this target.`
                    : 'No entry has the selected status in this target.'
            }),
            el(
                'button',
                {
                    class: 'btn',
                    text: 'Clear search and filters',
                    onclick: () => {
                        ui.search = '';
                        ui.filters.clear();
                        render();
                    }
                }
            )
        ]);
    }

    function renderParseError() {
        return el('div', { class: 'parse-error' }, [
            el('h2', { text: `Cannot show ${model.fileName}` }),
            el('p', { text: model.parseError ? model.parseError.message : 'Unknown problem.' }),
            el(
                'button',
                { class: 'btn primary', onclick: () => send({ type: 'reopenAsText' }) },
                [icon('text'), 'Reopen as Text']
            )
        ]);
    }

    function renderLegend() {
        const item = (color, label, title, dashed) =>
            el('span', { title }, [
                el('span', {
                    class: 'swatch',
                    style: dashed
                        ? `border:1px dashed ${color};background:transparent`
                        : `background:${color}`
                }),
                label
            ]);
        return el('div', { class: 'legend' }, [
            item('var(--introduced)', 'new', 'Declared here for the first time'),
            item('var(--override)', 'override', 'Replaces an inherited value'),
            item('var(--redundant)', 'redundant', 'Same as inherited — changes nothing', true),
            item('var(--shadowed)', 'shadowed', 'A higher-priority file wins instead'),
            el('span', { class: 'grow' }),
            el('span', {}, [
                el('span', { class: 'kbd', text: '/' }),
                ' search · ',
                el('span', { class: 'kbd', text: '↑↓' }),
                ' move · ',
                el('span', { class: 'kbd', text: 'Enter' }),
                ' apply · ',
                el('span', { class: 'kbd', text: 'Esc' }),
                ' cancel · right-click for actions'
            ])
        ]);
    }

    // ------------------------------------------------------------- controls

    function textField(value, placeholder, commit, fid) {
        const input = el('input', {
            class: 'field',
            value,
            placeholder,
            spellcheck: 'false',
            'data-fid': fid,
            onkeydown: (event) => {
                event.stopPropagation();
                if (event.key === 'Enter') {
                    event.preventDefault();
                    input.blur();
                } else if (event.key === 'Escape') {
                    input.value = value;
                    input.blur();
                }
            },
            onblur: () => {
                if (input.value !== value) {
                    commit(input.value);
                }
            }
        });
        return input;
    }

    function selectField(value, options, commit, fid) {
        const select = el(
            'select',
            { class: 'field', 'data-fid': fid, onchange: () => commit(select.value) },
            options.map((option) => el('option', { value: option, text: option }))
        );
        select.value = value;
        return select;
    }

    // -------------------------------------------------------------- context

    document.addEventListener('contextmenu', (event) => {
        const holder = event.target?.closest ? event.target.closest('[data-vscode-context]') : null;
        if (!holder) {
            return;
        }
        try {
            send({ type: 'context', context: JSON.parse(holder.dataset.vscodeContext) });
        } catch {
            /* malformed payload, nothing to cache */
        }
    });

    // ------------------------------------------------------------- keyboard

    document.addEventListener('keydown', (event) => {
        const target = event.target;
        const typing =
            target && (target.tagName === 'INPUT' || target.tagName === 'SELECT' || target.isContentEditable);

        if ((event.ctrlKey || event.metaKey) && event.key.toLowerCase() === 's') {
            event.preventDefault();
            send({ type: 'save' });
            return;
        }
        if (event.altKey && event.key.toLowerCase() === 'z') {
            event.preventDefault();
            send({ type: event.shiftKey ? 'redo' : 'undo' });
            return;
        }
        if ((event.ctrlKey || event.metaKey) && event.key.toLowerCase() === 'f') {
            event.preventDefault();
            const search = root.querySelector('[data-fid="search"]');
            if (search) {
                search.focus();
                search.select();
            }
            return;
        }

        if (typing) {
            return;
        }

        if (event.key === '/') {
            event.preventDefault();
            root.querySelector('[data-fid="search"]')?.focus();
            return;
        }

        if (event.key === 'ArrowDown' || event.key === 'ArrowUp') {
            const rows = [...root.querySelectorAll('.row[tabindex]')];
            if (rows.length === 0) {
                return;
            }
            const current = rows.indexOf(document.activeElement);
            const next = event.key === 'ArrowDown' ? current + 1 : current - 1;
            const clamped = Math.max(0, Math.min(rows.length - 1, next === -1 ? 0 : next));
            rows[clamped].focus();
            rows[clamped].scrollIntoView({ block: 'nearest' });
            event.preventDefault();
            return;
        }

        if (event.key === 'Enter' && document.activeElement?.classList?.contains('row')) {
            document.activeElement.click();
            event.preventDefault();
        }
    });

    // -------------------------------------------------------------- wire-up

    window.addEventListener('message', (event) => {
        const message = event.data;
        if (!message) {
            return;
        }
        if (message.type === 'model') {
            model = message.model;
            docUri = message.docUri;
            dirty = Boolean(message.dirty);
            canUndo = Boolean(message.canUndo);
            canRedo = Boolean(message.canRedo);
            if (model?.presets && ui.activePreset >= model.presets.length) {
                ui.activePreset = 0;
                ui.activeConfig = 0;
            }
            const preset = model?.presets?.[ui.activePreset];
            if (preset && ui.activeConfig >= preset.configs.length) {
                ui.activeConfig = 0;
            }
            render();
        } else if (message.type === 'applied') {
            toast(message.label ? `Applied: ${message.label}` : 'Change applied', {
                label: 'Undo',
                run: () => send({ type: 'undo' })
            });
        }
    });

    send({ type: 'ready' });
})();
