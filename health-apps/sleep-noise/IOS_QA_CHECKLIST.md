# Sleeper Agent — iOS QA checklist

What has been tested, what has not, and what only a device can settle.

Fill in the right-hand column as you go. Anything not ticked is not tested,
and should not be described as working.

---

## 0. Read this first

**The Swift has never been compiled.** It was written on Linux, where no Apple
toolchain exists. Everything in `ios-apps/sleeper-agent/ios/App/App/Native/`
is careful code against the AVFoundation and MediaPlayer APIs and nothing
more. Budget for a round of compile errors on the first Mac build.

**Every claim about background audio is a design intention until section 4 is
ticked.** The architecture was chosen (see `IOS_PORT_NOTES.md` §4) precisely
because it does not depend on WKWebView surviving backgrounding — but that
reasoning is an argument, not a measurement.

### What *is* tested, and by what

| | |
|---|---|
| `test/regression.mjs` | 132 checks. The web app: DSP invariants at 44.1 and 48 kHz, panel shape, all five presets, persistence, migration from pre-mixer settings, transport, blackout. Add `--slow` for the end-to-end sleep timer. |
| `test/native-bridge.mjs` | 67 checks. The **JavaScript half** of the native path against a mock plugin: call sequence, PCM byte count, chunking, parameter mapping, cache behaviour on a second start, blackout, haptics, the settings mirror, and every engine event. |

Both suites pass at the current commit. Neither of them executes a single line
of Swift.

```
node health-apps/sleep-noise/test/regression.mjs
node health-apps/sleep-noise/test/native-bridge.mjs
```

---

## 1. First build (Mac)

| # | Check | ✓ |
|---|---|---|
| 1.1 | `cd ios-apps/sleeper-agent && npm install` | |
| 1.2 | `npm run sync` — builds `www/` and runs `cap sync ios` | |
| 1.3 | `npm run open` — Xcode opens `App.xcworkspace` | |
| 1.4 | Swift Package dependencies resolve (`capacitor-swift-pm`) | |
| 1.5 | Project builds for a simulator. **Record every error fixed here.** | |
| 1.6 | No warnings that indicate a functional problem | |
| 1.7 | `SleeperAudioPlugin` and `SleeperShellPlugin` appear in the Xcode console as registered plugins at launch | |
| 1.8 | Target's Signing & Capabilities shows **Background Modes → Audio** | |

If 1.7 fails the app will silently fall back to… nothing: `NATIVE` will be
false, the app will try Web Audio inside the WebView, and it may even seem to
work in the foreground. **Check 1.7 explicitly**; do not infer it from hearing
sound.

## 2. Simulator (cheap checks only)

The simulator's audio session is not the phone's. Use it for layout and logic,
never to conclude anything about backgrounding.

| # | Check | ✓ |
|---|---|---|
| 2.1 | Launch shows the dark launch screen with the crescent — **no white flash** | |
| 2.2 | Panel renders; no link back to the BrokildApps website anywhere | |
| 2.3 | The "Play with screen locked" switch is absent | |
| 2.4 | The remaining switch reads "Keep the screen on in blackout" | |
| 2.5 | First play shows "Preparing brown…" then starts; sound is heard | |
| 2.6 | Second play starts immediately (loops now cached) | |
| 2.7 | All five presets load and sound different from one another | |
| 2.8 | Every fader audibly changes its own layer and nothing else | |
| 2.9 | Long-pressing a label does **not** raise Copy / Look Up | |
| 2.10 | Blackout goes fully black, status bar disappears, tap wakes it | |
| 2.11 | iPhone SE 3rd gen: nothing clipped, no horizontal scroll | |
| 2.12 | iPhone 17 Pro Max: safe areas respected top and bottom | |
| 2.13 | iPad: portrait and all rotations lay out sensibly | |
| 2.14 | Rotating an iPhone does **not** rotate the app (portrait locked) | |

## 3. Real device — the basics

