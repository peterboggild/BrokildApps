# Sleeper Agent — iOS port notes

Working document for turning the existing web application at
`health-apps/sleep-noise/` into an iOS/iPadOS app suitable for TestFlight, the
App Store, and paid sale.

> **Coming back to this cold?** Start at
> [`ios-apps/sleeper-agent/START_HERE.md`](../../ios-apps/sleeper-agent/START_HERE.md)
> instead. This file explains *why* the app is built the way it is; that one
> explains what state everything is in and what to do next.

Written before any port work began, so that the baseline is recorded rather
than remembered. Updated as the port proceeds.

- **Branch:** `ios-sleeper-agent`
- **Baseline commit:** `96b04bb` (the five-fader mixer, PR #26)
- **Date started:** 2026-09-09

---

## 1. Existing architecture

One file. `index.html` is 74.8 KB and 1811 lines: 190 lines of CSS, 1424 lines
of JavaScript, and the markup between them. There is no build step, no
framework, no bundler, no dependency of any kind.

```
health-apps/sleep-noise/
    index.html               the whole application
    icon.svg                 411 bytes
    manifest.webmanifest     PWA manifest (installable to home screen)
    app.json                 BrokildApps catalogue entry
```

### External dependencies: none

Audited by grep for `http(s)://`, `fetch(`, `XMLHttpRequest`, `WebSocket`,
`@import`, `@font-face`, `<link`, `<script src`, `importScripts`,
`sendBeacon`, `gtag`, `analytics`.

The only matches are three same-directory `<link>` tags (`manifest.webmanifest`,
`icon.svg` twice). **The application makes no network requests at all.** No
CDN, no web fonts (system font stack only), no analytics, no telemetry, no
cookies, no service worker, no remote configuration. Every sound is synthesised
in the browser from `Math.random()`.

This is the single most valuable fact for the port: the offline requirement and
the privacy story are already satisfied by the existing design, not something
that has to be retrofitted.

### How the sound is made

Everything is rendered once into looping `AudioBuffer`s, deliberately, because
once the screen is off the page may be frozen and anything driven from
`setTimeout` would stall while the audio clock carried on.

- **Noise layers** (`genRaw` → `makeBuffer`): 20-second stereo loops, the two
  channels generated independently for width, with the last second cross-faded
  into the first so the loop point is inaudible. Normalised to 0.18 RMS with a
  0.9 peak ceiling.
  - `brown` — integrated white, trimmed at 26 Hz
  - `green` — Paul Kellet pink with a 500 Hz bandpass hump
  - `rain` — pink split into wash/sizzle/rumble plus ~55 discrete drops per
    second cut from three bandpassed textures, then soft-clipped
  - `softrain` — the same generator with 26 drops/s, softer, lowpassed at
    3.2 kHz
  - `ocean` — brown under two swell envelopes (2 and 3 cycles per loop) with
    bandpassed foam on the crests
- **Ambient synth** (`makePad`): five chord themes, each rendered as two
  co-prime loops — a 23 s chord bed and a 31 s melody line — so the pair only
  realigns after about 12 minutes. Additive sine partials written into the
  buffer modulo its length, so notes wrap seamlessly; then a two-lap lowpass so
  the filter state at the seam matches.

### Live audio graph (Web Audio)

```
per open fader:  BufferSource(loop) → fadeGain → levelGain ─┐
                                                            ├→ highpass 24 Hz
silent clock:    BufferSource(loop) → gain 0 ──────────┐    │        ↓
                                                       │    │   lowpass (tone)
ambient bed:     BufferSource(loop) ─┐                 │    │        ↓
ambient melody:  BufferSource(loop) ─┴→ padFade → padGain    │   swell gain ← LFO
                                                       │    │        ↓
                                                       │    └── compressor (limiter)
                                                       ↓            ↓
                                                    env gain ───────┘
                                                       ↓
                                                   master vol
                                                       ↓
                             MediaStreamDestination → <audio srcObject>
                                          (or ctx.destination as fallback)
```

Three details matter for the port:

1. **The `<audio>` element route is deliberate.** Web Audio is fed into a
   `MediaStreamDestination` and played through a hidden `<audio>` element, so
   the browser classifies it as *media playback* rather than as page audio.
   That is what currently lets it survive a locked screen in mobile Safari and
   appear in the lock-screen controls. There is a fallback that watches the
   element's clock and switches to `ctx.destination` if the stream never plays.
2. **The sleep timer runs on the audio clock, not on wall time.** A silent
   looping source is started with `stop(endTime)` scheduled on it, and its
   `onended` fires `finish()`. Fades are `linearRampToValueAtTime` on the
   audio clock. This is why the timer still fires when the page is frozen.
   `setInterval` is used only to repaint the countdown.
3. **Buffers are held only while a fader is open**, and handed back when it
   closes. A 20 s stereo loop at 48 kHz is 7.7 MB.

| Buffers resident | 48 kHz |
|---|---|
| One noise loop | 7.7 MB |
| Ambient bed (23 s) | 8.8 MB |
| Ambient melody (31 s) | 11.9 MB |
| Typical: 2 faders + a theme | 36.1 MB |
| Worst case: 5 faders + a theme | 59.1 MB |

### Persistence

One `localStorage` key, `brokild-sleep-noise`, holding a single JSON object.
Unchanged since the app was called Sleep Noise — renaming it would silently
discard everyone's settings. Keys:

```
mix {brown,green,rain,softrain,ocean}   volume   tone
mode   durationMin   untilTime          fadeInSec   fadeOutMin
waves   waveDepth   wavePeriod          pad   padLevel
bgPlayback   keepAwake   autoDim
```

No personal data, no identifiers, no timestamps, no usage history. A migration
path reads the pre-mixer `type` key and maps it onto the new faders.

---

## 2. Feature inventory (the regression baseline)

Pinned down by `test/regression.mjs` — 114 automated checks, all passing at the
baseline commit. Run it before and after every port change:

```
node health-apps/sleep-noise/test/regression.mjs           # ~35 s
node health-apps/sleep-noise/test/regression.mjs --slow     # adds the ~90 s timer test
```

**Verified present and working, not assumed:**

| Feature | Detail |
|---|---|
| Five-fader mixer | brown, green, rain, soft rain, ocean; independent levels, summed |
| Five presets | Deep, Rainroom, Forest, Shore, Drift — each sets every fader, tone, theme and theme level |
| Preset matching | by shape, not a stored flag; survives reload, returns if a fader is moved away and back |
| Ambient synth | six buttons (off + five themes) and its own level fader |
| Master volume | squared curve |
| Tone | 200 Hz–20 kHz log-swept lowpass over the noise bus |
| Swell | optional slow LFO on the whole noise bus; depth and period adjustable |
| Sleep timer | "play for" (12 duration chips + 15-minute stepper) or "stop at" a clock time |
| Fade in | off / 10 s / 30 s / 1 / 2 / 5 min |
| Fade out | off / 1 / 5 / 10 / 20 / 30 min before the stop |
| Unlimited mode | no timer at all |
| Blackout screen | true-black overlay, dim clock, auto-entry after a minute, tap to wake |
| Persistence | every setting, across reloads |
| Migration | from pre-mixer saved settings |
| Silent-but-running | every fader down and the theme off is legal; the timer still runs |
| Live mixing | a fader raised mid-playback joins without interrupting the audio |

**DSP invariants checked at 44.1 and 48 kHz:** no non-finite samples, peak
within 0.9, RMS in 0.05–0.30, DC below 2e-3, channels balanced within 25%,
loop-wrap step smaller than the largest step inside the buffer, build under 5 s.

### Two defects the baseline suite found immediately

- **DC offset.** Green, rain and soft rain carried a standing offset of up to
  9e-3 (−41 dBFS). Pink's generator has gain right down to DC and everything
  cut from pink inherited it; brown has been trimmed at 26 Hz since the
  beginning for exactly this reason. The same cut now ends the pink-derived
  layers. Offsets are now around 1e-7. Fixed in `d44838e`.
- **Stale preset label.** Moving a fader left the preset name and its lit chip
  unchanged for up to a second, until the next UI tick. Fixed in `d44838e`.

---

## 3. Likely iOS incompatibilities

Assessed against the code, ordered by how much work they imply. Nothing here
has yet been confirmed on a device — there is no Apple hardware in this
environment. Items marked **must verify** are the ones a device will settle.

### 3.1 `navigator.wakeLock` — unsupported on iOS

Used in seven places for the "Keep the phone awake" switch. The Screen Wake
Lock API is not implemented in WebKit on iOS, so `navigator.wakeLock` is
`undefined` and the switch already does nothing there — the app correctly
reports this (*"This browser cannot keep the screen awake"*), but the option is
dead weight on the platform.

Natively the requirement is different and simpler. Holding the *screen* awake
all night is the wrong goal; what matters is that *audio* survives the screen
turning off, which is the background-audio capability, not a wake lock. The
switch should become an idle-timer hold used only by blackout mode
(`UIApplication.shared.isIdleTimerDisabled`), where keeping a deliberately
black screen lit is the actual intent.

### 3.2 `requestFullscreen` — unavailable for arbitrary elements

Blackout mode calls `dim.requestFullscreen()` / `webkitRequestFullscreen()` to
remove the browser's own bars, which are the only thing left glowing once the
page is black. WebKit on iOS only offers fullscreen for `<video>`, so this
silently fails and the app falls back to advising "add to home screen".

Natively this problem disappears: there are no browser bars. Blackout becomes
strictly better. The fullscreen code and the home-screen advice should both be
suppressed in the native build.

### 3.3 MediaSession / Now Playing — present but not sufficient

`navigator.mediaSession` is set with metadata and play/pause/stop handlers.
WKWebView's MediaSession support is partial and its relationship to the real
Now Playing centre is not something to depend on for a shipping app.

Replace with native `MPNowPlayingInfoCenter` and `MPRemoteCommandCenter`,
driven from the plugin. Only `play` and `pause` (and `togglePlayPause`) should
be registered — there is no next/previous concept here, and registering
unwanted commands makes the lock screen show controls that do nothing.

### 3.4 `createMediaStreamDestination` → `<audio srcObject>` — **must verify**

This is the highest-risk item in the port and the reason for the architecture
in section 4.

In mobile Safari the trick works and is what gives the app its lock-screen
survival today. Inside a Capacitor WKWebView it is much less certain:

- `MediaStream` as an `<audio>` `srcObject` is a WebRTC path; WebKit has
  historically had bugs playing a `MediaStreamDestination` through it, and
  behaviour has changed between iOS releases.
- More fundamentally, WKWebView renders in separate processes. When the host
  app is backgrounded, iOS may suspend or heavily throttle the web content
  process **even when the host app holds the `audio` background mode**, unless
  audio is genuinely flowing through the system media pipeline.
- `AudioContext` on iOS is also subject to being put into an `interrupted`
  state on backgrounding, from which it does not always recover cleanly.

An 8-hour unattended session is exactly the case where "usually works" is not
good enough: the failure mode is silence at 02:00, which the user discovers in
the morning. I am not willing to certify that path without a device, and I do
not think it should be the shipping architecture even if it happens to test
clean once.

### 3.5 `localStorage` — evictable

WKWebView's local storage is subject to eviction under storage pressure and to
being cleared by "Clear website data" style operations in some configurations.
For an app whose entire state is one small JSON object, `UserDefaults` via
Capacitor Preferences is strictly more reliable. Migrate, keeping the same
shape and a one-time read of any existing `localStorage` value.

### 3.6 Layout and touch behaviour

- `env(safe-area-inset-*)` is already used for the body padding, the blackout
  bar and the blackout hint — good. Needs checking against the Dynamic Island
  and the home indicator on a device.
- `viewport-fit=cover` and `maximum-scale=1` are already set.
- `overscroll-behavior-y: contain` is set on the body, but WKWebView still
  rubber-bands the scroll view itself. Capacitor exposes this as a config
  setting rather than CSS.
- `touch-action: manipulation` is set, which suppresses double-tap zoom.
- Text selection is not suppressed. A long press on a label in a native app
  showing the Copy/Look Up menu reads as a bug. Needs
  `-webkit-user-select: none` on chrome, kept **on** for nothing in particular
  here since there is no user content to copy.
- `-webkit-tap-highlight-color: transparent` is already set.
- The `<a class="home" href="../../">BrokildApps</a>` link and the footer link
  navigate to the website. In a native app they would either fail or, worse,
  navigate the WebView away from the app. Must be hidden natively.

### 3.7 Audio session, interruptions, routes

The web app has no concept of any of these; the browser handles them opaquely.
Natively all of it must be handled explicitly: interruptions (calls, Siri,
alarms), route changes (AirPods in/out, headphones unplugged, Bluetooth
switching), and session deactivation/reactivation. See section 4.

Default posture, chosen conservatively: **an interruption pauses and does not
auto-resume unless iOS says the interruption ended with the "should resume"
option.** Unexpected sound at 3 a.m. is worse than silence.

### 3.8 Build times on a phone

Starting the Rainroom preset builds two noise loops plus two ambient loops. On
this desktop that measures 1.4–1.7 s. On a phone, expect several times that.
The existing "busy" state covers it, but it is a poor first impression for a
paid app and it recurs on every single start. The buffer cache in section 4
removes it after the first run.

### 3.9 Non-issues worth recording

- No autoplay problem: audio always starts from an explicit tap.
- No CORS, mixed-content, or CSP concerns — nothing is loaded.
- No IndexedDB, no service worker, no push, no permissions prompts of any kind.
- No protected-API usage, so no `Info.plist` purpose strings are required.
- No camera, microphone, location, contacts, photos, HealthKit, or
  notifications. **The app requires no permissions at all.**

---

## 4. Background-audio architecture recommendation

### Recommendation: keep the JavaScript generators, move playback into native

**Rejected: Web Audio in the WebView with `UIBackgroundModes: audio`.**
Cheapest to build, and it might well pass a first test. But per 3.4 the
suspension behaviour of the web content process is not under our control, the
failure mode is an entire silent night, and the whole product promise is
"reliable for eight hours". This is the wrong place to accept risk.

**Rejected: rewrite the synthesis in Swift.** The generators are ~350 lines of
portable array maths and would port. But they *are* the product — the exact
character of the ocean swell, the drop distribution in the rain, the two-lap
lowpass at the pad seam. A Swift reimplementation I cannot compile or measure
in this environment would risk shipping a subtly different app, and the offline
harness that currently proves the DSP correct would no longer be testing what
ships.

**Chosen: JavaScript renders, native plays.**

The application's own design makes this unusually clean. Everything is already
rendered into finite looping buffers, and everything downstream of them is
simple, native-friendly signal flow:

```
JavaScript                          │  Swift (AVAudioEngine)
────────────────────────────────────┼──────────────────────────────────────
genRaw / makeBuffer / makePad       │  AVAudioPlayerNode per layer (looping)
  → 20 s / 23 s / 31 s PCM loops    │        ↓
  → cached as WAV in Caches/        │  AVAudioMixerNode  (fader levels)
                                    │        ↓
UI, presets, settings, countdown    │  AVAudioUnitEQ lowpass  (tone)
                                    │        ↓
                                    │  swell gain, driven natively
                                    │        ↓
                                    │  master envelope (fade in / fade out)
                                    │        ↓
                                    │  output  +  AVAudioSession .playback
                                    │            MPNowPlayingInfoCenter
                                    │            interruption / route handling
```

Native owns everything that has to survive backgrounding: the audio session,
the players, the gains, the sleep timer (scheduled on the engine's sample time,
the same principle the web app already uses on the audio clock), and the fades.
JavaScript owns the UI and the buffer generation, and it may be frozen for the
whole night without consequence.

Why this is the right trade:

- Background reliability stops being a WebView question and becomes the
  ordinary iOS audio path, which is designed for exactly this.
- The sound is bit-identical to the tested web version, because it is the same
  generator output.
- It answers App Store guideline 4.2 (minimum functionality) decisively: this
  is not a website in a wrapper, it is a native audio engine with a web UI.
- The existing offline DSP harness keeps its value — it tests the code that
  actually ships.

**PCM handoff.** Buffers cross the bridge once, as 16-bit WAV written to the
app's Caches directory, keyed by layer id and sample rate. 16 bits is ample:
the signal is noise, which dithers itself, and the material is normalised to
0.18 RMS. First play of a given layer generates and caches; every later play,
including on later nights, skips both generation and transfer and native loads
the file directly. This also fixes 3.8 — the slow start happens once, not every
night.

**Fallback.** The Web Audio path is not deleted. The audio backend becomes an
interface with two implementations, selected at runtime by whether the native
plugin is present. The browser build keeps Web Audio and remains the canonical
web app; the native build uses the plugin. If device testing shows the native
engine misbehaving in some specific case, the fallback is one flag away, and
the web app is never at risk from native work.

### Capabilities and session configuration

- `UIBackgroundModes` = `["audio"]`. Nothing else.
- `AVAudioSession` category `.playback`, mode `.default`, no
  `.mixWithOthers` (this app is the thing you are listening to), and
  `.duckOthers` off.
- Activate on first play, not at launch, so the app never takes the audio
  session — or interrupts someone's music — merely by being opened.
- Deactivate when the timer finishes or playback is stopped, so the Now
  Playing slot is released.

---

## 5. Proposed source-tree changes

Two constraints shaped this:

- `health-apps/sleep-noise/` is served live by GitHub Pages. Putting
  `node_modules/`, an Xcode project and a build output directory in that path
  would publish a lot of dead weight and risks breaking the page.
- Duplicating `index.html` into a native `www/` would create two copies that
  drift apart. That is the failure mode to avoid above all others.

So: **one canonical source file, a generated `www/`, and the native project
kept in a sibling directory.**

```
health-apps/sleep-noise/          ← published web app, remains canonical
    index.html                      one source, two audio backends chosen at runtime
    icon.svg  manifest.webmanifest  app.json
    IOS_PORT_NOTES.md               this file
    IOS_QA_CHECKLIST.md
    test/regression.mjs

ios-apps/sleeper-agent/           ← native project, not published
    package.json
    capacitor.config.ts
    scripts/build-www.mjs           copies the web app into www/
    www/                            generated, git-ignored
    ios/App/…                       Xcode project
    ios/App/App/plugins/            the Swift audio plugin
    assets/icon/                    icon source + generated sizes
    APP_STORE_METADATA.md
    APP_PRIVACY_NOTES.md
    README-iOS.md
```

`index.html` stays a single file that works when opened directly in a browser,
as it does today. The native adaptations are runtime-conditional, so the web
build is unaffected and the regression suite keeps testing the real thing. The
native build step is a copy, not a transform, so there is nothing to get out of
sync.

`.gitignore` (the repository currently has none) gains `node_modules/`,
`ios-apps/*/www/`, `ios/App/Pods/`, `*.xcuserstate`, `build/`, `DerivedData/`,
and the signing-artefact patterns from section 8.

---

## 6. Detected development environment

| | |
|---|---|
| OS | Ubuntu 24.04.4 LTS, Linux 6.18.44 |
| Arch | x86_64, 4 cores, 16 GB RAM, 30 GB free |
| Host | ephemeral remote container (work is lost unless pushed) |
| Network | outbound HTTPS via proxy; npm registry reachable |

### Tools present

| Tool | Version |
|---|---|
| git | 2.43.0 |
| node | 22.22.2 |
| npm | 10.9.7 |
| python3 | 3.11.15 |
| ruby | 3.3.6 |
| Chromium | Playwright 1.56.1 build 1194 (`/opt/pw-browsers/chromium`) |
| Playwright | 1.56.1 (global) |

Capacitor 8.5.1 is the current release on npm, matching the brief.

### Tools absent

| Tool | Consequence |
|---|---|
| **Xcode** | no build, no archive, no simulator, no signing, no validation |
| **swift / swiftc** | **Swift plugin code cannot be compiled or type-checked here** |
| **CocoaPods** (`pod`) | `npx cap add ios` cannot complete its pod install step |
| **xcrun / simctl** | no simulator screenshots |
| ImageMagick / rsvg / Inkscape | no SVG rasteriser — worked around with headless Chromium |

### The hard boundary

Everything up to and including a complete, reviewed Capacitor project with
Swift sources, assets, tests and documentation can be done here. **Nothing
that requires compiling, signing, archiving, running on a simulator or device,
or talking to App Store Connect can be done here.** That work needs macOS with
a current Xcode.

This has one consequence that must be stated plainly and repeated in the QA
checklist: **the Swift plugin in this repository has never been compiled.** It
is written carefully against the AVFoundation and MediaPlayer APIs, but first
build on a Mac should be expected to surface compile errors, and every claim
about background behaviour is a design intention until a device confirms it.
Nothing in section 4 is "tested" in the sense the rest of this document uses
that word.

---

## 7. Work plan

| Phase | Where | Status |
|---|---|---|
| 1. Baseline + regression suite | here | **done** — 114 checks |
| 2. This document | here | **done** |
| 3. Capacitor scaffold, config, gitignore | here | **done** — Capacitor 8.5.1, SPM (no CocoaPods) |
| 4. Audio backend chosen at runtime; Web Audio path untouched | here | **done** |
| 5. Swift plugin (session, engine, timer, Now Playing, interruptions) | here | **written, never compiled** |
| 6. Native JS backend + chunked WAV handoff + disk cache | here | **done** — 65 checks against a mock plugin |
| 7. iOS adaptations (chrome, haptics, idle timer, status bar, settings mirror) | here | **done** |
| 8. Icon + launch screen assets | here | **done** |
| 9. Docs, App Store metadata, privacy notes, privacy policy page | here | **done** |
| 10. First compile, fix build errors | **Mac** | |
| 11. Simulator run, screenshots | **Mac** | |
| 12. Device QA — the whole of `IOS_QA_CHECKLIST.md` | **Mac + iPhone** | |
| 13. Apple Developer Program enrolment | **human** | |
| 14. Signing, archive, validate | **Mac + human** | |
| 15. TestFlight round, multi-night testing | **human** | |
| 16. App Store Connect record, pricing, agreements, declarations | **human** | |
| 17. Submit for review | **human** | |

---

## 8. Security notes

Never committed, and covered by `.gitignore`: Apple credentials, App Store
Connect API keys (`AuthKey_*.p8`), signing certificates and private keys
(`*.p12`, `*.cer`, `*.mobileprovision`), keychains, and any `ExportOptions`
containing a team identifier that the user has not chosen to publish.

The app itself holds no secrets, has no endpoints, and needs no credentials of
any kind at runtime.
