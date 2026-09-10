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
Expect to fix it. The likely trouble spots, in the order I would suspect them:

1. **Swift Package resolution.** Capacitor 8 uses SPM; the first CI run has to
   fetch `capacitor-swift-pm`, and `-skipPackagePluginValidation` is there
   because package plugins otherwise prompt for confirmation that CI cannot
   give.
2. **`xcpretty` may not be installed** on the runner. The compile step falls
   back to plain `xcodebuild` if the piped version fails, which is ugly but
   works.
3. **`agvtool`** needs to run from the directory containing the project and can
   be fussy; the build-number step is `continue-on-error` so it cannot fail the
   run. If the build number does not take, set it by editing `Info.plist`
   directly and committing.
4. **`security set-key-partition-list`** is the step that most often breaks
   signing in CI. If codesign hangs or reports "user interaction is not
   allowed", that is the line to look at.
5. **`base64 --decode -o`** is the macOS spelling. It is correct here because
   this step runs on the macOS runner, but it would need `-d >` on Linux.
6. **The bundle identifier must match** in three places: the App ID you
   registered, the provisioning profile, and `PRODUCT_BUNDLE_IDENTIFIER`. A
   mismatch produces a signing error that does not name the real cause.

## Sources

- [How to Build and Deploy iOS Apps Without Owning a Mac](https://dev.to/capawesome/how-to-build-and-deploy-ios-apps-without-owning-a-mac-2cbb)
- [How to build and ship an iOS app without a Mac](https://dev.to/maclessdev/how-to-build-and-ship-an-ios-app-without-a-mac-17o5)
- [Building and Publishing an iOS App with your iPad for real](https://teaandtechtime.com/building-and-publishing-an-ios-app-with-your-ipad-for-real/)
- [Upload builds — App Store Connect Help](https://developer.apple.com/help/app-store-connect/manage-builds/upload-builds/)
