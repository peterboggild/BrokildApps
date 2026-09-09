# Sleeper Agent — App Store material (draft)

Everything here is a **draft for your approval**. Nothing has been submitted.
Wording, pricing, category and every legally meaningful declaration are yours
to make in App Store Connect.

All copy below has been written to avoid health and medical claims. See §9 for
what was deliberately not said, and why.

---

## 1. Identity

| Field | Value | Note |
|---|---|---|
| App name | **Sleeper Agent** | 14 chars, limit 30 |
| Subtitle | **Mix your own sleep sound** | 25 chars, limit 30 |
| Bundle ID | `com.peterboggild.sleeperagent` | **provisional — confirm before registering** |
| Version | 1.0.0 | |
| Primary language | English (UK) | the copy is British spelling |
| Copyright | `2026 Peter Boggild` | App Store Connect wants no © symbol |

**Bundle identifier is irreversible.** Once registered with Apple it cannot be
changed or reused. Confirm the spelling before it is created.

Subtitle alternatives if you prefer:

- `Brown noise, rain and ocean` (27)
- `A mixer for sleeping sound` (26)
- `Sleep sound, mixed by you` (25)

## 2. Category

**Recommended primary: Health & Fitness.** It is where sleep-sound apps live
and where people look for them.

**Secondary: Utilities.**

The trade-off, stated plainly: Health & Fitness attracts more scrutiny of
health claims. Since the app makes none (§9), that scrutiny should cost
nothing — but if you would rather sidestep the question entirely, **Utilities**
as primary is defensible and loses only some discoverability. `Info.plist`
currently declares `public.app-category.healthcare-fitness`; App Store Connect
overrides it, so changing your mind later is free.

## 3. Promotional text (170 max)

Editable any time without a new build — use it for what is new.

> Five sounds on five faders: brown noise, green noise, rain, soft rain and
> ocean. Lay them over each other, set a sleep timer, and let it fade out on
> its own.

*(163 characters)*

## 4. Description

```
Sleeper Agent is a mixer, not a playlist.

Five sounds sit on five faders — brown noise, green noise, rain, soft rain
and ocean — and they play together in any combination you like. Lay soft
rain over brown noise. Put a little surf under green. Run one on its own.
The mix is yours, and it stays exactly where you left it.

Five presets to start from:

Deep — brown noise alone, rolled off dark.
Rainroom — brown under soft rain, with a slow minor synth theme far behind.
Forest — green noise with a light rain through it.
Shore — surf on its own swell, brown noise filling in underneath.
Drift — quiet green and far-off surf under a brighter theme.

Move any fader afterwards and the mix becomes your own.

FIVE SLOW SYNTH THEMES

Above the noise, on a fader of its own, sits an ambient synth: five slow
chord themes from plain major to dark minor. Each is built from two loops
of different length, so the music does not repeat for about twelve minutes.
Meant for lying awake on purpose rather than for sleeping through — anything
melodic holds the attention more than plain noise does.

A SLEEP TIMER THAT LETS GO GENTLY

Play for anything from fifteen minutes to twelve hours, or stop at a clock
time. The sound fades in when it starts and sinks away slowly before it
stops, so the silence does not wake you. Or set no limit at all.

A SCREEN THAT GOES DARK

Blackout mode turns the display truly black — no status bar, no glow on the
bedside table — with a dim clock you can read if you wake. It goes dark by
itself after a minute if you leave it alone.

THE REST OF THE PANEL

Tone rolls off the top end: darker is gentler over a whole night, brighter
masks more speech and traffic. Swell adds a very slow rise and fall across
the whole mix, like breathing surf. Fade in and fade out are adjustable
separately.

MADE THE UNFASHIONABLE WAY

Every sound is generated on your phone, sample by sample, as it plays.
Nothing is streamed and nothing is downloaded, so it works in airplane mode
on a plane, in a tent, or anywhere with no signal at all.

There is no account. There is no subscription. There is no advertising, no
analytics and no tracking of any kind. The app makes no network connections
whatsoever — not one — and collects nothing about you, because it never
sends anything anywhere.

You buy it once and it is yours.
```

*(~2,050 characters of 4,000.)*

## 5. Keywords (100 characters, comma-separated, no spaces)

```
noise,brown,white,rain,ocean,sleep,timer,mixer,ambient,offline,focus,tinnitus,masking,fan,surf
```

*(95 characters.)*

