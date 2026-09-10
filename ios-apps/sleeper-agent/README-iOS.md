# Sleeper Agent for iOS

The native iOS/iPadOS build of the web app at `health-apps/sleep-noise/`.

Written so that someone picking this up later does not have to reconstruct the
reasoning. The reasoning itself — why the architecture is what it is, what was
rejected and why — is in `health-apps/sleep-noise/IOS_PORT_NOTES.md`.

---

## 1. Architecture in one page

```
health-apps/sleep-noise/index.html          ← ONE canonical source
        │                                      the website and the app
        │  (copied verbatim by scripts/build-www.mjs)
        ▼
ios-apps/sleeper-agent/www/                 ← generated, git-ignored
        │
        ▼
   WKWebView (Capacitor 8)
        │
        │  renders the loops, owns the UI
        │  ── SleeperAudio  ──►  loops as chunked 16-bit WAV, cached in Caches/
        │  ── SleeperShell  ──►  haptics, idle timer, status bar, settings mirror
        ▼
   SleeperEngine (Swift)
        AVAudioSession .playback  +  UIBackgroundModes: audio
        AVAudioEngine
          one looping AVAudioPlayerNode per fader → noise mixer
            → AVAudioUnitEQ (tone lowpass) → peak limiter ─┐
          two looping players for the ambient theme → pad mixer ─┤
                                                                 ▼
                                            master (envelope × volume) → output
        one 40 Hz control loop drives: swell, fade-in, fade-out, sleep timer
        MPNowPlayingInfoCenter + MPRemoteCommandCenter
        interruption / route-change / media-reset / config-change handling
```

**The key idea:** JavaScript renders, native plays. Everything in this app was
already rendered into finite looping buffers, and everything downstream of them
is signal flow that `AVAudioEngine` does natively. So once playback starts, the
WebView may be frozen until morning without consequence — the engine, the
fades and the timer are all in Swift, in a process iOS keeps alive because it
is playing audio.

**One source of truth.** `index.html` picks its audio backend at runtime by
whether the Capacitor bridge is present. The build step is a *copy*, never a
transform, so the website and the app cannot drift, and the test suites test
the file that actually ships.

### Where things are

| Path | |
|---|---|
| `health-apps/sleep-noise/index.html` | the entire app — UI, DSP, both backends |
| `health-apps/sleep-noise/test/regression.mjs` | 132 checks, the web app |
| `health-apps/sleep-noise/test/native-bridge.mjs` | 67 checks, the JS side of the native path |
| `health-apps/sleep-noise/IOS_PORT_NOTES.md` | architecture and decisions |
| `health-apps/sleep-noise/IOS_QA_CHECKLIST.md` | **read before trusting anything** |
| `ios/App/App/Native/SleeperEngine.swift` | the audio engine |
| `ios/App/App/Native/SleeperAudioPlugin.swift` | the bridge, loop transfer, WAV cache |
| `ios/App/App/Native/SleeperShellPlugin.swift` | haptics, idle timer, status bar, settings mirror |
| `ios/App/App/Native/SleeperViewController.swift` | `CAPBridgeViewController` + hideable status bar |
| `scripts/build-www.mjs` | copies the web app into `www/`, with guards |
| `scripts/render-assets.mjs` | rasterises the icon and launch mark |
| `ios/App/App.xcodeproj/xcshareddata/xcschemes/App.xcscheme` | the shared scheme. Committed on purpose — a build machine finds no scheme without it |
| `../../.github/workflows/sleeper-agent-ios.yml` | builds, signs and uploads on a GitHub-hosted Mac |
| `NO_MAC_ROUTE.md` | what a Mac is actually needed for, and how to ship without owning one |
| `assets/icon/*.svg` | icon and launch-mark artwork sources |
| `APP_STORE_METADATA.md` | listing copy, screenshots plan, review notes |
| `APP_PRIVACY_NOTES.md` | what to answer in the App Privacy questionnaire |

---

## 2. Required tools

| | |
|---|---|
| **macOS** | required for anything that builds, signs, archives or uploads |
| **Xcode** | current release. Apple requires submissions built with the iOS 26 SDK or later — verify the current floor before submitting |
| Xcode Command Line Tools | `xcode-select --install` |
| Node.js | 20 or newer (developed on 22) |
| CocoaPods | **not needed.** Capacitor 8 uses Swift Package Manager |

The web work, the Swift sources, the assets, the tests and all documentation
can be done on any platform. Only the build onwards needs a Mac.

---

## 3. Install and first build

```bash
git clone https://github.com/peterboggild/BrokildApps.git
cd BrokildApps
git checkout ios-sleeper-agent

cd ios-apps/sleeper-agent
npm install
npm run sync            # builds www/ and runs `cap sync ios`
npm run open            # opens ios/App/App.xcodeproj
```

In Xcode, let Swift Package Manager resolve `capacitor-swift-pm`, then build.

> **Expect compile errors on the very first build.** The Swift in this
> repository was written on Linux and has never been through `swiftc`. See
> `IOS_QA_CHECKLIST.md` §0 and §1.

