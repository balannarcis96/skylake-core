/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2025 Balan Narcis (balannarcis96@gmail.com)
 *
 * Style-preserving JSON writer.
 *
 * Every mutation rewrites the whole document, so the writer has to reproduce
 * the incoming formatting byte-for-byte in untouched regions, otherwise a
 * one-value change lands in review as a whole-file diff. Measured against the
 * m2-server and skylake-core preset trees, `JSON.stringify(v, null, 4)` is
 * already byte-identical except for two things, both of which we detect per
 * file and reproduce here:
 *
 *   1. empty containers, written as `{\n<indent>}` in some files and `{}` in
 *      others,
 *   2. presence or absence of a trailing newline.
 */
'use strict';

/**
 * @typedef {Object} JsonStyle
 * @property {string} indent          One indentation level, e.g. four spaces.
 * @property {boolean} expandEmpty    Write `{\n<indent>}` instead of `{}`.
 * @property {boolean} trailingNewline
 * @property {string} eol
 */

/** @type {JsonStyle} */
const DEFAULT_STYLE = {
    indent: '    ',
    expandEmpty: false,
    trailingNewline: false,
    eol: '\n'
};

/**
 * Infer the formatting conventions of an existing document.
 * @param {string} text
 * @returns {JsonStyle}
 */
function detectStyle(text) {
    const eol = /\r\n/.test(text) ? '\r\n' : '\n';

    // First indented line wins; tabs are honoured if that is what the file uses.
    let indent = DEFAULT_STYLE.indent;
    const indentMatch = /\n([ \t]+)\S/.exec(text);
    if (indentMatch) {
        indent = indentMatch[1];
    }

    // `{` or `[` followed only by whitespace before the matching close.
    const expandEmpty = /[{[][ \t]*\r?\n[ \t]*[}\]]/.test(text);

    return {
        indent,
        expandEmpty,
        trailingNewline: /\r?\n$/.test(text),
        eol
    };
}

/**
 * Escape a string using the same rules as Python's `json.dumps(..., ensure_ascii=False)`,
 * which is what produced these files originally.
 * @param {string} value
 * @returns {string}
 */
function quote(value) {
    let out = '"';
    for (const ch of value) {
        switch (ch) {
            case '"': out += '\\"'; break;
            case '\\': out += '\\\\'; break;
            case '\b': out += '\\b'; break;
            case '\f': out += '\\f'; break;
            case '\n': out += '\\n'; break;
            case '\r': out += '\\r'; break;
            case '\t': out += '\\t'; break;
            default: {
                const code = ch.codePointAt(0) ?? 0;
                if (code < 0x20) {
                    out += '\\u' + code.toString(16).padStart(4, '0');
                } else {
                    out += ch;
                }
            }
        }
    }
    return out + '"';
}

/**
 * Serialize `value` using `style`.
 * @param {*} value
 * @param {JsonStyle} [style]
 * @returns {string}
 */
function stringify(value, style = DEFAULT_STYLE) {
    const { indent, expandEmpty, eol } = style;

    /**
     * @param {*} node
     * @param {number} depth
     * @returns {string}
     */
    const write = (node, depth) => {
        if (node === null || node === undefined) {
            return 'null';
        }
        if (typeof node === 'string') {
            return quote(node);
        }
        if (typeof node === 'number') {
            return Number.isFinite(node) ? String(node) : 'null';
        }
        if (typeof node === 'boolean') {
            return node ? 'true' : 'false';
        }

        const pad = indent.repeat(depth + 1);
        const closePad = indent.repeat(depth);

        if (Array.isArray(node)) {
            if (node.length === 0) {
                return expandEmpty ? `[${eol}${closePad}]` : '[]';
            }
            const items = node.map((item) => pad + write(item, depth + 1));
            return `[${eol}${items.join(',' + eol)}${eol}${closePad}]`;
        }

        const keys = Object.keys(node);
        if (keys.length === 0) {
            return expandEmpty ? `{${eol}${closePad}}` : '{}';
        }
        const props = keys.map((key) => `${pad}${quote(key)}: ${write(node[key], depth + 1)}`);
        return `{${eol}${props.join(',' + eol)}${eol}${closePad}}`;
    };

    return write(value, 0) + (style.trailingNewline ? eol : '');
}

module.exports = { detectStyle, stringify, quote, DEFAULT_STYLE };