**One keyword to decide about: `tinnitus`.** People with tinnitus genuinely
search for sound maskers, so it is a real term of intent rather than a claim —
and a keyword is not visible copy. But it is adjacent to a medical condition,
and the description quite deliberately never mentions it. If you would rather
keep the listing entirely clear of medical vocabulary, drop it and use the
characters for `pink` and `green`:

```
noise,brown,white,pink,green,rain,ocean,sleep,timer,mixer,ambient,offline,focus,masking,surf
```

*(91 characters.)* My recommendation is this second version — the app does not
need the word, and consistency between keywords and copy is worth more than
one search term.

## 6. What's New in This Version

For 1.0.0:

```
First release.
```

## 7. Support and marketing URLs

| Field | Value | Status |
|---|---|---|
| Support URL | `https://peterboggild.github.io/BrokildApps/health-apps/sleep-noise/support.html` | created on this branch |
| Marketing URL | `https://peterboggild.github.io/BrokildApps/health-apps/sleep-noise/` | exists (the web version) |
| Privacy Policy URL | `https://peterboggild.github.io/BrokildApps/health-apps/sleep-noise/privacy.html` | created on this branch |

A Support URL is **required**, and it has to be a page a human can actually
use. `support.html` is written and published on this branch: the contact
address is `brokildapps@gmail.com`, and above it sit twelve answers to the
questions this app will actually generate — nothing heard on play, sound
stopping in the night (nearly always the sleep timer, or an interruption the
app deliberately does not auto-resume from), the fade-in catching people out,
battery, and why the synth themes keep you awake on purpose.

A reviewer will open this URL. A page with real content on it reads better
than a bare mailto, and it is also the page that answers the support email
before it is sent.

Also worth knowing: the app now prints its version and build at the very
bottom of its own screen, on iOS only. Nothing else in the app said what
version it was, which made a useful bug report impossible. The support page
asks for that line.

## 8. Age rating

Expect **4+**. In the questionnaire, every content category should be *None*:
no violence, no profanity, no sexual content, no gambling, no drug references,
no horror, no contests, no unrestricted web access, no user-generated content,
no messaging.

Two questions that catch people out, and the honest answers here:

- *Does the app include third-party advertising?* **No.**
- *Does the app have unrestricted web access?* **No** — the app has no browser
  and makes no network requests at all.

## 9. Health claims — what was deliberately not said

The app is a sound generator. It is not a treatment, and the listing must not
imply that it is.

**Not used anywhere, on purpose:** insomnia, tinnitus (except the keyword
question in §5), anxiety, ADHD, sleep disorder, sleep apnoea, cure, treat,
therapy, therapeutic, clinically proven, doctor-recommended, improves sleep
quality, helps you sleep better, reduces stress.

**Used instead:** what the app *does* — generates sound, mixes it, masks
traffic and snoring, fades out on a timer. Description of function, not
promise of outcome.

An audit of the existing app copy found no claims to walk back: the only greps
that hit were the string `health-apps` inside a file path and the word
`treats` inside a code comment about the media element.

If you ever want to say something stronger, that becomes a regulatory
decision rather than a copywriting one, and should be made deliberately.

## 10. Screenshots

Required: **6.9″ iPhone** (1290 × 2796 portrait). Strongly advised: **6.5″**
(1242 × 2688). If you declare iPad support — the build does — a **13″ iPad**
size is required too. No transparency; PNG or JPEG.

Five frames, in this order:

| # | Shows | Caption |
|---|---|---|
| 1 | The mixer, Rainroom preset loaded, brown and soft rain up, timer at 8 h | *Five sounds. One mix.* |
| 2 | The five preset chips with Shore lit and its description | *Five ways in — then make it yours* |
| 3 | Sleep timer: "Play for" with 8 h selected, fade rows visible | *Fades out on its own* |
| 4 | Blackout screen with the dim clock | *A screen that goes truly dark* |
| 5 | Ambient synth section, Rainroom lit, level fader | *Five slow synth themes over the top* |

Frame 5 could instead carry the offline/privacy message over frame 1's
mixer — if you want that argued visually rather than only in the description,
say so and I will lay out an alternative.

**These must be captured on a Mac**, in the simulator at the exact sizes
above, from the real build. I cannot produce them here, and I will not
fabricate screenshots: every frame listed is a state the app genuinely has,
but the pixels have to come from the app.

## 11. App Review contact and notes

App Review Information also asks for a contact for the reviewer, separate from
the public Support URL. `brokildapps@gmail.com` serves for both unless you
would rather Apple had a different one. A phone number is required in that
form too, and I have not guessed at it.

