# 07 · Packaging and documenting the release

## The zip

A VST3 is a folder, so the archive must preserve it as one:

```
<Product>-VST3-win64.zip
  <Product>.vst3\Contents\x86_64-win\<Product>.vst3   ← the binary
  <Product>.vst3\Contents\Resources\moduleinfo.json   ← host scan manifest
  <Product>-Manual.pdf
  README.txt
```

`README.txt` says, in this order: what it is, how to install (unzip → copy the
**folder** into `C:\Program Files\Common Files\VST3\` → rescan), the system
requirements including the WebView2 note, and what is in the box.

`tools/package-release.ps1` builds this.

## The manual

Write it as HTML with a print stylesheet and render it with headless Chrome
(`--print-to-pdf`, `--no-pdf-header-footer`). This gives you a manual that is
diffable, versionable, and regenerates in seconds when the UI changes.

A structure that works:

1. **Cover** — dark, full bleed, the instrument's own palette, one hero shot.
2. **What it is** — the concept in three paragraphs and one table.
3. **Quick start** — five numbered steps, no more.
4. **Reference** — one section per panel, tables of control → what it does.
5. **Try this** — six to ten recipes: what to set, and what you will hear.
   This is the section people actually read; write it last, when you know the
   instrument.
6. **In the host** — automation, state, and the honest limitations.

Design notes: cream paper with a dark cover reads better than an all-dark
manual and prints sanely; screenshots pop against it. Set
`print-color-adjust: exact`, give figures `break-inside: avoid`, and start
sections with `break-before: page`. Use `@page { size: A4; margin: … }` and
`@page :first { margin: 0 }` for a full-bleed cover; make the cover a few
millimetres *shorter* than the page or rounding adds a blank sheet.

## The parity document

Ship `docs/feature-parity.md` with three honest headings:

- **Identical** — what was ported exactly, with enough detail that a sceptical
  reader believes you.
- **Adapted** — what changed shape because a plugin is not a browser (native
  save dialogs, host automation, an approximated compressor).
- **Omitted / limited** — camera capture, anything needing the editor open.

Write it as you go, not at the end. It is also the changelog you will want when
you return to the project in six months.

## Versioning and identity

- Bump `project(<Name> VERSION x.y.z)` and the plugin version together.
- Never change `PLUGIN_CODE` after release — projects reference it.
- Keep the product name stable; users find the plugin by that string.

## Publishing

If you publish on GitHub Pages, put the zip and the PDF next to a small landing
page: what it is, three screenshots, a download button, numbered install steps,
and the requirements table. People arriving from a link need the install steps
more than they need the feature list.
