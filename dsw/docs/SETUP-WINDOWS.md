# Setting up a C++ toolchain on Windows (for DSW and VS Code)

Everything DSW needs on a Windows PC, from a blank machine to pressing
**Build** in VS Code. Total time ≈ 20 minutes, mostly download.

You need three things: a **compiler**, **CMake**, and **VS Code with two
extensions**. Two routes to the compiler — pick ONE:

| Route | Compiler | Choose it when |
|-------|----------|----------------|
| **A. Visual Studio Build Tools** (recommended) | MSVC (`cl.exe`) | You want the standard Windows toolchain — the same one the BrokildApps CI uses. |
| **B. MSYS2 / MinGW-w64** | GCC (`g++`) | You prefer the GNU toolchain you may know from Linux. |

---

## Route A — Visual Studio Build Tools (MSVC)

1. Download **Build Tools for Visual Studio** from
   <https://visualstudio.microsoft.com/downloads/> (scroll to
   *Tools for Visual Studio → Build Tools*). This is the compiler
   **without** the Visual Studio IDE — you'll use VS Code instead.
2. In the installer, tick the workload **“Desktop development with C++”**.
   The defaults inside it are right: MSVC v143 (or newer), the Windows 11
   SDK, and **C++ CMake tools for Windows** (which brings CMake and Ninja,
   so you can skip step “CMake” below).
3. Install (~7 GB), no reboot needed.

> MSVC is only usable from a *developer* environment. VS Code's CMake Tools
> extension sets that up automatically when you pick the MSVC kit — you never
> need the "x64 Native Tools Command Prompt" unless you like terminals.

## Route B — MSYS2 / MinGW-w64 (GCC)

1. Install MSYS2 from <https://www.msys2.org/> (one installer, defaults fine).
2. Open the **“MSYS2 UCRT64”** shell from the Start menu and run:
   ```sh
   pacman -S --needed mingw-w64-ucrt-x86_64-gcc \
                      mingw-w64-ucrt-x86_64-cmake \
                      mingw-w64-ucrt-x86_64-ninja
   ```
3. Add `C:\msys64\ucrt64\bin` to your user **PATH**
   (Start → “edit environment variables”), so VS Code and CMake can find
   `g++.exe`. Log out/in or restart VS Code after changing PATH.

## CMake (route A users usually already have it)

If `cmake --version` doesn't work in a fresh terminal, install CMake from
<https://cmake.org/download/> (Windows x64 installer) and tick
**“Add CMake to the system PATH”** during install.

## Git

If `git --version` doesn't work, install from <https://git-scm.com/download/win>
(defaults fine), then:

```sh
git clone https://github.com/peterboggild/BrokildApps.git
```

## VS Code

1. Install VS Code from <https://code.visualstudio.com/>.
2. Install two extensions (Ctrl+Shift+X, search by name):
   - **C/C++** (Microsoft) — IntelliSense, debugging.
   - **CMake Tools** (Microsoft) — configure/build/run from the status bar.
3. Open the folder `BrokildApps/dsw` (File → Open Folder).
4. The first time, CMake Tools asks for a **kit** — pick
   *“Visual Studio Build Tools … x64”* (route A) or the *GCC …
   ucrt64* kit (route B). If it doesn't ask: Ctrl+Shift+P →
   **CMake: Select a Kit**.
5. Status bar (bottom): set variant to **Release**, press **Build**,
   then **▶** (Launch) — or Ctrl+Shift+P → **CMake: Run Without Debugging**.
6. Open <http://127.0.0.1:8090/> in a browser. That's DSW running.

## Checking it worked

In a fresh terminal (**not** one that was open during the installs):

```sh
cmake --version        # ≥ 3.15
git --version
```

plus `cl` in a “Developer Command Prompt for VS” (route A) or `g++ --version`
anywhere (route B).

## Notes for DSW specifically

- **OpenMP** (used by the example plugins for the field loops) is included
  with both MSVC and MinGW GCC — nothing extra to install. CMake enables it
  automatically when found.
- The DSW networking code carries Winsock and `LoadLibrary` paths for
  Windows, but the development host so far has been Linux — **the Windows
  build is honest beta**. If it fails, the error and a fix are welcome; the
  code paths are in `src/net.cpp` and `src/host.cpp` behind `#ifdef _WIN32`.
- Windows Defender Firewall may ask to allow `dsw.exe` network access on
  first run. DSW binds **localhost only**; allowing it on private networks
  is fine, and denying it entirely also works for local use.

## The same setup serves the VST3 plugins

This toolchain (route A) is exactly what builds the audio plugins elsewhere
in this repository — for those you additionally need the **JUCE** checkout
and the WebView2 NuGet package, which their own CMake fetches; see
`vst3-apps/clone-wars/plugin/CMakeLists.txt` and the CI workflow for the
reference invocation.
