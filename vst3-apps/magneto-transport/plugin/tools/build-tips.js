#!/usr/bin/env node
// Magneto Transport - builds the tooltip cards (BUGLIST 6).
//
//   node tools/build-tips.js            build Source/ui/tips.json from docs/tips/tips.src.json
//   node tools/build-tips.js --mathml "<tex>"   print one equation's MathML (for checking)
//
// The source is the same shape as tips.json, with equations written in a TeX subset
// between $...$ (inline) or $$...$$ (display). They are converted to MathML Core here,
// so WebView2 renders them natively and the source stays readable.
//
// Checked, and the build FAILS (exit 1, naming the card) on:
//   - a card id required by docs/PROTOCOL.md or a parameter in Source/Params.h with no card;
//   - TeX this converter does not understand (it never emits broken markup);
//   - a <figure data-fig> name not in PROTOCOL.md's figure list;
//   - a <span data-num> key not in PROTOCOL.md's number-key list;
//   - HTML that is not well formed, or uses a tag outside the small allowed set.
// Warned about: a measured-looking number typed into a card (it should be a data-num
// key, which the bench regenerates every build, so a card can never go stale).
//
// No dependencies. If Source/ui/tips-numbers.json does not exist yet, a stub with every
// key (text "—") is written so the page can load; the bench writes the real one.

'use strict';
const fs = require('fs');
const path = require('path');

const ROOT   = path.resolve(__dirname, '..');
const SRC    = path.join(ROOT, 'docs', 'tips', 'tips.src.json');
const OUT    = path.join(ROOT, 'Source', 'ui', 'tips.json');
const NUMS   = path.join(ROOT, 'Source', 'ui', 'tips-numbers.json');
const PROTO  = path.join(ROOT, 'docs', 'PROTOCOL.md');
const PARAMS = path.join(ROOT, 'Source', 'Params.h');

// Controls on the panel that are not (yet) rows in Params.h but need a card.
const EXTRA_IDS = ['pattern', 'mech'];

class TipError extends Error {}
function fail(msg) { throw new TipError(msg); }

// ------------------------------------------------------------------ the contract

