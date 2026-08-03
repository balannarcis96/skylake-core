/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2025 Balan Narcis (balannarcis96@gmail.com)
 *
 * Minimal DOM so the webview front end can be rendered and driven from Node.
 *
 * Syntax checks prove the file parses; they say nothing about whether render()
 * survives a real model. This implements just enough of the DOM for editor.js
 * to build its tree, so the render path and the click handlers can be exercised
 * against actual preset data.
 */
'use strict';

class ClassList {
    /** @param {Element} owner */
    constructor(owner) {
        this.owner = owner;
    }

    _set() {
        return new Set(String(this.owner.className || '').split(/\s+/).filter(Boolean));
    }

    _write(set) {
        this.owner.className = [...set].join(' ');
    }

    add(...names) {
        const set = this._set();
        names.forEach((name) => set.add(name));
        this._write(set);
    }

    remove(...names) {
        const set = this._set();
        names.forEach((name) => set.delete(name));
        this._write(set);
    }

    contains(name) {
        return this._set().has(name);
    }

    toggle(name, force) {
        const set = this._set();
        const next = force === undefined ? !set.has(name) : force;
        if (next) {
            set.add(name);
        } else {
            set.delete(name);
        }
        this._write(set);
        return next;
    }
}

class Element {
    /** @param {string} tag */
    constructor(tag) {
        this.tagName = String(tag).toUpperCase();
        this.children = [];
        this.parentNode = null;
        this.attributes = new Map();
        this.listeners = new Map();
        this.className = '';
        this._text = '';
        this.dataset = {};
        this.classList = new ClassList(this);
        this.style = new Proxy({}, { get: (t, k) => t[k] ?? '', set: (t, k, v) => ((t[k] = v), true) });
        this.value = '';
        this.disabled = false;
        this.isContentEditable = false;
        this.selectionStart = 0;
        this.ownerDocument = null;
    }

    get textContent() {
        if (this.children.length === 0) {
            return this._text;
        }
        return this.children.map((child) => child.textContent).join('');
    }

    set textContent(value) {
        this._text = String(value ?? '');
        this.children = [];
    }

    setAttribute(name, value) {
        this.attributes.set(name, String(value));
        if (name.startsWith('data-')) {
            const key = name
                .slice(5)
                .replace(/-([a-z])/g, (_, ch) => ch.toUpperCase());
            this.dataset[key] = String(value);
        }
        if (name === 'value') {
            this.value = String(value);
        }
        if (name === 'disabled') {
            this.disabled = true;
        }
        if (name === 'class') {
            this.className = String(value);
        }
    }

    getAttribute(name) {
        return this.attributes.has(name) ? this.attributes.get(name) : null;
    }

    hasAttribute(name) {
        return this.attributes.has(name);
    }

    appendChild(node) {
        node.parentNode = this;
        this.children.push(node);
        return node;
    }

    removeChild(node) {
        const at = this.children.indexOf(node);
        if (at >= 0) {
            this.children.splice(at, 1);
            node.parentNode = null;
        }
        return node;
    }

    remove() {
        if (this.parentNode) {
            this.parentNode.removeChild(this);
        }
    }

    addEventListener(type, handler) {
        const bucket = this.listeners.get(type) ?? [];
        bucket.push(handler);
        this.listeners.set(type, bucket);
    }

    removeEventListener(type, handler) {
        const bucket = this.listeners.get(type) ?? [];
        const at = bucket.indexOf(handler);
        if (at >= 0) {
            bucket.splice(at, 1);
        }
    }

    /**
     * Fire a handler on this node, then bubble to ancestors unless stopped.
     * @param {string} type
     * @param {Record<string, any>} [init]
     */
    dispatch(type, init = {}) {
        let stopped = false;
        const event = {
            type,
            target: init.target ?? this,
            currentTarget: this,
            preventDefault() {},
            stopPropagation() {
                stopped = true;
            },
            ...init
        };
        let node = this;
        while (node) {
            for (const handler of node.listeners.get(type) ?? []) {
                event.currentTarget = node;
                handler(event);
            }
            if (stopped) {
                break;
            }
            node = node.parentNode;
        }
        return event;
    }

    click() {
        return this.dispatch('click');
    }

    focus() {
        const doc = rootDocument;
        if (doc) {
            doc.activeElement = this;
        }
    }

    blur() {
        const doc = rootDocument;
        if (doc && doc.activeElement === this) {
            doc.activeElement = doc.body;
        }
        this.dispatch('blur');
    }

    select() {}

    setSelectionRange() {}

    scrollIntoView() {}

    get scrollTop() {
        return this._scrollTop ?? 0;
    }

    set scrollTop(value) {
        this._scrollTop = value;
    }

    /** Depth-first descendants including self. */
    *walk() {
        yield this;
        for (const child of this.children) {
            if (child.walk) {
                yield* child.walk();
            }
        }
    }

    matches(selector) {
        return matchesSelector(this, selector);
    }

    closest(selector) {
        let node = this;
        while (node) {
            if (node.matches && node.matches(selector)) {
                return node;
            }
            node = node.parentNode;
        }
        return null;
    }

    querySelector(selector) {
        for (const node of this.walk()) {
            if (node !== this && node.matches(selector)) {
                return node;
            }
        }
        return null;
    }

