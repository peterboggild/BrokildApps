# Start here

You are looking at a half-finished iOS app. This file says what state it is
in, where everything is, and what to do next — so that picking it up in six
months costs you an afternoon of reading rather than a re-derivation.

**Last worked on:** 9 September 2026
**Branch:** `ios-sleeper-agent`
**Nothing has been submitted to Apple. No money has been spent. No Apple
account has been created.**

---

## 1. What this is

Sleeper Agent is a sleep-sound app that has existed for a while as a web page
at `health-apps/sleep-noise/index.html`, published on GitHub Pages. This
directory turns that same page into a native iOS/iPadOS app using Capacitor,
with a real native audio engine underneath it.

The web version still works and is unaffected. It is the same file.

## 2. State of play

| | |
|---|---|
| Web app | **Working**, unchanged in behaviour, 132 automated checks passing |
| Capacitor project | **Set up**, Capacitor 8.5.1, Swift Package Manager (no CocoaPods) |
| JavaScript side of the native bridge | **Written and tested** — 67 checks against a mock plugin |
| Swift audio engine | **Written. Never compiled.** No Apple toolchain existed on the machine it was written on |
| App icon, launch screen | **Done** |
| Privacy policy, support page | **Written and published** in the repo |
| App Store listing copy | **Drafted**, awaiting your approval |
| Anything requiring a Mac | **Not started** |
| Anything requiring an Apple account | **Not started** |

The single most important sentence in this repository: **the Swift has never
been through a compiler.** Expect a round of build errors the first time you
open it in Xcode. That is normal and expected, not a sign something is wrong.

## 3. The one thing standing in the way

**Compiling it.** Building, signing and uploading an iOS app needs macOS — but
not macOS *that you own*.

`.github/workflows/sleeper-agent-ios.yml` builds, signs and uploads the app on
a GitHub-hosted Mac that exists for ten minutes and is then destroyed. macOS
runner minutes are free on public repositories, and this repository is public.
Everything else — enrolment, the listing, TestFlight, submission — is a
browser, and works on an iPad.

**`NO_MAC_ROUTE.md`** covers that path in full, including how to make the
signing certificates with `openssl` instead of a Mac's Keychain.

The recommendation is still to **borrow a Mac for one afternoon** for the first
compile and the first night of device testing, because the Swift here has never
been compiled and fixing that through a ten-minute CI round trip is slow going.
After that, the CI route serves indefinitely. `SHIPPING_GUIDE.md` step 1
compares all the options.

## 4. Where everything is

Read in this order if you are coming back cold:

| File | What it gives you |
|---|---|
| **`START_HERE.md`** | this file |
| **`SHIPPING_GUIDE.md`** | the complete step-by-step from here to the App Store, written for someone who has never done it |
| **`NO_MAC_ROUTE.md`** | can an iPad do this? What a Mac is actually needed for, and how to ship without owning one |
| `../../health-apps/sleep-noise/IOS_PORT_NOTES.md` | why the app is built the way it is — the architecture and the alternatives that were rejected |
| `README-iOS.md` | the maintenance document: architecture on a page, every file, the build commands, what not to break |
| `../../health-apps/sleep-noise/IOS_QA_CHECKLIST.md` | what to test, in order. §4 is the one that matters |
| `APP_STORE_METADATA.md` | the listing: name, description, keywords, screenshots plan, review notes, pricing recommendation |
| `APP_PRIVACY_NOTES.md` | what to answer in Apple's privacy questionnaire, and how to verify it yourself |
| `DECISIONS_OPEN.md` | the short list of things only you can decide |

Code:

| | |
|---|---|
| `../../health-apps/sleep-noise/index.html` | the whole app — UI, sound generation, both audio backends. One file, one source of truth |
| `ios/App/App/Native/*.swift` | the native audio engine and plugins (4 files) |
| `../../health-apps/sleep-noise/test/` | two test suites, runnable on any machine |
| `scripts/build-www.mjs` | copies the web app into the native project |
| `scripts/render-assets.mjs` | regenerates the app icon and launch artwork |

## 5. Prove it still works, right now

On any machine with Node, from the repository root. Neither needs a Mac:

```bash
node health-apps/sleep-noise/test/regression.mjs      # 132 checks, the web app
node health-apps/sleep-noise/test/native-bridge.mjs   # 67 checks, the native path
```

If both pass, nothing has rotted. If the second one fails, the JavaScript side
of the native bridge has been broken by an edit to `index.html`.

The tests need Playwright and a Chromium. If it is not installed:

```bash
npm install -g playwright && npx playwright install chromium
```

## 6. Git

```
main                       the live website, untouched by this work
ios-sleeper-agent          ← everything described here
claude/sleep-noise-mixer-presets-c0oafq   the five-fader mixer (open as PR #26)
```

`ios-sleeper-agent` was branched from the mixer work, so it contains the mixer
too. If PR #26 is merged first, this branch will merge cleanly after it.

Nothing here has been merged to `main`, so the live website is exactly as it
was, apart from nothing. The privacy and support pages exist only on this
branch until it is merged — **which matters, because the App Store needs those
two URLs to be live.** See `SHIPPING_GUIDE.md` step 8.

## 7. Picking this up with an AI assistant later

Point it at these files, in this order, and it will have the whole picture:

```
ios-apps/sleeper-agent/START_HERE.md
ios-apps/sleeper-agent/SHIPPING_GUIDE.md
health-apps/sleep-noise/IOS_PORT_NOTES.md
ios-apps/sleeper-agent/README-iOS.md
health-apps/sleep-noise/IOS_QA_CHECKLIST.md
```

A prompt that works:

> I have a half-finished iOS port in this repository, on the branch
> `ios-sleeper-agent`. Read `ios-apps/sleeper-agent/START_HERE.md` and the
> files it points to before doing anything. The Swift in
> `ios-apps/sleeper-agent/ios/App/App/Native/` has never been compiled. I am
> now at [step N of SHIPPING_GUIDE.md] and here is what happened: [paste].

The commit messages on this branch are unusually detailed on purpose —
`git log ios-sleeper-agent` explains not just what changed but why, including
the reasoning behind decisions that would otherwise look arbitrary.

## 8. Things that are true and easy to forget

- **The web version and the app are the same file.** Editing
  `health-apps/sleep-noise/index.html` changes both. That is deliberate; two
  copies would drift.
- **`npm run build` will refuse** if that file loses something the native
  build needs, or gains a network request that would break offline use. If it
  refuses, read the message before changing the guard.
- **Do not rewrite the sound generation in Swift.** The generators are the
  product, and the test suite measures the exact code that ships.
- **The loop cache lives in `Caches/`** so iOS may delete it. That is intended;
  the app regenerates.
- **Interruptions do not auto-resume**, deliberately. Sound restarting by
  itself at 3 a.m. is worse than sound not restarting.
- **Never commit signing material.** `.gitignore` already covers `*.p12`,
  `*.mobileprovision`, `AuthKey_*.p8` and the rest.
