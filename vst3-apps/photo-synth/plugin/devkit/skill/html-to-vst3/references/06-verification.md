# 06 · Verification

There is no test suite for "does it look and sound like the prototype". Build
one out of three cheap, repeatable checks.

## 1 · Probe the page headlessly

The bridged page still runs in a plain browser, because every bridge call is
wrapped in `try/catch`. Exploit that: load it in headless Chrome with a stub
backend, drive it, and assert on the DOM.

```html
<script>
window.__JUCE__ = { backend: { emitEvent: function (n, p) { window.__sent.push(p); },
                               addEventListener: function () {} } };
window.__sent = []; window.__errs = [];
window.addEventListener("error", function (e) { window.__errs.push(e.message + "@" + e.lineno); });
</script>
```

and at the end of the page a probe that runs after a delay, exercises the UI
(dispatch `PointerEvent`s at a canvas, click buttons, open panels) and writes
its findings into `document.title`, which `--dump-dom` returns.

What to assert, every time:

- `window.__errs` is empty — no JavaScript threw;
- the controls you changed exist and hold the values you expect;
- canvases have non-black pixels where content should be
  (`getImageData` on the centre);
- a simulated drag produces a sane readout and a burst of bridge messages.

This catches nearly every splice mistake in about three seconds, without
building anything.

## 2 · Build, in Release

```powershell
cmake --build <build> --config Release 2>&1 | Select-String -Pattern "error|warning C4|-> C:"
```

Read the warnings in your own files. A change is not finished until this is
clean.

## 3 · Run it and look at it

Launch the standalone build, screenshot the window, and actually look at the
image against the original page:

```powershell
$p = Get-Process "<Product>"
# GetWindowRect + Graphics.CopyFromScreen → PNG
```

Compare: fonts, spacing, colours, the state of toggles, readouts showing
plausible numbers. This is the step that catches "the page loaded but the
resource provider is serving a stale copy" and "the theme is subtly wrong in
WebView2".

Then load it in the DAW and check the things only a host can exercise:
MIDI timing, automation, save/reload of the project, opening and closing the
window repeatedly, and CPU with everything switched on.

## Comparing sound numerically

For deterministic paths, render the same short sequence twice and diff:

- prototype → `OfflineAudioContext` → WAV;
- plugin → its own offline render → WAV.

Peak, RMS and spectral centroid within a fraction of a dB means you got the
gain staging and the filters right. A constant offset points at gain staging;
a brightness difference points at filter Q units or a missing clamp.

## Screenshots for documentation

Automate them, do not take them by hand. One capture page that picks its setup
from `location.hash`, two passes (one `--dump-dom` to measure the element
rectangle, one `--screenshot` for pixels), then crop. Identical setups, 2×
DPI, repeatable when the UI changes. `tools/capture-screenshots.ps1` in this
kit does this; see `08-gotchas.md` for why it drives Chrome the way it does.

## Report honestly

State what you ran and what it produced. If a check was skipped, say so. "Built
clean and the standalone renders correctly; not yet tested in a DAW" is a
useful sentence. "Everything works" is not.