| # | Check | ✓ |
|---|---|---|
| 3.1 | Installs and launches on a physical iPhone | |
| 3.2 | Sound plays with the app in the foreground | |
| 3.3 | Volume, tone and every fader respond while playing, without clicks | |
| 3.4 | Preset changes are smooth — no gap, no click, no jump in level | |
| 3.5 | Theme change (e.g. Rainroom → Drift) restarts cleanly, timer intact | |
| 3.6 | **All five faders at 100 with a theme at 100: listen for clipping.** This is the case the peak limiter exists for; if the limiter failed to attach, the Xcode log says "peak limiter unavailable" | |
| 3.7 | Haptics fire on preset, play/pause and blackout — and **not** while dragging a fader | |
| 3.8 | Blackout is genuinely black in a dark room (no grey, no status bar) | |

## 4. Real device — background audio · **THE CRITICAL SECTION**

Nothing else in this document matters if this section fails. Do these on a
phone, not a simulator, and with the phone unplugged.

| # | Check | ✓ |
|---|---|---|
| 4.1 | Start playing, lock the screen. Sound continues. | |
| 4.2 | Still playing after **5 minutes** locked | |
| 4.3 | Still playing after **60 minutes** locked | |
| 4.4 | Still playing after a **full night** (7–8 h) locked | |
| 4.5 | Switch to another app: sound continues | |
| 4.6 | Open a video in another app, then return: state is sane | |
| 4.7 | Lock screen shows title and subtitle (e.g. "Rainroom", "6 h 12 m left") | |
| 4.8 | Lock-screen pause works, and play resumes | |
| 4.9 | Control Center pause/play works | |
| 4.10 | Lock screen shows **no** next/previous/scrubber controls | |
| 4.11 | Force-quitting the app stops the audio and clears Now Playing | |
| 4.12 | Reopening after a force-quit does **not** start playing by itself | |

If 4.2–4.4 fail, do not start patching blindly. Capture the Xcode device log
around the failure and check, in order: was the audio background mode present
in the built app; did `AVAudioSession` stay active; did
`AVAudioEngineConfigurationChange` or `mediaServicesWereReset` fire; is
`SleeperEngine.isPlaying` still true while silent (engine stopped) or false
(something called stop).

## 5. Real device — the sleep timer

| # | Check | ✓ |
|---|---|---|
| 5.1 | "Stop at" a time ~2 minutes ahead: stops at that minute | |
| 5.2 | Fade-out is audible and gradual, not a cut | |
| 5.3 | Same again with the screen **locked** for the whole session | |
| 5.4 | Same again with the app **backgrounded** | |
| 5.5 | A "play for 15 min" session ends on time | |
| 5.6 | Reopening the app mid-countdown shows the correct time remaining | |
| 5.7 | Reopening after the timer finished shows "Timer finished" and is stopped | |
| 5.8 | Unlimited mode plays indefinitely and never fades | |
| 5.9 | Pausing and resuming does not shorten the night | |
| 5.10 | Timer expiry releases the Now Playing slot | |
| 5.11 | Change the phone's time zone mid-session: the countdown does **not** jump (the engine is on the monotonic clock) | |

## 6. Real device — interruptions and routes

Expected posture: **pause, and do not resume unless iOS says to.** Sound
restarting on its own at 3 a.m. is worse than sound not restarting.

| # | Check | ✓ |
|---|---|---|
| 6.1 | Incoming phone call: audio pauses | |
| 6.2 | After the call: behaviour matches what iOS asked for; panel agrees with reality | |
| 6.3 | Siri: pauses, and afterwards the panel is not lying about its state | |
| 6.4 | An alarm or timer going off: pauses, recovers sanely | |
| 6.5 | Wired headphones unplugged: pauses, panel says why | |
| 6.6 | AirPods removed from ears / disconnected: pauses | |
| 6.7 | AirPods reconnected: playback can be resumed by tapping play | |
| 6.8 | Switch output to a Bluetooth speaker mid-session: sound follows, no silence | |
| 6.9 | Switch back to phone speaker: sound follows | |
| 6.10 | After any of the above, the mixer faders still work | |
| 6.11 | Another app plays audio, then stops: Sleeper Agent's state is sane | |