## 4. Everyday development

```bash
# Edit the web app — the real one, in health-apps/sleep-noise/index.html.
npm test                # both suites, ~1 minute
npm run sync            # push the change into www/ and the iOS project
```

`npm run build` alone refreshes `www/`. It **fails on purpose** if
`index.html` loses a hook the native build depends on, or gains a network
request that would break airplane-mode use. If it fails, read the message
before changing the guard.

For UI-only work, opening `health-apps/sleep-noise/index.html` directly in a
browser is much faster than a device round-trip. The Web Audio path behaves
the same; only the audio backend differs.

## 5. Testing

```bash
node health-apps/sleep-noise/test/regression.mjs          # 132 checks, web app
node health-apps/sleep-noise/test/regression.mjs --slow    # + end-to-end timer (~90 s)
node health-apps/sleep-noise/test/native-bridge.mjs        # 67 checks, native path
```

The bridge suite installs a mock Capacitor plugin before the page's own script
and records every call, so it can check the call sequence, the exact PCM byte
count, the chunking, the parameter mapping and every engine event — without a
Mac. **It executes no Swift.** Device testing is not optional; the checklist
says what it has to cover.

## 6. Simulator

Fine for layout and logic. Useless for conclusions about background audio: the
simulator's audio session is not a phone's. `IOS_QA_CHECKLIST.md` §2 lists
what is worth doing there.

## 7. Physical device

Configuration is already done in the project. What is left needs your hands:

1. Connect the iPhone by cable and unlock it.
2. If iOS asks, tap **Trust This Computer** and enter the passcode.
3. If Xcode asks for Developer Mode: **Settings → Privacy & Security →
   Developer Mode → on**, then restart the phone.
4. In Xcode, choose the phone from the run-destination menu.
5. Signing: **Xcode → Settings → Accounts → +** and sign in with the Apple
   Account for the app. Then in the target's **Signing & Capabilities**, tick
   *Automatically manage signing* and pick the Team. A free account is enough
   to run on your own device; TestFlight and the App Store need paid Apple
   Developer Program membership.
6. Run, then work through `IOS_QA_CHECKLIST.md` §3 onwards.

## 8. Release build

```
Version (marketing):  1.0.0     — in the target's General tab
Build:                1         — increase for every upload, never reuse
Configuration:        Release
Bundle identifier:    com.peterboggild.sleeperagent   (provisional — confirm)
```

Work `IOS_QA_CHECKLIST.md` §12 before archiving.

## 9. Archive and upload

```
Xcode → Product → Destination → Any iOS Device (arm64)
Xcode → Product → Archive
Organizer → Validate App        ← fix everything it reports
Organizer → Distribute App → App Store Connect → Upload
```

Then in App Store Connect the build takes some minutes to finish processing
before it can be assigned to TestFlight or a release.

## 10. TestFlight

1. App Store Connect → your app → **TestFlight**.
2. Add yourself to **Internal Testing** and enable the build for that group.
3. Install through the TestFlight app.
4. Repeat `IOS_QA_CHECKLIST.md` §4 on this build, then use it for at least
   three real nights (§13).

Do not skip a TestFlight round. Section 4 is the whole product, and it cannot
be checked from a desk.

## 11. App Store release

`APP_STORE_METADATA.md` has the listing copy and the review notes;
`APP_PRIVACY_NOTES.md` has what to answer in the privacy questionnaire and
why. Both are drafts for you to approve — the declarations are legally
meaningful and must be made by a person.

Selling the app also needs, in App Store Connect: the **Paid Apps Agreement**
accepted, and tax and banking details completed. These cannot be guessed.

---

## 12. Things worth knowing before you change something

**Do not rewrite the DSP in Swift.** The generators are the product, and the
offline suite measures the code that ships. Moving them to Swift would retire
that guarantee.

**Do not make startup asynchronous.** `index.html` reads its settings
synchronously from `localStorage` so the first painted frame is correct. The
UserDefaults mirror exists for the rare case where WKWebView has evicted local
storage; it restores and reloads rather than making everyone wait.

**The sleep timer is on a monotonic clock**, in both backends. A phone that
corrects its clock or changes time zone overnight must not lengthen or shorten
the night.

**Interruptions do not auto-resume** unless iOS says `shouldResume`. That is
deliberate. Sound restarting on its own at 3 a.m. is worse than sound not
restarting.

**The loop cache is in `Caches/`,** so iOS may evict it. That is fine and
intended: the app re-renders on the next start. Do not move it to
Documents — it is reproducible data, and Documents is backed up.

**`build-www.mjs`'s guards are load-bearing.** They are the only thing that
notices if a change to the web app quietly breaks the native build or the
offline promise.

**Never commit signing material.** `.gitignore` covers `*.p12`, `*.cer`,
`*.mobileprovision`, `AuthKey_*.p8`, keychains and `ExportOptions.plist`. The
app itself holds no secrets and needs no credentials at runtime.
