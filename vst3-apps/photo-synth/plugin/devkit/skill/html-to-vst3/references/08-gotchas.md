# 08 · Gotchas

Every one of these cost real time on the reference port. Read before starting.

## WebView2 (the plugin editor)

| Symptom | Cause and fix |
|---|---|
| Editor is blank / white | WebView2 runtime missing, or the user-data folder is not writable. Set `withUserDataFolder()` to `%APPDATA%\<Vendor>\<Product>\WebView2`. |
| `prompt()` / `confirm()` do nothing | WebView2 suppresses them. Wrap in `try/catch`, supply a default (`"Preset " + timestamp`), and make destructive actions immediate with a toast instead of a confirm. |
| Download links do nothing | A WebView cannot start downloads. Route exports through a native `FileChooser`; for text, read the Blob with `FileReader` and send the string to C++. |
| Dropdown lists are unreadable | Native `<select>` popups take the page's text colour on a system background. Style explicitly: `select option { color: …; background: … }`. |
| Page is stale after a rebuild | The binary data is embedded at compile time — rebuild, and remember the host caches the DLL for the session. |

## Chrome headless (probes, screenshots, PDF)

| Symptom | Cause and fix |
|---|---|
| No stdout at all from `--dump-dom` | In restricted/non-interactive shells the call operator returns nothing. Use `Start-Process -Wait -NoNewWindow -RedirectStandardOutput <file>` and read the file. |
| "Multiple targets are not supported in headless mode" | An argument containing a comma (`--window-size=1400,900`) was split by the shell and the remainder read as a second URL. Pass the whole command line as **one quoted string**, not an array. |
| `--screenshot` fails with "Access is denied" | Earlier headless instances are lingering. Give each run its own `--user-data-dir`, and kill stray `chrome` processes between batches. |
| Canvas is blank in the capture | The page draws on `requestAnimationFrame`. Give it real time: `--virtual-time-budget=4000` plus a `setTimeout` before you assert or capture. |
| PDF has a stray blank page | A full-bleed cover exactly the height of the page. Make it 2–3 mm shorter. |

## PowerShell 5.1

| Symptom | Cause and fix |
|---|---|
| A function silently does nothing | You assigned to `$args` — it is an automatic variable. Rename it. |
| `git commit -m` with a here-string mangles the message | Multi-line strings to native commands are re-parsed. Write the message to a file and use `git commit -F <file>`. |
| Em dashes become `â€"` | The script itself was read as ANSI. Save scripts containing non-ASCII as **UTF-8 with BOM**, and read/write files with `[IO.File]::ReadAllText/WriteAllText` + explicit encoding rather than `Get-Content -Raw` / `Out-File`. |
| Inline `if` inside a string fails | PS 5.1 has no inline `if` expression and no ternary. Use a full `if` statement. |

## Building and installing

| Symptom | Cause and fix |
|---|---|
| Copy fails: "being used by another process" | The DAW has the plugin loaded. Rename the loaded `.vst3` binary to `.old`, copy the new one in, delete the `.old` later. The running session keeps the old image; the next launch picks up the new one. |
| DAW still shows the old version after reinstalling | A host maps a plugin DLL **once per session**. Removing and re-adding the device is not enough — restart the DAW. |
| Two plugins collide in the host | Same `PLUGIN_CODE` + manufacturer code. They generate the VST3 class ID. Change the code when building a variant alongside an existing plugin. |
| MSVC path-length errors | Move the project to a short path (`C:\b\<Project>`). |
| Build is fine but the DAW glitches | You built Debug. Always Release for real use. |

## DSP

| Symptom | Cause and fix |
|---|---|
| A swept filter periodically goes silent | A biquad driven to 0 Hz degenerates. Clamp swept frequencies to ≥ ~20 Hz (the browser clamps internally, which is why the prototype sounded fine). |
| Every filter is too wide or too narrow | `lowpass`/`highpass` take **Q in dB** in WebAudio; the others take it linearly. |
| Reverb is far too loud or too quiet | `ConvolverNode` normalises by default. Apply the same calibrated normalisation, then load with `Normalise::no`. |
| Sound is right but drifts/vibrato are wrong | You moved per-render-quantum work to per-sample. Keep the 128-sample cadence. |
| Clicks when changing a setting | A value is being applied per block. Smooth it, and land graph changes under a short master dip (~60 ms). |
| Crackle after hours of running | A circular buffer index overflowed to negative. Fold indices periodically. |
| The patch resets when the sample rate changes | `prepareToPlay` re-ran and reset the DSP. Restore from atomic shadow copies instead. |

## Process

- Ask for the plugin's **name and identity codes early** if it will live
  alongside an existing plugin; renaming after release breaks projects.
- Keep the untouched prototype in `reference/`. When something sounds wrong,
  the first useful question is "what does the original do here?", and you can
  only answer it if you did not edit the original.
- Splice the bridge with a script, not by hand, so a prototype update is a
  re-run rather than a re-port.