## 7. Offline and privacy

| # | Check | ✓ |
|---|---|---|
| 7.1 | **Airplane mode, Wi-Fi and Bluetooth off: the app works completely.** | |
| 7.2 | First launch on a fresh install, offline: loops render and play | |
| 7.3 | No permission prompt of any kind ever appears | |
| 7.4 | Xcode's network instrument shows **no** outbound connections during a session | |
| 7.5 | Delete and reinstall: settings are gone (nothing is stored off-device) | |

## 8. Persistence and lifecycle

| # | Check | ✓ |
|---|---|---|
| 8.1 | Every setting survives force-quit and relaunch | |
| 8.2 | Settings survive a device restart | |
| 8.3 | Settings survive an app **update** (install over the top) | |
| 8.4 | Reopening the app never starts audio on its own | |
| 8.5 | Backgrounding while paused leaves it paused | |

## 9. Battery and performance

Do these on an unplugged phone, screen off, over a real night.

| # | Check | Target | ✓ |
|---|---|---|---|
| 9.1 | Battery used over 8 h, screen off, 2 faders + a theme | ideally under ~10% | |
| 9.2 | Battery over 8 h with blackout screen **on** | expect much more — this is why the switch exists | |
| 9.3 | Xcode Energy gauge during playback | "Low" | |
| 9.4 | CPU during steady playback | low single digits | |
| 9.5 | Memory after an hour | stable, not climbing | |
| 9.6 | Memory with all five faders and a theme open | note the figure; the caches are on disk, but AVAudioPCMBuffers are not | |
| 9.7 | Phone is not warm in the morning | | |
| 9.8 | `Caches/sleeper-loops/` size after opening every fader and theme | note it; iOS may evict this directory, which is fine — it re-renders | |

## 10. Accessibility

Measured on Linux at 375, 393, 834 and 1112 px: every interactive control is
at least 44pt, and there is no horizontal overflow. Re-confirm on a device.

| # | Check | ✓ |
|---|---|---|
| 10.1 | VoiceOver: every control has a meaningful label | |
| 10.2 | VoiceOver on a fader says e.g. "Brown level, 55%" — and "closed" at zero | |
| 10.3 | VoiceOver on Tone says e.g. "soft, 3.2 kHz", not "62" | |
| 10.4 | VoiceOver on the ambient level names the theme | |
| 10.5 | Focus order follows the visual order | |
| 10.6 | Largest Dynamic Type: nothing overlaps or is cut off | |
| 10.7 | Reduce Motion on: nothing objectionable | |
| 10.8 | The open/closed state of a fader is readable without colour (the `—` vs `55%`) | |
| 10.9 | Every control reachable one-handed on the largest iPhone | |

## 11. Audio quality

| # | Check | ✓ |
|---|---|---|
| 11.1 | No click at start of playback | |
| 11.2 | No click at stop | |
| 11.3 | No click when a fader opens or closes | |
| 11.4 | No click at a preset change | |
| 11.5 | No audible seam where a 20-second loop repeats (listen for a minute) | |
| 11.6 | Ambient theme has no audible seam (bed 23 s, melody 31 s) | |
| 11.7 | Ocean swell sounds like surf, not like tremolo | |
| 11.8 | Swell on top of Ocean does not sound like two competing tremolos | |
| 11.9 | No hiss, hum or whine that is not part of the sound | |
| 11.10 | Left and right are balanced on headphones | |
| 11.11 | The mix sounds the same on the phone as the web version does on a desktop | |
| 11.12 | Tone at minimum is dark but not muffled to nothing; at maximum not harsh | |

## 12. Release build

