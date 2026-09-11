# Shipping without owning a Mac

Answering the question directly: **can an iPad do this?**

| | |
|---|---|
| Run and test the app on an iPad | **Yes** — the app supports iPad, and TestFlight installs on it |
| Do all the App Store paperwork from an iPad | **Yes** — enrolment, listing, pricing, privacy, submission, TestFlight |
| **Build and upload the app from an iPad** | **No.** There is no Xcode for iPadOS, and the one thing that comes close cannot open this project |
| Ship the whole app without ever touching a Mac | **Yes, actually** — see §3 |

---

## 1. Why an iPad cannot build it

Apple has never shipped Xcode for iPadOS. The nearest thing is **Swift
Playgrounds**, which genuinely can build an app on an iPad and upload it
straight to App Store Connect — so the idea is not fantasy. But it only works
with its own project format, supports SwiftUI apps, and offers [a limited
subset of entitlements](https://teaandtechtime.com/building-and-publishing-an-ios-app-with-your-ipad-for-real/):
no extensions, no widgets, no in-app purchases.

It cannot open a standard `.xcodeproj`, which is what this project is. It
cannot resolve the Capacitor Swift package this app is built on. It has no way
to add the four custom Swift plugin files, or to configure the background-audio
capability that is the entire point of the port.

So: not this app, not now, not with a change of a few settings.

## 2. What an iPad *can* do

Rather more than you might expect. Cross-referenced against
`SHIPPING_GUIDE.md`:

| Step | On an iPad? |
|---|---|
| 2. Apple Account and two-factor | **Yes** |
| 9. Enrol in the Developer Program | **Yes** — the *Apple Developer* app on iPad handles enrolment |
| 10. Paid Apps Agreement, tax, banking | **Yes** — App Store Connect in Safari |
| 11. Create the app record | **Yes** |
| 12. Listing copy, privacy questionnaire, age rating | **Yes** |
| 13. Screenshots | **Yes** — but see the note below |
| 16. TestFlight testing | **Yes** — install and test on the iPad itself |
| 17. Submit for review | **Yes** |
| 5–7, 14, 15. Build, sign, archive, upload | **No** |

**Screenshots:** you need iPhone-sized screenshots, so an iPad screenshot will
not do for the required 6.9-inch slot. But a screenshot taken on a real iPhone
running the app is exactly the right pixel size and is perfectly acceptable —
so once the app is on your phone via TestFlight, your phone is your screenshot
machine, no simulator required. If you declare iPad support, an iPad
screenshot covers the iPad slot.

## 3. The route that needs no Mac at all

Only the *build* needs macOS. It does not need macOS **that you own**.

GitHub gives free macOS runner minutes to public repositories, and
`peterboggild/BrokildApps` is public. A workflow can therefore compile, sign
and upload the app on a rented-by-the-minute Mac that exists for ten minutes
and is then destroyed — [a well-travelled route](https://dev.to/capawesome/how-to-build-and-deploy-ios-apps-without-owning-a-mac-2cbb).

That workflow is written and committed: **`.github/workflows/sleeper-agent-ios.yml`**.

```
  iPad or any browser                GitHub Actions (macOS)          Your iPhone
  ───────────────────                ──────────────────────          ───────────
  edit / approve  ──── git push ───►  runs the test suites
  Actions ▸ Run workflow              builds www from index.html
                                      compiles the Swift
                                      signs with your certificate
                                      uploads to TestFlight  ────►  install & test
                                                                     overnight
  App Store Connect ◄──────────────── build appears
  (metadata, submit)
```

Every box on the left is a browser. Nothing requires a Mac in your hands.

### The honest catches

**You must pay the $99 first.** TestFlight requires paid membership, so the
"try it free on your own phone before spending anything" ordering in
`SHIPPING_GUIDE.md` step 7 is not available on this route. You would be paying
before knowing whether the app plays through a night.

**The first build will be slow going.** The Swift here has never been
compiled. On a Mac you fix an error and rebuild in seconds. Through CI it is
git commit, push, wait ten minutes, read the log, repeat — and there may well
be a dozen rounds. That is hours of dead time rather than an afternoon of work.

**You cannot use the Xcode debugger.** When something misbehaves on the phone
at 3 a.m., the device console is how you find out why, and that needs a Mac.
`IOS_QA_CHECKLIST.md` §4 asks you to capture exactly that if background audio
fails.

### So the recommendation

**Borrow a Mac for one afternoon**, purely to get it compiling and to run the
first device test. Then use the CI workflow from your iPad for everything
afterwards — every update, every upload, indefinitely.

That gets you the fast feedback loop exactly where it is worth most (the first
compile, and the first night of real testing) and costs you nothing thereafter.

If no Mac is available at all, the CI route genuinely works. Start with
`mode: build-only`, which needs no secrets and no money, and just tells you
whether it compiles.

---

## 4. Making the six secrets, without a Mac

Signing material is traditionally made with Keychain Access on a Mac. It does
not have to be: a certificate request is just a file, and `openssl` makes one
on any operating system.

You need `openssl` (built into macOS and Linux; on Windows use WSL or Git
Bash). Everything else is a browser.

### 4.1 A private key and a certificate request

```bash
openssl genrsa -out ios_distribution.key 2048

openssl req -new -key ios_distribution.key -out ios_distribution.csr \
  -subj "/emailAddress=brokildapps@gmail.com/CN=Sleeper Agent Distribution/C=DK"
```

**Keep `ios_distribution.key`.** Losing it means starting this section again.
Do not commit it — `.gitignore` already refuses `*.p12` and friends, but this
`.key` is worth putting somewhere safe and private.

### 4.2 Turn it into a certificate

1. [developer.apple.com/account/resources/certificates](https://developer.apple.com/account/resources/certificates)
   → **+**
2. Choose **Apple Distribution**.
3. Upload `ios_distribution.csr`.
4. Download the resulting `distribution.cer`.

Convert it to the `.p12` bundle CI needs, choosing any export password:

```bash
openssl x509 -inform DER -in distribution.cer -out distribution.pem
openssl pkcs12 -export -legacy \
  -inkey ios_distribution.key -in distribution.pem \
  -out distribution.p12 -name "Apple Distribution"
```

> If `-legacy` is rejected, drop it — it is only needed on OpenSSL 3.x to
> produce a bundle older tooling can read.

Then base64 it, which is the form a GitHub secret takes:

```bash
base64 -w0 distribution.p12 > distribution.p12.base64   # Linux
base64 -i distribution.p12 -o distribution.p12.base64   # macOS
```

### 4.3 App ID and provisioning profile

1. [Identifiers](https://developer.apple.com/account/resources/identifiers) →
   **+** → **App IDs** → **App**.
   - Description: `Sleeper Agent`
   - Bundle ID: **Explicit**, `com.peterboggild.sleeperagent`
   - Capabilities: **leave everything unticked.** Background audio is declared
     in `Info.plist` and needs no App ID capability. Ticking things you do not
     use invites review questions.
2. [Profiles](https://developer.apple.com/account/resources/profiles) → **+** →
   **App Store Connect** → pick the App ID → pick the certificate you just
   made → name it → download the `.mobileprovision`.

```bash
base64 -w0 profile.mobileprovision > profile.base64
```

### 4.4 An App Store Connect API key

This is what lets CI upload without your Apple password.

1. App Store Connect → **Users and Access** → **Integrations** →
   **App Store Connect API** → **+**
2. Access: **App Manager**.
3. Download the `.p8`. **It can only be downloaded once.**
4. Note the **Key ID** and the **Issuer ID** shown on that page.

Your **Team ID** is on
[developer.apple.com/account](https://developer.apple.com/account) under
Membership — ten characters.

### 4.5 Put them in GitHub

Repository → **Settings** → **Secrets and variables** → **Actions** →
**New repository secret**, six times:

| Secret | Value |
|---|---|
| `IOS_DIST_CERT_P12_BASE64` | contents of `distribution.p12.base64` |
| `IOS_DIST_CERT_PASSWORD` | the export password from §4.2 |
| `IOS_PROVISIONING_PROFILE_BASE64` | contents of `profile.base64` |
| `APPLE_TEAM_ID` | the ten-character Team ID |
| `APPSTORE_API_KEY_ID` | Key ID from §4.4 |
| `APPSTORE_API_ISSUER_ID` | Issuer ID from §4.4 |
| `APPSTORE_API_PRIVATE_KEY` | the **entire** `.p8` file, `-----BEGIN` line and all |

GitHub secrets cannot be read back once saved, and are masked in logs. The
workflow deletes the keychain and the key file at the end of every run.

---

## 5. Running it

Repository → **Actions** → **Sleeper Agent iOS (TestFlight)** → **Run
workflow**. This works fine in Safari on an iPad.

**First, always: `mode: build-only`.** No secrets, no money, no upload. It runs
both test suites, builds `www/` from the canonical `index.html`, and compiles
the Swift. If it fails, the log tells you what — and the errors are the same
ones a Mac would give you.

Hand them to an AI assistant like this:

> The GitHub Actions run of `.github/workflows/sleeper-agent-ios.yml` failed
> compiling the Swift in `ios-apps/sleeper-agent/ios/App/App/Native/`. That
> code has never been compiled. Read
> `ios-apps/sleeper-agent/START_HERE.md` first, then fix these errors:
>
> [paste the log]

Once `build-only` is green, run it again with `mode: testflight` and a
`build_number` higher than any you have uploaded before. Ten to twenty minutes
later the build appears in App Store Connect under TestFlight, and you install
it on your phone.

---

## 6. Two things that were fixed to make this possible

**A shared scheme.** Xcode writes its scheme into `xcuserdata/`, which is
git-ignored, so a build machine that has never had Xcode's UI opened on it
finds no scheme at all and `xcodebuild -scheme App` fails before compiling
anything. `ios/App/App.xcodeproj/xcshareddata/xcschemes/App.xcscheme` is
committed for exactly this reason. Do not delete it.

**The workflow is manual-dispatch only.** It never runs on push. Every upload
consumes a build number Apple will not accept twice, and a half-finished commit
should not become a TestFlight build.

---

## 7. This workflow has never been run

Like the Swift it builds, it was written on Linux with no way to test it.
Expect to fix it. What follows is where to look, with the symptom you would
actually see in the log.

Three of these can only bite in `mode: testflight`, which is another reason to
run `build-only` first — that path has just two places to go wrong.

### Can fail in `build-only`

**1. Swift Package resolution.** Capacitor 8 uses SPM, so the first run has to
fetch `capacitor-swift-pm` from GitHub. *Symptom:* "failed to resolve
dependencies", a network timeout, or a prompt about a package plugin that CI
cannot answer. `-skipPackagePluginValidation` is already passed for the last of
those. *If it persists:* add a separate step before the compile —
`xcodebuild -resolvePackageDependencies -project "$XCODE_PROJECT"` — so
resolution fails on its own line instead of inside the build.

**2. The Swift itself.** By far the likeliest, and the whole reason
`build-only` exists. *Symptom:* ordinary Swift compile errors in
`ios/App/App/Native/`. *Fix:* they are the same errors a Mac would give you;
paste the log into an assistant with the prompt in §5.

### Can only fail in `testflight`

**3. `security set-key-partition-list`.** The single most common way signing
breaks in CI. *Symptom:* `codesign` hangs until the job times out, or reports
*"User interaction is not allowed"* or `errSecInternalComponent`. *Cause:* the
imported key is in the keychain but nothing is authorised to use it without a
UI prompt. That line is what grants access; if it is failing, check the
keychain password matches the one used to create it.

**4. Bundle identifier mismatch.** It has to be the same in three places: the
App ID you registered, the provisioning profile built from it, and
`PRODUCT_BUNDLE_IDENTIFIER`. *Symptom:* "No profile for team ... matching
'...' found", or a signing error naming the profile rather than the real
cause. *Fix:* check all three read `com.peterboggild.sleeperagent`.

**5. The App Store Connect upload.** *Symptom:* `altool` reports an
authentication failure, or cannot find the key. *Cause:* the `.p8` has to be at
`~/.appstoreconnect/private_keys/AuthKey_<KEY_ID>.p8` with the Key ID in the
filename matching `APPSTORE_API_KEY_ID` exactly; the workflow writes it there,
but a secret pasted with a missing `-----BEGIN` line or stray whitespace fails
here rather than earlier. *Also:* a build number Apple has already seen is
rejected at this step, not before it.

### Already removed

Three more were in this list when it was written, and were fixed by re-reading
the workflow rather than left as warnings:

- The compile step piped through `xcpretty` under `set -o pipefail`, so a
  **real compile error** failed the pipeline and triggered the fallback
  rebuild — doubling the slowest step and printing every error twice. It now
  runs plain `xcodebuild`, which is also the output you want when diagnosing
  Swift that has never been compiled.
- `agvtool` was called from the project root, but it has to run from the
  directory holding the `.xcodeproj`, so it could only ever have failed. The
  build number is now set by `PlistBuddy` alone, from the right path, and the
  step prints what it set.
- `base64 --decode -o` used the BSD-only output flag, which a GNU coreutils
  `base64` earlier in `PATH` would reject. It is a redirect now.

## Sources

- [How to Build and Deploy iOS Apps Without Owning a Mac](https://dev.to/capawesome/how-to-build-and-deploy-ios-apps-without-owning-a-mac-2cbb)
- [How to build and ship an iOS app without a Mac](https://dev.to/maclessdev/how-to-build-and-ship-an-ios-app-without-a-mac-17o5)
- [Building and Publishing an iOS App with your iPad for real](https://teaandtechtime.com/building-and-publishing-an-ios-app-with-your-ipad-for-real/)
- [Upload builds — App Store Connect Help](https://developer.apple.com/help/app-store-connect/manage-builds/upload-builds/)
