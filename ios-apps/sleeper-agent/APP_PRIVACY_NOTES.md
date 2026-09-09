# Sleeper Agent — App Privacy notes

What to answer in App Store Connect's **App Privacy** questionnaire, and the
evidence for each answer.

**You must make these declarations yourself.** They are legally meaningful and
Apple treats them as commitments. Nothing here has been submitted. What follows
is a reading of the code, offered so that you can check it rather than take it
on trust.

---

## 1. The short answer

> **Data Not Collected.**

Answer **No** to "Do you or your third-party partners collect data from this
app?" and the questionnaire ends there. The listing will show
*Data Not Collected*, and the app will need no privacy "nutrition label"
entries at all.

## 2. Why that is true, and how to check

Apple's definition of *collect* is "transmitting data off the device". Data
that never leaves the device is not collected. This app cannot transmit
anything, because it has no code that talks to a network.

Four things are worth verifying yourself before you sign anything:

**No network code.** Search the shipped web app for every way a browser can
reach the network:

```bash
grep -nE 'https?://|fetch\(|XMLHttpRequest|new WebSocket|sendBeacon|EventSource|importScripts' \
  health-apps/sleep-noise/index.html
```

The only matches are three `<link>` tags pointing at files in the same
directory (`manifest.webmanifest`, `icon.svg`). No remote URL appears anywhere.

**No third-party SDKs.** The app's entire dependency list:

```bash
cd ios-apps/sleeper-agent && npm ls --depth=0
```

`@capacitor/core`, `@capacitor/cli`, `@capacitor/ios`. Capacitor is the native
shell; it is not an analytics or advertising framework and collects nothing on
its own. There is no Firebase, no Crashlytics, no Sentry, no AppsFlyer, no
Adjust, no Facebook SDK, no advertising identifier, no attribution SDK.

**No permissions.** `Info.plist` contains no `NS*UsageDescription` key of any
kind, because the app asks for nothing: no microphone, camera, location,
notifications, contacts, photos, calendar, Bluetooth or HealthKit. An app that
requests no permissions cannot read anything about the person using it.

**A build-time guard.** `scripts/build-www.mjs` refuses to produce the native
bundle if `index.html` ever gains a remote script, a remote font, a network
call, or an analytics or beacon call. This is not merely a promise about today;
it fails the build if someone changes it.

**And confirm on the device.** `IOS_QA_CHECKLIST.md` §7 covers the empirical
version: the whole app used in airplane mode, and Xcode's network instrument
showing no outbound connections during a session.

## 3. What the app stores, and where

Locally, on the device, and nowhere else:

| What | Where | Why |
|---|---|---|
| Settings — the five fader levels, volume, tone, swell, timer preference, fade lengths, ambient theme and level, blackout preference | WebView `localStorage`, key `brokild-sleep-noise`, mirrored into `UserDefaults` | so the app opens as you left it |
| Rendered audio loops | `Caches/sleeper-loops/*.wav` | so starting is instant after the first time |

Both are the user's own settings and the app's own generated audio. Neither
contains a name, an email address, an identifier, a location, a timestamp, a
usage history, or anything derived from the person. Deleting the app removes
both. Neither is synced, uploaded, or backed up anywhere off the device.

The `UserDefaults` mirror exists because WKWebView's local storage can be
evicted under storage pressure; it holds the same settings JSON and nothing
more. The loop cache is deliberately in `Caches/` rather than `Documents/`
precisely so that iOS may delete it — it is reproducible data, and it is not
included in device backups.

## 4. Questions Apple asks that are easy to get wrong

| Question | Answer | Why |
|---|---|---|
| Do you collect data? | **No** | nothing is transmitted; §2 |
| Do you use data for tracking? | **No** | nothing is collected, and there is no ATT prompt or IDFA |
| Do you use third-party analytics? | **No** | §2, dependency list |
| Do you show advertising? | **No** | no ad SDK, no ad code |
| Do you collect Diagnostics / crash data? | **No** | no crash-reporting SDK. (Apple's *own* crash reports, if the user has opted into sharing them with developers, are not app collection and are not declared here) |
| Identifiers — User ID, Device ID? | **No** | none generated, none read |
| Product Interaction / Usage Data? | **No** | the settings never leave the device, so they are not collected |
| Health & Fitness data? | **No** | no HealthKit, no sleep tracking, no sensors. The app makes sound; it measures nothing |
| Purchases? | **No** | no in-app purchases. A paid-up-front app's purchase is Apple's transaction, not the app's data collection |
| Contact Info? | **No** | no account, no sign-in, no contact form in the app |
| Do you use the AdvertisingIdentifier? | **No** | AdSupport is not linked |

If you later add anything that reports to a server — analytics, crash
reporting, a newsletter form — these answers stop being true and must be
updated **before** that build is submitted.

## 5. Privacy manifest and required-reason APIs

The app uses no API from Apple's *required reason* list (no file timestamps,
no disk space APIs, no active-keyboard APIs, no user-defaults access from a
third-party SDK). It links no third-party SDK that requires a signature.

So no `PrivacyInfo.xcprivacy` is required for the app itself. Capacitor ships
its own manifest for its own framework where required; nothing needs adding by
hand. Confirm at validation time — Xcode Organizer's *Validate App* step
reports any missing privacy manifest, and `IOS_QA_CHECKLIST.md` §12.11 covers
it.

`ITSAppUsesNonExemptEncryption` is set to `false` in `Info.plist`. This is
truthful — the app contains and uses no encryption — and it removes the export
compliance question from every upload.

## 6. The published privacy policy

`health-apps/sleep-noise/privacy.html`, which will be served at:

```
https://peterboggild.github.io/BrokildApps/health-apps/sleep-noise/privacy.html
```

It was written to describe what the code actually does rather than to be a
generic template, and it says plainly that nothing is collected. **Read it
before you point Apple at it** — you are the one making the representation,
and a privacy policy that overclaims is worse than none.

One thing to check: it names no contact address. Apple requires a Support URL
where a person can reach you, so decide what address you are willing to
publish. If you want, I will add it to both the privacy page and a matching
`support.html`.

## 7. What would change these answers

Recorded so the next person knows what is load-bearing:

- Adding any analytics, crash reporting or advertising SDK.
- Adding an account, sign-in, or any server component.
- Adding remote configuration, or a "check for updates" call.
- Adding iCloud or CloudKit sync of settings.
- Adding HealthKit — writing sleep data would make this a Health & Fitness
  data collector and change the review posture considerably.
- Adding in-app purchases or subscriptions.
- Loading any resource over the network, including a web font.

The build guard in `scripts/build-www.mjs` catches the last one automatically.
The rest need a person to notice, which is what this section is for.