| # | Check | ✓ |
|---|---|---|
| 12.1 | Release configuration builds | |
| 12.2 | Version is `1.0.0`, build number set and higher than any previous | |
| 12.3 | Bundle identifier is the one registered with Apple | |
| 12.4 | App icon appears correctly on the home screen and in Settings | |
| 12.5 | App name under the icon is not truncated awkwardly | |
| 12.6 | No debug logging in the console in a Release build | |
| 12.7 | `webContentsDebuggingEnabled` is false | |
| 12.8 | No `localhost` or development URL anywhere in the bundle | |
| 12.9 | No credentials, keys or tokens in the bundle | |
| 12.10 | Archive validates in Xcode Organizer with no errors | |
| 12.11 | Privacy manifest requirements satisfied (the app declares no tracking and uses no required-reason APIs) | |
| 12.12 | Airplane-mode test repeated on the **release** build | |

## 13. TestFlight

| # | Check | ✓ |
|---|---|---|
| 13.1 | Build uploads and finishes processing | |
| 13.2 | Installs from TestFlight on a device that has never had it | |
| 13.3 | Section 4 repeated on the TestFlight build | |
| 13.4 | At least **three real nights** of ordinary use | |
| 13.5 | Morning state after each night is correct and unsurprising | |
| 13.6 | No crashes reported in App Store Connect | |

## 14. Known-unknowns to watch for on first device contact

The Swift has had one critical read-back pass without a compiler, which found
and fixed five real defects — so the first build starts from better than a
first draft, but still from unproven code:

- `SleeperEngine` used `@objc` and `#selector` while not being an `NSObject`
  subclass. **That could not have compiled.** Now block-based observers.
- `CACurrentMediaTime()` was used without importing QuartzCore.
- A headphone unplug left the engine permanently marked interrupted, which
  stopped the control loop — and with it the sleep timer — for the rest of the
  night. Route loss now stops cleanly.
- An interruption ran the sleep timer down while paused, so a ten-minute call
  cost ten minutes of the night. The timer is now frozen and restored.
- `stop()` held its caller for the whole length of the fade.

What follows is where to look next. None of it has been observed; all of it is
a guess about the places the design is most likely to be wrong.

1. **Plugin registration.** If Capacitor 8's SPM setup does not pick up Swift
   files under `App/Native/`, the plugins never register. Check 1.7.
2. **`AVAudioUnitEQ` low-pass band.** Whether one band with
   `filterType = .lowPass` behaves as expected over the whole 200 Hz–20 kHz
   sweep, and whether changing `frequency` live is glitch-free.
3. **The peak limiter.** `kAudioUnitSubType_PeakLimiter` is created
   defensively and skipped if unavailable. Check 3.6 and the log line.
4. **`master.outputVolume` stepped at 40 Hz.** Fine on noise in theory; listen
   for zipper noise on a long fade-out (5.2).
5. **Bridge payload size.** 512 KB base64 chunks, eight of them per loop. If
   this is slow or memory-hostile on an older phone, reduce `CHUNK_BYTES`.
6. **`AVAudioFile` reading a hand-written WAV header.** `prepareEnd` already
   probes the file before accepting it, so a header bug should show as a
   rejected loop rather than as noise — but check the first render works.
7. **Interruption bookkeeping.** The `.ended`-without-`shouldResume` branch
   adjusts `startedAt` to avoid shortening the night; verify with 5.9.
8. **Idle timer and blackout.** Whether the idle timer hold survives the
   screen being locked manually while blackout is up.
9. **Sample-rate changes mid-session.** Plugging in a device that forces
   44.1 kHz while 48 kHz loops are loaded. AVAudioEngine should resample;
   confirm no silence (6.8).
10. **`UILaunchScreen` without a storyboard.** If the launch screen is blank,
    the colour asset name or image name is wrong.

---

## Sign-off

Do not report an item as passing unless it was actually performed.

```
Tester:
Device / iOS version:
Build:
Date:

Sections completed:
Failures found:
Outstanding:
```
