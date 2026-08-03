/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2025 Balan Narcis (balannarcis96@gmail.com)
 *
 * Minimal position-tracking JSON parser.
 *
 * `JSON.parse` throws away offsets, but every navigation feature in this
 * extension ("reveal in text editor", diagnostics ranges, surgical edits)
 * needs to map a logical path back to a byte range in the document. So we
 * parse once and keep an AST alongside the plain value.
 *
 * Deliberately strict JSON (no comments, no trailing commas): the Python
 * generator uses `json.load`, and accepting more than it does would let the
 * editor write files CMake then refuses.
 */
'use strict';

/**
 * @typedef {Object} AstNode
 * @property {'object'|'array'|'string'|'number'|'boolean'|'null'} type
 * @property {number} offset         Start offset of the node.
 * @property {number} end            End offset (exclusive).
 * @property {*} value               Materialized JS value.
 * @property {AstProperty[]} [properties] For objects.
 * @property {AstNode[]} [items]        For arrays.
 */

/**
 * @typedef {Object} AstProperty
 * @property {string} key
 * @property {number} keyOffset      Offset of the opening quote of the key.
 * @property {number} keyEnd         Offset just past the closing quote.
 * @property {AstNode} value
 * @property {number} offset         Start of the whole `"key": value` pair.
 * @property {number} end            End of the whole pair.
 */

class JsonParseError extends Error {
    constructor(message, offset) {
        super(message);
        this.name = 'JsonParseError';
        this.offset = offset;
    }
}

const WHITESPACE = new Set([' ', '\t', '\n', '\r']);

class Parser {
    /** @param {string} text */
    constructor(text) {
        this.text = text;
        this.pos = 0;
    }

    error(message) {
        throw new JsonParseError(message, this.pos);
    }

    skipWhitespace() {
        while (this.pos < this.text.length && WHITESPACE.has(this.text[this.pos])) {
            this.pos++;
        }
    }

    expect(ch) {
        if (this.text[this.pos] !== ch) {
            this.error(`Expected '${ch}' but found '${this.text[this.pos] ?? '<eof>'}'`);
        }
        this.pos++;
    }

    /** @returns {AstNode} */
    parse() {
        this.skipWhitespace();
        const node = this.parseValue();
        this.skipWhitespace();
        if (this.pos !== this.text.length) {
            this.error('Trailing content after top-level value');
        }
        return node;
    }

    /** @returns {AstNode} */
    parseValue() {
        this.skipWhitespace();
        const ch = this.text[this.pos];
        switch (ch) {
            case '{':
                return this.parseObject();
            case '[':
                return this.parseArray();
            case '"':
                return this.parseString();
            case 't':
            case 'f':
                return this.parseBoolean();
            case 'n':
                return this.parseNull();
            default:
                if (ch === '-' || (ch >= '0' && ch <= '9')) {
                    return this.parseNumber();
                }
                return this.error(`Unexpected character '${ch ?? '<eof>'}'`);
        }
    }

    /** @returns {AstNode} */
    parseObject() {
        const offset = this.pos;
        this.expect('{');
        /** @type {AstProperty[]} */
        const properties = [];
        const value = {};

        this.skipWhitespace();
        if (this.text[this.pos] === '}') {
            this.pos++;
            return { type: 'object', offset, end: this.pos, value, properties };
        }

        for (;;) {
            this.skipWhitespace();
            const propOffset = this.pos;
            if (this.text[this.pos] !== '"') {
                this.error('Expected a string key');
            }
            const keyOffset = this.pos;
            const keyNode = this.parseString();
            const keyEnd = this.pos;
            const key = /** @type {string} */ (keyNode.value);

            this.skipWhitespace();
            this.expect(':');
            const valueNode = this.parseValue();

            properties.push({
                key,
                keyOffset,
                keyEnd,
                value: valueNode,
                offset: propOffset,
                end: valueNode.end
            });
            // Last writer wins, matching Python's `json.load` behaviour on
            // duplicate keys. `duplicateKeys()` surfaces them as diagnostics.
            value[key] = valueNode.value;

            this.skipWhitespace();
            if (this.text[this.pos] === ',') {
                this.pos++;
                continue;
            }
            if (this.text[this.pos] === '}') {
                this.pos++;
                break;
            }
            this.error(`Expected ',' or '}' in object`);
        }

        return { type: 'object', offset, end: this.pos, value, properties };
    }

    /** @returns {AstNode} */
    parseArray() {
        const offset = this.pos;
        this.expect('[');
        /** @type {AstNode[]} */
        const items = [];
        const value = [];

        this.skipWhitespace();
        if (this.text[this.pos] === ']') {
            this.pos++;
            return { type: 'array', offset, end: this.pos, value, items };
        }

        for (;;) {
            const item = this.parseValue();
            items.push(item);
            value.push(item.value);

            this.skipWhitespace();
            if (this.text[this.pos] === ',') {
                this.pos++;
                continue;
            }
            if (this.text[this.pos] === ']') {
                this.pos++;
                break;
            }
            this.error(`Expected ',' or ']' in array`);
        }

        return { type: 'array', offset, end: this.pos, value, items };
    }