### Notes

Paste into *App Review Information → Notes*:

```
No account, no sign-in and no demo credentials are needed. Every feature is
available immediately on first launch.

WHAT THE APP DOES
Sleeper Agent generates sleep sound on the device. Five noise and weather
sounds (brown noise, green noise, rain, soft rain, ocean) are synthesised in
JavaScript sample by sample, then played by a native AVAudioEngine. An
optional ambient synth layer of five chord themes is generated the same way.
There are no audio files in the bundle and no streamed audio.

BACKGROUND AUDIO
The app declares UIBackgroundModes: audio. This is core functionality, not an
accessory to it: the app is a sleep-sound generator, so it must keep playing
with the screen locked for the length of a night. Playback only ever begins
from an explicit tap on the play control; the app never starts audio on its
own, and never activates the audio session merely by being opened.

SLEEP TIMER
Playback is normally bounded by a sleep timer ("play for" a duration, or
"stop at" a clock time), which fades the sound out, stops it, and releases
the audio session. An unlimited mode is also available.

NETWORK
The app makes no network requests of any kind. It works fully in airplane
mode, which is the intended use. There is no analytics, advertising,
crash-reporting or tracking SDK, and no remote configuration.

PERMISSIONS
The app requests no permissions at all — no microphone, camera, location,
notifications, contacts, photos or health data.

DATA
The only data stored is the user's own settings (fader levels, timer
preference, tone), kept on the device in local storage and UserDefaults.
Nothing is collected and nothing leaves the device.

FIRST LAUNCH
The first time a given sound is used, the app renders its 20-second loop and
caches it on the device. This takes a few seconds and shows a "Preparing…"
message. Later starts are immediate. This is the app generating audio, not
downloading it.

ARCHITECTURE
The interface is a WKWebView over a native audio engine. This is not a web
page in a wrapper: the bundle is entirely self-contained and offline, and the
audio engine, sleep timer, fades, Now Playing integration and interruption
handling are all native (AVAudioSession, AVAudioEngine,
MPNowPlayingInfoCenter, MPRemoteCommandCenter).
```

The last paragraph is there for guideline **4.2 Minimum Functionality**,
which is the most likely thing to be raised. Answer it in the notes rather
than waiting to be asked.

## 12. Pricing — a recommendation, not a decision

**Recommended: paid once, no subscription, no in-app purchase, no ads.**

This removes a startling amount of complexity — no receipt validation, no
StoreKit, no restore-purchases flow, no subscription lifecycle, no paywall
copy — and it is consistent with the product's actual argument, which is that
it does nothing behind your back. It also keeps the privacy answers trivially
true.

For a first release, somewhere around **€4.99** (Apple tier equivalent) is a
reasonable place to start: above the impulse tier that invites one-star
"it's just noise" reviews, below where people expect a subscription and a
library of recordings.

For context, sleep-noise apps cluster at free-with-ads, free-with-subscription,
or paid once in roughly the €2–€8 range. A paid-once app in that band, with no
account and no tracking, is a clear position.

**You set the price in App Store Connect.** I have not chosen one, and the
figure above is a starting point for you to move.

## 13. Availability

Suggested: all territories. There is nothing region-specific — no content, no
network use, no legal exposure that varies by market.

The app is in English only. Localisation is a later decision; if you do
localise, the description is the part that matters — the interface is nearly
wordless apart from its explanatory notes.

## 14. Release strategy

Suggested: **manual release** after approval, rather than automatic. It costs
nothing, and it means an approval at an awkward moment does not put the app
live before you have looked at it.

---

## 15. Before submission — what is still open

Human decisions, roughly in the order they come up:

1. Confirm the bundle identifier (§1) — irreversible once registered.
2. Choose the category (§2).
3. Decide the `tinnitus` keyword question (§5).
4. Approve or rewrite the description (§4).
5. ~~Support URL~~ — done: `support.html`, contact `brokildapps@gmail.com`.
6. Apple Developer Program membership — see `README-iOS.md` §7.
7. Paid Apps Agreement, tax and banking details in App Store Connect.
8. Set the price (§12).
9. Capture the screenshots on a Mac (§10).
10. Complete the age-rating questionnaire (§8) and the App Privacy answers
    (`APP_PRIVACY_NOTES.md`).
11. Work `IOS_QA_CHECKLIST.md` — at minimum §4, which is the product.
