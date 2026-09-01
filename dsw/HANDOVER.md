# DSW — handover note (developer-kit docs task, in progress)

For the next Claude session (any environment) or human continuing this.
Written 2026-08-25 by the cloud session, on branch **`claude/dsw-devkit-9jkd55`**
(pushed; continue there, then merge to `main`). The repo-wide coordination
rules are in `vst3-apps/clone-wars/HANDOVER.md` — fetch before editing, own
branch, merge through `main`, never force-push. The Clone Wars OPERATOR
section covers Clone Wars only; DSW is unclaimed territory.

## The task (Peter's words, condensed)

1. Update DSW on GitHub to the newest version — **done before this task
   started**: `dsw/` on `main` is the newest code; nothing was stale.
2. Developer kit: build & run instructions — mostly existed in
   `dsw/README.md`; Windows/VS Code toolchain setup was missing.
3. Make sure instructions for **creating plugins** are there.
4. Add explanations for **installing LAMMPS on a PC**.
5. (added mid-task) Instructions for **installing the compilers etc. for
   VS Code** (C++ toolchain on Windows).

## Done on this branch

- **`docs/SETUP-WINDOWS.md`** — full toolchain guide: MSVC Build Tools
  route and MSYS2/GCC route, CMake, Git, VS Code + C/C++ & CMake Tools
  extensions, kit selection, build/run of DSW, OpenMP notes, the honest
  "Windows build is beta" caveat, and a pointer that the same toolchain
  builds the VST3s.
- **`plugins/heat-blob/`** — a new third example plugin, written to be THE
  tutorial: ~130-line heavily commented `src/plugin.cpp` exercising every
  ABI function (state, `{t:"set"}` control, `{t:"poke"}` interaction,
  telemetry via `poll_message`, RGBA render), a ~100-line `ui/index.html`
  using `/dex.js` (slider, pause, clear, click-to-poke, stats readout),
  `dex.json`, registered in `CMakeLists.txt`.
- **Verified**: full DSW build clean on Linux; the host serves
  `/api/plugins` listing heat-blob. NOT yet browser-tested (see below).

## Remaining work (in order)

1. **`docs/CREATING-A-PLUGIN.md`** — the walkthrough Peter asked for.
   Structure that fits the house voice: (a) what a bundle is (point at the
   README anatomy), (b) "build the tutorial plugin" — walk `heat-blob`'s
   `plugin.cpp` top to bottom explaining each ABI function against
   `include/dex_plugin.h`'s contracts (threading: host serializes per
   instance, no locks needed; memory: returned pointers valid until next
   call), (c) the UI side (`/dex.js`, message vocabulary is yours),
   (d) "make it yours": copy the folder, rename id in `dex.json` +
   `plugin.cpp` + `dsw_add_plugin(...)`, build, refresh launcher,
   (e) porting an existing JS prototype (README already has the 4-step
   version — link, don't duplicate). Keep `plugin.cpp` and the doc in
   lockstep — the code comments already anticipate the doc.
2. **Browser-test heat-blob** (the cloud session has Playwright +
   chromium at `/opt/pw-browsers/chromium`; on a PC just open it):
   launch `./dsw`, open the card, check frames draw, slider/pause/clear/
   poke work, stats tick. Screenshot → `docs/heat-blob.png`, add to the
   README examples table alongside gray-scott and wave-tank.
3. **`docs/INSTALL-LAMMPS.md`** — LAMMPS on a PC. Suggested shape
   (verify commands against https://docs.lammps.org/Install.html before
   publishing; the cloud sandbox could not reach lammps.org):
   - Easiest, Windows: the official pre-built Windows installer packages
     (https://packages.lammps.org/windows.html) — one .exe, includes
     MPI-capable binaries and the examples; note admin-vs-user install.
   - Cross-platform, science-stack-friendly: `conda install -c conda-forge lammps`
     (gives the `lmp` binary AND the Python module; pairs well if Peter
     wants LAMMPS driven from Python or, later, from a DEX core).
   - WSL2 route: `sudo apt install lammps` inside Ubuntu — the Linux
     experience on the PC.
   - From source (when a custom package like GRANULAR/REAXFF is needed):
     git clone, `cmake -B build -D PKG_...=on cmake/`, build; on Windows
     this uses the same MSVC toolchain as docs/SETUP-WINDOWS.md.
   - Smoke test: run `lmp -in in.lj` from the bench examples; where the
     examples land per route.
   - A closing note connecting it to DSW: LAMMPS as a library
     (`liblammps` + C API) is exactly the shape a future `lammps-dex`
     plugin core wants — the ABI's advance/render maps onto
     `lammps_command("run 100")` + position readback.
4. **Link the three docs from `dsw/README.md`** (a short "Developer kit"
   section near Build & run) and consider linking SETUP-WINDOWS from the
   Portability section. Optionally surface them on `dsw/index.html`.
5. **Merge to `main`** (docs + one plugin; no CI watches `dsw/`, so a
   local build check is the gate), push, confirm Pages picks it up.
6. Update this file (or delete it) when done.

## Facts the next session should not re-derive

- The plugin ABI is `include/dex_plugin.h` (7 functions, ABI v1); JSON
  helpers in `include/dex_msg.h`. The host serializes all calls for one
  instance on one worker thread — plugins need no locks.
- `dsw_add_plugin(<id>)` in `dsw/CMakeLists.txt` builds
  `plugins/<id>/src/plugin.cpp` straight into the bundle folder.
- Build: `cmake -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build -j`
  in `dsw/`, run `./dsw`, open http://127.0.0.1:8090/. `dsw/build/` and
  built `.so` files are gitignored.
- Replacing a plugin binary needs a host restart; new bundles and UI
  files just need a launcher refresh.
- The Windows code paths (Winsock, LoadLibrary) exist but are untested —
  first Windows build reports are wanted, per SETUP-WINDOWS.md.
- `dsw/DEX-INTEGRATION.md` is the paste-ready guide for adding DSW to the
  separate DEX site (dex-2dphys.github.io) — untouched by this task.