    /** @returns {AstNode} */
    parseString() {
        const offset = this.pos;
        this.expect('"');
        let out = '';
        for (;;) {
            const ch = this.text[this.pos];
            if (ch === undefined) {
                this.error('Unterminated string');
            }
            if (ch === '"') {
                this.pos++;
                break;
            }
            if (ch === '\\') {
                this.pos++;
                const esc = this.text[this.pos];
                switch (esc) {
                    case '"': out += '"'; break;
                    case '\\': out += '\\'; break;
                    case '/': out += '/'; break;
                    case 'b': out += '\b'; break;
                    case 'f': out += '\f'; break;
                    case 'n': out += '\n'; break;
                    case 'r': out += '\r'; break;
                    case 't': out += '\t'; break;
                    case 'u': {
                        const hex = this.text.substr(this.pos + 1, 4);
                        if (!/^[0-9a-fA-F]{4}$/.test(hex)) {
                            this.error('Invalid \\u escape');
                        }
                        out += String.fromCharCode(parseInt(hex, 16));
                        this.pos += 4;
                        break;
                    }
                    default:
                        this.error(`Invalid escape '\\${esc ?? '<eof>'}'`);
                }
                this.pos++;
                continue;
            }
            out += ch;
            this.pos++;
        }
        return { type: 'string', offset, end: this.pos, value: out };
    }

    /** @returns {AstNode} */
    parseNumber() {
        const offset = this.pos;
        const match = /^-?(0|[1-9]\d*)(\.\d+)?([eE][+-]?\d+)?/.exec(this.text.slice(this.pos));
        if (!match) {
            this.error('Invalid number');
        }
        this.pos += match[0].length;
        return { type: 'number', offset, end: this.pos, value: Number(match[0]) };
    }

    /** @returns {AstNode} */
    parseBoolean() {
        const offset = this.pos;
        if (this.text.startsWith('true', this.pos)) {
            this.pos += 4;
            return { type: 'boolean', offset, end: this.pos, value: true };
        }
        if (this.text.startsWith('false', this.pos)) {
            this.pos += 5;
            return { type: 'boolean', offset, end: this.pos, value: false };
        }
        return this.error('Invalid literal');
    }

    /** @returns {AstNode} */
    parseNull() {
        const offset = this.pos;
        if (!this.text.startsWith('null', this.pos)) {
            this.error('Invalid literal');
        }
        this.pos += 4;
        return { type: 'null', offset, end: this.pos, value: null };
    }
}

/**
 * Parse `text` into a position-tracking AST.
 * @param {string} text
 * @returns {AstNode}
 * @throws {JsonParseError}
 */
function parseTree(text) {
    return new Parser(text).parse();
}

/**
 * Walk an AST down a path of object keys / array indices.
 * @param {AstNode|undefined} node
 * @param {(string|number)[]} path
 * @returns {AstNode|undefined}
 */
function nodeAtPath(node, path) {
    let current = node;
    for (const segment of path) {
        if (!current) {
            return undefined;
        }
        if (typeof segment === 'number') {
            if (current.type !== 'array' || !current.items) {
                return undefined;
            }
            current = current.items[segment];
        } else {
            if (current.type !== 'object' || !current.properties) {
                return undefined;
            }
            // Match the last occurrence so it agrees with last-writer-wins.
            let found;
            for (const prop of current.properties) {
                if (prop.key === segment) {
                    found = prop.value;
                }
            }
            current = found;
        }
    }
    return current;
}

/**
 * Like {@link nodeAtPath} but returns the `"key": value` property rather than
 * just the value, so callers can highlight the key itself.
 * @param {AstNode|undefined} root
 * @param {(string|number)[]} path
 * @returns {AstProperty|undefined}
 */
function propertyAtPath(root, path) {
    if (path.length === 0) {
        return undefined;
    }
    const parent = nodeAtPath(root, path.slice(0, -1));
    const last = path[path.length - 1];
    if (!parent || parent.type !== 'object' || !parent.properties || typeof last !== 'string') {
        return undefined;
    }
    let found;
    for (const prop of parent.properties) {
        if (prop.key === last) {
            found = prop;
        }
    }
    return found;
}

/**
 * Collect duplicate keys within every object in the tree.
 * @param {AstNode} root
 * @returns {{path: (string|number)[], key: string, property: AstProperty}[]}
 */
function duplicateKeys(root) {
    const out = [];
    /** @param {AstNode} node @param {(string|number)[]} path */
    const visit = (node, path) => {
        if (node.type === 'object' && node.properties) {
            const seen = new Set();
            for (const prop of node.properties) {
                if (seen.has(prop.key)) {
                    out.push({ path: path.concat(prop.key), key: prop.key, property: prop });
                }
                seen.add(prop.key);
                visit(prop.value, path.concat(prop.key));
            }
        } else if (node.type === 'array' && node.items) {
            node.items.forEach((item, i) => visit(item, path.concat(i)));
        }
    };
    visit(root, []);
    return out;
}

module.exports = { parseTree, nodeAtPath, propertyAtPath, duplicateKeys, JsonParseError };