function between(text, startMarker, endMarker) {
    const a = text.indexOf(startMarker);
    if (a < 0) fail(`PROTOCOL.md: marker not found: "${startMarker}"`);
    const b = endMarker ? text.indexOf(endMarker, a + startMarker.length) : -1;
    return text.slice(a + startMarker.length, b < 0 ? undefined : b);
}
function backticks(text) {
    const out = [];
    const re = /`([^`]+)`/g;
    let m;
    while ((m = re.exec(text))) out.push(m[1].trim());
    return out;
}

function readContract() {
    const proto = fs.readFileSync(PROTO, 'utf8').replace(/\r\n/g, '\n');
    const cardText = between(proto, 'Card ids:', '### `tips-numbers.json`');
    const partIds = backticks(cardText).filter(t => /^[a-z]+:[a-z0-9]+$/.test(t));
    const figText = between(proto, 'Figure names', '### Number keys');
    const figs = backticks(figText).filter(t => /^[a-z0-9]+$/.test(t));
    const numText = between(proto, '### Number keys', '## Settings');
    const nums = backticks(numText).filter(t => /^(ref|eng)\.[a-z0-9_]+$/.test(t));
    if (!partIds.length) fail('PROTOCOL.md: no part/readout/settings card ids found');
    if (!figs.length) fail('PROTOCOL.md: no figure names found');
    if (!nums.length) fail('PROTOCOL.md: no number keys found');

    const params = fs.readFileSync(PARAMS, 'utf8');
    const specs = params.slice(params.indexOf('SPECS[]'));
    const paramIds = [];
    const re = /\{\s*"([a-z0-9]+)",\s*"[^"]*",\s*Kind::/g;
    let m;
    while ((m = re.exec(specs))) paramIds.push(m[1]);
    if (!paramIds.length) fail('Params.h: no parameter ids found in SPECS[]');

    const required = [...new Set([...paramIds, ...EXTRA_IDS, ...partIds])];
    return { required, paramIds, partIds, figs: new Set(figs), nums: new Set(nums), numList: nums };
}

// ------------------------------------------------------------------ TeX -> MathML Core

function esc(t) {
    return t.replace(/&/g, '&amp;').replace(/</g, '&lt;').replace(/>/g, '&gt;').replace(/"/g, '&quot;');
}

const GREEK_LOWER = {
    alpha: 'α', beta: 'β', gamma: 'γ', delta: 'δ', epsilon: 'ϵ', varepsilon: 'ε', zeta: 'ζ',
    eta: 'η', theta: 'θ', vartheta: 'ϑ', iota: 'ι', kappa: 'κ', lambda: 'λ', mu: 'μ', nu: 'ν',
    xi: 'ξ', pi: 'π', rho: 'ρ', varrho: 'ϱ', sigma: 'σ', tau: 'τ', upsilon: 'υ', phi: 'ϕ',
    varphi: 'φ', chi: 'χ', psi: 'ψ', omega: 'ω',
};
const GREEK_UPPER = {
    Gamma: 'Γ', Delta: 'Δ', Theta: 'Θ', Lambda: 'Λ', Xi: 'Ξ', Pi: 'Π', Sigma: 'Σ',
    Upsilon: 'Υ', Phi: 'Φ', Psi: 'Ψ', Omega: 'Ω',
};
const OPERATORS = {   // \name -> <mo>
    cdot: '⋅', times: '×', div: '÷', approx: '≈', le: '≤', leq: '≤', ge: '≥', geq: '≥',
    propto: '∝', pm: '±', mp: '∓', to: '→', rightarrow: '→', leftarrow: '←', ll: '≪', gg: '≫',
    ne: '≠', neq: '≠', equiv: '≡', sim: '∼', simeq: '≃', ldots: '…', cdots: '⋯', mid: '∣',
    circ: '∘', prime: '′', int: '∫', iint: '∬', oint: '∮', sum: '∑', prod: '∏', in: '∈',
    langle: '⟨', rangle: '⟩', lvert: '|', rvert: '|', vert: '|', Vert: '‖',
};
const IDENTIFIERS = { infty: '∞', partial: '∂', nabla: '∇', hbar: 'ℏ', ell: 'ℓ' };
const FUNCTIONS = new Set(['exp', 'ln', 'log', 'lg', 'sin', 'cos', 'tan', 'sinh', 'cosh', 'tanh',
    'arcsin', 'arccos', 'arctan', 'max', 'min', 'lim', 'det', 'sgn']);
const ACCENTS = { hat: 'ˆ', bar: '¯', dot: '˙', ddot: '¨', tilde: '˜', vec: '→' };
const SPACES = { ',': '0.1667em', ':': '0.2222em', ';': '0.2778em', ' ': '0.25em',
    quad: '1em', qquad: '2em' };
const DELIMS = { '(': '(', ')': ')', '[': '[', ']': ']', '|': '|', '.': '',
    '\\{': '{', '\\}': '}', '\\|': '‖', '\\langle': '⟨', '\\rangle': '⟩',
    '\\lvert': '|', '\\rvert': '|', '\\lbrace': '{', '\\rbrace': '}' };
const FUNC_APPLY = '<mo>&#x2061;</mo>';

function texToMathML(src, display) {
    const s = src;
    let i = 0;
    const err = (msg) => fail(`TeX "${src}": ${msg} (at ${i})`);
    const ws = () => { while (i < s.length && /\s/.test(s[i])) i++; };
    const row = (a) => a.length === 1 ? a[0] : '<mrow>' + a.join('') + '</mrow>';

    function readCmd() {          // positioned at the backslash
        i++;
        if (i >= s.length) err('lone backslash');
        if (/[a-zA-Z]/.test(s[i])) {
            let j = i;
            while (j < s.length && /[a-zA-Z]/.test(s[j])) j++;
            const n = s.slice(i, j);
            i = j;
            return n;
        }
        return s[i++];
    }
    function rawGroup() {         // {...} as raw text, braces balanced
        ws();
        if (s[i] !== '{') err('expected {');
        let depth = 0, j = i;
        for (; j < s.length; j++) {
            if (s[j] === '{') depth++;
            else if (s[j] === '}') { depth--; if (depth === 0) break; }
        }
        if (depth !== 0) err('unbalanced { in raw group');
        const t = s.slice(i + 1, j);
        i = j + 1;
        return t;
    }
    function atRight() {
        return s.startsWith('\\right', i) && !/[a-zA-Z]/.test(s[i + 6] || '');
    }
    function parseSeq(stop) {
        const out = [];
        for (;;) {
            ws();
            if (i >= s.length) {
                if (stop === '}') err('missing }');
                if (stop === 'right') err('\\left without \\right');
                if (stop === ']') err('missing ]');
                break;
            }
            if (s[i] === '}') { if (stop === '}') { i++; break; } err('unexpected }'); }
            if (stop === ']' && s[i] === ']') { i++; break; }
            if (atRight()) { if (stop === 'right') break; err('\\right without \\left'); }
            const a = parseScripted();
            if (a !== '') out.push(a);
        }
        return out;
    }
    function parseArg() {
        ws();
        if (s[i] === '{') { i++; const a = parseSeq('}'); if (!a.length) err('empty argument'); return row(a); }
        if (i >= s.length) err('missing argument');
        const a = parseAtom();
        if (a === '') err('empty argument');
        return a;
    }
    function delimiter() {
        ws();
        let d;
        if (s[i] === '\\') { const j = i; const c = readCmd(); d = '\\' + c; if (!(d in DELIMS)) { i = j; err('bad delimiter ' + d); } }
        else d = s[i++];
        if (!(d in DELIMS)) err('bad delimiter ' + d);
        return DELIMS[d];
    }
    function parseScripted() {
        const base = parseAtom();
        if (base === '') return '';
        let sub = null, sup = null;
        for (;;) {
            ws();
            if (s[i] === '^' && sup === null) { i++; sup = parseArg(); }
            else if (s[i] === '_' && sub === null) { i++; sub = parseArg(); }
            else if (s[i] === "'") {
                let p = '';
                while (s[i] === "'") { p += '′'; i++; }
                const pr = `<mo>${p}</mo>`;
                sup = sup === null ? pr : `<mrow>${pr}${sup}</mrow>`;
            }
            else if (s[i] === '^' || s[i] === '_') err('double script');
            else break;
        }
        if (sub !== null && sup !== null) return `<msubsup>${base}${sub}${sup}</msubsup>`;
        if (sub !== null) return `<msub>${base}${sub}</msub>`;
        if (sup !== null) return `<msup>${base}${sup}</msup>`;
        return base;
    }
    function mathrm(t) {
        if (/[\\{}$^_]/.test(t)) err('\\mathrm content must be plain text: ' + t);
        const parts = [];
        const re = /([A-Za-zµΩÅ°]+)|(\d+(?:\.\d+)?)|(\s+)|(.)/g;
        let m;
        while ((m = re.exec(t))) {
            if (m[1]) parts.push(m[1].length === 1 ? `<mi mathvariant="normal">${esc(m[1])}</mi>` : `<mi>${esc(m[1])}</mi>`);
            else if (m[2]) parts.push(`<mn>${m[2]}</mn>`);
            else if (m[3]) parts.push('<mspace width="0.25em"></mspace>');
            else if (m[4] === '/') parts.push('<mo lspace="0" rspace="0">/</mo>');
            else parts.push(`<mo>${esc(m[4] === '-' ? '−' : m[4])}</mo>`);
        }
        if (!parts.length) err('empty \\mathrm');
        return row(parts);
    }
    function command() {
        const name = readCmd();
        if (name in SPACES) return `<mspace width="${SPACES[name]}"></mspace>`;
        if (name === '!') return '';
        if (name === '{') return '<mo>{</mo>';
        if (name === '}') return '<mo>}</mo>';
        if (name === '|') return '<mo>‖</mo>';
        if (name === '%') return '<mo>%</mo>';
        if (name in GREEK_LOWER) return `<mi>${GREEK_LOWER[name]}</mi>`;
        if (name in GREEK_UPPER) return `<mi mathvariant="normal">${GREEK_UPPER[name]}</mi>`;
        if (name in IDENTIFIERS) return `<mi>${IDENTIFIERS[name]}</mi>`;
        if (name in OPERATORS) {
            const big = /^(int|iint|oint|sum|prod)$/.test(name);
            return big ? `<mo largeop="true">${OPERATORS[name]}</mo>` : `<mo>${OPERATORS[name]}</mo>`;
        }
        if (FUNCTIONS.has(name)) return `<mi>${name}</mi>${FUNC_APPLY}`;
        if (name === 'operatorname') {
            const t = rawGroup();
            if (!/^[A-Za-z]+$/.test(t)) err('\\operatorname needs letters: ' + t);
            return `<mi>${t}</mi>${FUNC_APPLY}`;
        }
        if (name === 'frac' || name === 'dfrac' || name === 'tfrac') {
            const a = parseArg(), b = parseArg();
            return `<mfrac>${a}${b}</mfrac>`;
        }
        if (name === 'sqrt') {
            ws();
            if (s[i] === '[') { i++; const n = parseSeq(']'); const a = parseArg(); return `<mroot>${a}${row(n)}</mroot>`; }
            return `<msqrt>${parseArg()}</msqrt>`;
        }
        if (name in ACCENTS) {
            const a = parseArg();
            return `<mover accent="true">${a}<mo stretchy="false">${ACCENTS[name]}</mo></mover>`;
        }
        if (name === 'mathrm') return mathrm(rawGroup());
        if (name === 'text') {
            const t = rawGroup();
            if (/[\\{}$]/.test(t)) err('\\text content must be plain text: ' + t);
            return `<mtext>${esc(t).replace(/^ | $/g, ' ')}</mtext>`;
        }
        if (name === 'left') {
            const open = delimiter();
            const inner = parseSeq('right');
            i += 6;   // \right
            const close = delimiter();
            const o = open ? `<mo fence="true" form="prefix">${esc(open)}</mo>` : '';
            const c = close ? `<mo fence="true" form="postfix">${esc(close)}</mo>` : '';
            return `<mrow>${o}${inner.join('')}${c}</mrow>`;
        }
        err('unknown command \\' + name);
    }
    function parseAtom() {
        ws();
        const c = s[i];
        if (c === undefined) err('unexpected end');
        if (c === '{') { i++; const a = parseSeq('}'); return '<mrow>' + a.join('') + '</mrow>'; }
        if (c === '\\') return command();
        if (/[0-9]/.test(c) || (c === '.' && /[0-9]/.test(s[i + 1] || ''))) {
            let j = i;
            while (j < s.length && /[0-9]/.test(s[j])) j++;
            if (s[j] === '.' && /[0-9]/.test(s[j + 1] || '')) { j++; while (j < s.length && /[0-9]/.test(s[j])) j++; }
            const n = s.slice(i, j);
            i = j;
            return `<mn>${n}</mn>`;
        }
        if (/[a-zA-Z]/.test(c)) { i++; return `<mi>${c}</mi>`; }
        if (c === '(' || c === '[') { i++; return '<mo form="prefix" stretchy="false">' + c + '</mo>'; }
        if (c === ')' || c === ']') { i++; return '<mo form="postfix" stretchy="false">' + c + '</mo>'; }
        if (c === '/') { i++; return '<mo lspace="0" rspace="0">/</mo>'; }
        if ('+=<>()[]/,|!:;*.?'.includes(c)) {
            i++;
            return `<mo>${esc(c)}</mo>`;
        }
        if (c === '-') { i++; return '<mo>−</mo>'; }
        if ('−×·±≈≤≥→∝'.includes(c)) { i++; return `<mo>${c}</mo>`; }
        if ('µμαβγδεζηθλνξπρστφχψωΩΦΔ∞∂'.includes(c)) { i++; return `<mi>${c}</mi>`; }
        if (c === '}') err('unexpected }');
        err(`character "${c}" not supported`);
    }

    const seq = parseSeq(null);
    if (!seq.length) err('empty equation');
    return `<math${display ? ' display="block"' : ''}>${row(seq)}</math>`;
}

// ------------------------------------------------------------------ HTML checks

const HTML_TAGS = new Set(['p', 'em', 'strong', 'b', 'i', 'sub', 'sup', 'br', 'figure', 'span',
    'ul', 'ol', 'li', 'code', 'small']);
const MATH_TAGS = new Set(['math', 'mrow', 'mi', 'mn', 'mo', 'mtext', 'mspace', 'mfrac', 'msqrt',
    'mroot', 'msub', 'msup', 'msubsup', 'mover', 'munder', 'munderover']);
const VOID = new Set(['br']);

function checkHtml(html, where) {
    if (/<(?![a-zA-Z\/!])/.test(html)) fail(`${where}: a bare "<" in text (write &lt;)`);
    const amp = html.match(/&(?!(?:[a-zA-Z]+|#\d+|#x[0-9a-fA-F]+);)/);
    if (amp) fail(`${where}: a bare "&" in text (write &amp;)`);
    const stack = [];
    const re = /<(\/?)([a-zA-Z][a-zA-Z0-9-]*)((?:\s+[a-zA-Z-]+(?:\s*=\s*(?:"[^"]*"|'[^']*'))?)*)\s*(\/?)>/g;
    let m, last = 0;
    while ((m = re.exec(html))) {
        const between = html.slice(last, m.index);
        if (/[<>]/.test(between)) fail(`${where}: malformed tag near "${between.slice(-30)}"`);
        last = re.lastIndex;
        const [, close, tag, , self] = m;
        const t = tag.toLowerCase();
        if (!HTML_TAGS.has(t) && !MATH_TAGS.has(t)) fail(`${where}: tag <${t}> is not allowed`);
        if (close) {
            const top = stack.pop();
            if (top !== t) fail(`${where}: </${t}> closes <${top || 'nothing'}>`);
        } else if (!self && !VOID.has(t)) stack.push(t);
    }
    if (/[<>]/.test(html.slice(last))) fail(`${where}: malformed tag at the end`);
    if (stack.length) fail(`${where}: unclosed <${stack.join('>, <')}>`);
}

// A measured result typed as text goes stale: warn on decimal values with result units.
const MEASURED = /[−+-]?\d+\.\d+\s*(?:dBA|dBFS|dB|%|cents|µs|us)(?![a-zA-Z])/g;

function convertField(html, where, stats) {
    if (typeof html !== 'string') fail(`${where}: not a string`);
    let out = html.replace(/\$\$([\s\S]+?)\$\$/g, (_, tex) => {
        stats.equations++;
        try { return texToMathML(tex.trim(), true); } catch (e) { fail(`${where}: ${e.message}`); }
    });
    out = out.replace(/\$([^$]+?)\$/g, (_, tex) => {
        stats.equations++;
        try { return texToMathML(tex.trim(), false); } catch (e) { fail(`${where}: ${e.message}`); }
    });
    if (out.includes('$')) fail(`${where}: an unmatched $`);
    checkHtml(out, where);
    return out;
}

// ------------------------------------------------------------------ build

function build() {
    const C = readContract();
    let src;
    try { src = JSON.parse(fs.readFileSync(SRC, 'utf8')); }
    catch (e) { fail(`${path.relative(ROOT, SRC)}: ${e.message}`); }
    if (!src || typeof src.cards !== 'object') fail('tips.src.json: no "cards" object');

    const missing = C.required.filter(id => !(id in src.cards));
    if (missing.length) fail('cards missing for: ' + missing.join(', '));

    const stats = { equations: 0 };
    const figsUsed = new Set(), numsUsed = new Set(), warnings = [];
    const cards = {};
    for (const [id, card] of Object.entries(src.cards)) {
        if (!C.required.includes(id)) warnings.push(`card "${id}" is not a required id (kept)`);
        for (const k of ['title', 'does', 'hood', 'notModelled'])
            if (typeof card[k] !== 'string') fail(`card "${id}": field "${k}" missing or not a string`);
        if (!card.title.trim()) fail(`card "${id}": empty title`);
        if (!card.does.trim()) fail(`card "${id}": empty "does"`);
        const out = { title: card.title };
        for (const k of ['does', 'hood', 'notModelled']) {
            const where = `card "${id}" ${k}`;
            const html = convertField(card[k], where, stats);
            let m;
            const fre = /data-fig\s*=\s*["']([^"']*)["']/g;
            while ((m = fre.exec(html))) {
                if (!C.figs.has(m[1])) fail(`${where}: figure "${m[1]}" is not in PROTOCOL.md's list`);
                figsUsed.add(m[1]);
            }
            const nre = /data-num\s*=\s*["']([^"']*)["']/g;
            while ((m = nre.exec(html))) {
                if (!C.nums.has(m[1])) fail(`${where}: number key "${m[1]}" is not in PROTOCOL.md's list`);
                numsUsed.add(m[1]);
            }
            const prose = html.replace(/<math[\s\S]*?<\/math>/g, ' ').replace(/<span data-num[^>]*><\/span>/g, ' ')
                              .replace(/<[^>]+>/g, ' ');
            const typed = prose.match(MEASURED);
            if (typed) warnings.push(`${where}: typed value(s) ${typed.map(t => '"' + t + '"').join(', ')} - a measured number should be a data-num key`);
            out[k] = html;
        }
        // hood length, as a guide for the author
        const words = out.hood.replace(/<math[\s\S]*?<\/math>/g, ' x ').replace(/<[^>]+>/g, ' ').split(/\s+/).filter(Boolean).length;
        if (out.hood && (words < 40 || words > 220)) warnings.push(`card "${id}": hood is ${words} words`);
        cards[id] = out;
    }

    fs.mkdirSync(path.dirname(OUT), { recursive: true });
    fs.writeFileSync(OUT, JSON.stringify({ version: 1, cards }, null, 1) + '\n', 'utf8');

    let stub = false;
    if (!fs.existsSync(NUMS)) {
        const numbers = {};
        for (const k of C.numList) numbers[k] = { value: null, unit: '', text: '—', stub: true };
        fs.writeFileSync(NUMS, JSON.stringify({ build: 'stub', numbers }, null, 1) + '\n', 'utf8');
        stub = true;
    }

    for (const w of warnings) console.warn('warning: ' + w);
    const unusedFigs = [...C.figs].filter(f => !figsUsed.has(f));
    console.log(`tips.json: ${Object.keys(cards).length} cards (${C.required.length} required), `
        + `${stats.equations} equations, ${figsUsed.size}/${C.figs.size} figures, `
        + `${numsUsed.size}/${C.nums.size} number keys`);
    if (unusedFigs.length) console.log('figures not used: ' + unusedFigs.join(', '));
    const unusedNums = C.numList.filter(k => !numsUsed.has(k));
    if (unusedNums.length) console.log('number keys not used: ' + unusedNums.join(', '));
    if (stub) console.log('wrote a stub ' + path.relative(ROOT, NUMS));
    console.log(`${warnings.length} warning(s)`);
}

try {
    const a = process.argv.indexOf('--mathml');
    if (a > 0) console.log(texToMathML(process.argv[a + 1] || '', process.argv.includes('--block')));
    else build();
} catch (e) {
    if (e instanceof TipError) { console.error('build-tips: ' + e.message); process.exit(1); }
    throw e;
}
