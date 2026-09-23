# 00 · Setting up a Windows machine for VST3 development

One-time. About 30–60 minutes, most of it downloading. Everything here is
free, and none of it requires a developer account.

## What you need

| Component | Version used for this kit | Notes |
|---|---|---|
| Visual Studio Community | 2026 (18.9) | **"Desktop development with C++"** workload — that is the only workload required. 2022 (17.x) works too; the CMake generator string changes. |
| CMake | 4.4.2 | 3.22+ is enough. Add it to PATH during install. |
| JUCE | 8.0.13 | The framework. Source only — no installer, no licence key for GPL/personal use. |
| WebView2 Runtime | 151.x | Already present on current Windows 10/11. Only needed if the editor is blank. |
| Git | any recent | To clone JUCE and to publish. |

VST3 does not need Steinberg's SDK separately: JUCE ships the VST3 headers it
needs.

## Install

1. **Visual Studio Community** — installer → *Workloads* → tick **Desktop
   development with C++** → Install. Everything else can be unticked.
2. **CMake** — from `cmake.org/download` (Windows x64 installer). Choose
   *Add CMake to the system PATH*.
3. **JUCE** — clone it somewhere permanent, outside your project:
   ```powershell
   git clone --depth 1 --branch 8.0.13 https://github.com/juce-framework/JUCE.git C:\AudioDev\JUCE
   ```
   You will point every project's `CMakeLists.txt` at this path.

## Verify the toolchain before writing any plugin code

```powershell
cmake --version                       # expect 3.22+
& "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe" -latest -property displayName
```

Then build something. Copy `template/` from this kit to a scratch folder, set
`JUCE_DIR` in its `CMakeLists.txt`, and run the two build commands below. If a
`.vst3` appears, the machine is ready.

## The build commands

Always build **out of tree** and in **Release** (Debug plugins are slow enough
to glitch in a DAW):

```powershell
cmake -S C:\path\to\Project -B C:\path\to\Project\build -G "Visual Studio 18 2026" -A x64
cmake --build C:\path\to\Project\build --config Release
```

Generator strings: `"Visual Studio 18 2026"`, `"Visual Studio 17 2022"`. Get it
wrong and CMake tells you which generators it knows.

Artefacts land in:
```
build\<Target>_artefacts\Release\VST3\<Product>.vst3\   ← the plugin bundle (a folder)
build\<Target>_artefacts\Release\Standalone\<Product>.exe
```

## Installing a plugin for testing

A VST3 "file" is really a folder. Copy the **whole bundle**:

```
C:\Program Files\Common Files\VST3\<YourCompany>\<Product>.vst3\
```

The vendor subfolder is optional but keeps things tidy — hosts scan
recursively. Then rescan plugins in the DAW.

**If the DAW is running it holds the DLL open** and the copy fails. See
`08-gotchas.md` for the rename-and-replace trick; and remember that a host
maps a plugin DLL once per session, so the DAW must be fully restarted before
it sees a new build.

## Choosing plugin identifiers

In `juce_add_plugin`:

- `PLUGIN_CODE` — 4 characters, unique per plugin.
- `PLUGIN_MANUFACTURER_CODE` — 4 characters, your vendor.
- `PRODUCT_NAME` — what the user sees.

Together these generate the VST3 class ID. **If two plugins share them the host
cannot tell them apart**, and projects saved with one will load the other. If
you are building a second version alongside an existing one, change
`PLUGIN_CODE` (e.g. `Psyn` → `Psy2`) and the product name.

## Recommended layout

```
C:\AudioDev\JUCE\                 one shared JUCE clone
C:\b\<Project>\                   short path — long paths break MSVC builds
  reference\<prototype>\          untouched copy of the web app
  Source\
  build\                          out of tree, gitignored
  dist\                           release zips
```

Keep the project path short. Deeply nested paths (`Dropbox\...\Documents\...`)
hit MSVC's path length limit during JUCE builds.
