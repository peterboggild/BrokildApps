# Licensing

*Short version: the plug-ins are **AGPLv3**, the parts that do not link JUCE are
also offered under **MIT**, and the artwork is not licensed for reuse.*

---

## The plug-ins — AGPLv3

Every VST3 in this repository is built with the
[JUCE framework](https://juce.com), version 8. JUCE 8 is dual-licensed: you may
use it under the **AGPLv3**, or under a paid commercial licence from its
authors. These plug-ins are built under the AGPLv3 option, so the AGPLv3 is what
they carry. The full text is in [`LICENSE`](LICENSE).

In practice, for anyone who wants to use this work:

- **Playing the plug-ins costs nothing and obliges nothing.** Download, install,
  use them in your own music, release that music commercially. The licence is
  about the software, not about what you make with it.
- **You may read, modify and build any of it.** That is the point of putting the
  source here.
- **If you distribute a modified build, publish your changes** under the same
  licence. That is the whole of the obligation, and it is why AGPLv3 rather
  than a permissive licence: the free JUCE option requires it.
- **If you would rather not be bound by that**, the alternative is a commercial
  JUCE licence from the JUCE authors, under which you would need your own
  arrangement here too. Ask.

`Copyright (C) 2026 Peter Bøggild.` Each plug-in's source carries the same.

## The parts that link no JUCE — MIT as well

Some of this repository has nothing to do with JUCE and is not encumbered by
it. Those parts are offered under the **MIT licence** as well as the AGPLv3, so
they can be reused freely, including in closed software:

| part | what it is |
|---|---|
| `BrokildWorldFX/src/`, `modules/`, `test/` | the world-rack DSP library, plain C++ with no framework |
| `BrokildWorldFX/ui/bwfx-rack.js` | the rack's browser panel |
| every plug-in's `test/bench.cpp` and friends | the offline benches, plain C++ |
| `tools/` | the site's own checkers and builders |
| `music-apps/`, `health-apps/` | the browser applications |
| `dsw/` | the Digital Science Workstation host and its plug-in ABI |

`BrokildWorldFX/adapter/` is the exception inside BWFX: those headers include
JUCE, so they are AGPLv3 like the plug-ins.

To use a dual-licensed part under MIT, take it on these terms:

```
Copyright (c) 2026 Peter Bøggild

Permission is hereby granted, free of charge, to any person obtaining a copy of
this software and associated documentation files (the "Software"), to deal in
the Software without restriction, including without limitation the rights to
use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
the Software, and to permit persons to whom the Software is furnished to do so,
subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
```

## The artwork is not licensed for reuse

Panel decals, knob and fader art, wordmarks, textures, preview images and manual
plates are **all rights reserved**. They live under each plug-in's
`assets/decals/` and `img/` folders, and under `assets/` at the repository root.

They are in the repository because a panel cannot be rebuilt without them, not
because they are being given away. Build the plug-ins, change them, publish your
changes. Do not lift the artwork into something else.

## What this repository does not own

- **JUCE** is licensed by its own authors under the terms above, and is not
  vendored here. Each plug-in's `CMakeLists.txt` points at a checkout you supply.
- **The VST3 SDK** arrives through JUCE and carries its own MIT licence.
- **The MIT KEMAR HRTF set** used by Thin Walls is the MIT Media Lab's, used
  under its own terms, and is credited in that plug-in's documentation.
- **Delivered decal art** was generated to order and is covered by the artwork
  paragraph above.
- **Anything a plug-in is named after** is a reference, not a claim. Battlestar
  Overdrive is published with the blessing of the artist it is named for. The
  rest are affectionate nods to books, films and records, and no endorsement by
  anyone is implied.

## Releases

Every downloadable archive carries a `README.txt` naming this licence and
linking to the source folder it was built from, which is how the AGPLv3's
requirement to offer the corresponding source is met.