    querySelectorAll(selector) {
        const out = [];
        for (const node of this.walk()) {
            if (node !== this && node.matches(selector)) {
                out.push(node);
            }
        }
        return out;
    }
}

class TextNode {
    constructor(text) {
        this._text = String(text ?? '');
        this.parentNode = null;
        this.children = [];
    }

    get textContent() {
        return this._text;
    }

    *walk() {
        yield this;
    }

    matches() {
        return false;
    }
}

/**
 * Supports the selector forms editor.js and the tests actually use:
 *   .class    [attr="value"]    tag.class    .a.b    .row[tabindex]
 *   plus descendant combinators: `.value-wrap input`
 */
function matchesSelector(node, selector) {
    if (!node.tagName) {
        return false;
    }
    for (const alternative of selector.split(',').map((entry) => entry.trim())) {
        if (matchesDescendantChain(node, alternative)) {
            return true;
        }
    }
    return false;
}

/**
 * Match right-to-left: the last compound must match the node, and each earlier
 * compound must match some ancestor, in order.
 * @param {Element} node
 * @param {string} selector
 */
function matchesDescendantChain(node, selector) {
    const parts = selector.split(/\s+/).filter(Boolean);
    if (parts.length === 0) {
        return false;
    }
    if (!matchesSingle(node, parts[parts.length - 1])) {
        return false;
    }
    let ancestor = node.parentNode;
    for (let i = parts.length - 2; i >= 0; i--) {
        let found = false;
        while (ancestor) {
            if (ancestor.tagName && matchesSingle(ancestor, parts[i])) {
                found = true;
                ancestor = ancestor.parentNode;
                break;
            }
            ancestor = ancestor.parentNode;
        }
        if (!found) {
            return false;
        }
    }
    return true;
}

function matchesSingle(node, selector) {
    let rest = selector.trim();
    const classes = new Set(String(node.className || '').split(/\s+/).filter(Boolean));

    const tagMatch = /^([a-zA-Z]+)/.exec(rest);
    if (tagMatch) {
        if (node.tagName !== tagMatch[1].toUpperCase()) {
            return false;
        }
        rest = rest.slice(tagMatch[1].length);
    }

    for (;;) {
        if (rest.startsWith('.')) {
            const match = /^\.([A-Za-z0-9_-]+)/.exec(rest);
            if (!match || !classes.has(match[1])) {
                return false;
            }
            rest = rest.slice(match[0].length);
            continue;
        }
        if (rest.startsWith('[')) {
            const match = /^\[([A-Za-z0-9_-]+)(?:=["']?([^\]"']*)["']?)?\]/.exec(rest);
            if (!match) {
                return false;
            }
            const actual = node.getAttribute(match[1]);
            if (actual === null) {
                return false;
            }
            if (match[2] !== undefined && actual !== match[2]) {
                return false;
            }
            rest = rest.slice(match[0].length);
            continue;
        }
        break;
    }
    return rest.trim() === '';
}

/** @type {any} */
let rootDocument = null;

/**
 * Install globals and return handles for driving the UI.
 * @param {(message: any) => void} onMessage  Receives every webview -> host post.
 */
function install(onMessage) {
    const document = {
        createElement: (tag) => new Element(tag),
        createElementNS: (_ns, tag) => new Element(tag),
        createTextNode: (text) => new TextNode(text),
        createDocumentFragment: () => new Element('#fragment'),
        listeners: new Map(),
        addEventListener(type, handler) {
            const bucket = this.listeners.get(type) ?? [];
            bucket.push(handler);
            this.listeners.set(type, bucket);
        },
        dispatch(type, init) {
            for (const handler of this.listeners.get(type) ?? []) {
                handler({ type, preventDefault() {}, stopPropagation() {}, ...init });
            }
        },
        querySelector(selector) {
            return this.body.querySelector(selector);
        },
        querySelectorAll(selector) {
            return this.body.querySelectorAll(selector);
        },
        getElementById(id) {
            return this._byId.get(id) ?? null;
        },
        _byId: new Map(),
        body: new Element('body'),
        activeElement: null
    };
    document.activeElement = document.body;
    rootDocument = document;

    const root = new Element('div');
    root.setAttribute('id', 'root');
    document._byId.set('root', root);
    document.body.appendChild(root);

    /** @type {any[]} */
    const posted = [];
    let state = {};

    const windowListeners = new Map();
    const globalWindow = {
        addEventListener(type, handler) {
            const bucket = windowListeners.get(type) ?? [];
            bucket.push(handler);
            windowListeners.set(type, bucket);
        },
        dispatch(type, event) {
            for (const handler of windowListeners.get(type) ?? []) {
                handler(event);
            }
        }
    };

    global.document = document;
    global.window = globalWindow;
    global.setTimeout = (fn) => {
        void fn;
        return 0;
    };
    global.acquireVsCodeApi = () => ({
        postMessage(message) {
            posted.push(message);
            if (onMessage) {
                onMessage(message);
            }
        },
        getState: () => state,
        setState(next) {
            state = next;
        }
    });

    return {
        document,
        window: globalWindow,
        root,
        posted,
        /** Deliver a host -> webview message. */
        post(message) {
            globalWindow.dispatch('message', { data: message });
        },
        /** All rendered key rows. */
        rows() {
            return root.querySelectorAll('.row');
        },
        text() {
            return root.textContent;
        }
    };
}

module.exports = { install, Element, TextNode };
