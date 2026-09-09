# Getting Sleeper Agent onto the App Store

A step-by-step guide written for someone who has never shipped an iOS app.

Work through it in order. Each step says what to do, why, and what you should
see when it worked. Nothing is skipped as "obvious".

> **A standing caveat.** Apple changes prices, menu locations, required
> screenshot sizes and enrolment rules regularly, and this was written in
> September 2026. Where a step says *"verify on Apple's page"*, do that rather
> than trusting the number here. The official pages are
> [developer.apple.com/programs](https://developer.apple.com/programs/) and
> [appstoreconnect.apple.com](https://appstoreconnect.apple.com).

---

## What this will cost you

| | |
|---|---|
| Apple Developer Program | **$99 / year** (or local equivalent). Required to put anything on the App Store or TestFlight. Recurring — the app is removed if you stop paying |
| A Mac | See step 1. Can be free |
| An iPhone | You presumably have one. Needed for the tests that matter |
| Apple's cut of each sale | **30%**, or **15%** if you join the Small Business Program (step 10) — do not skip that |
| Everything else | Nothing. No hosting, no server, no domain |

**Time:** an afternoon to find out whether it works. Then a week or two of
evenings, most of which is waiting — for Xcode to download, for enrolment to
be approved, for review.

---

## Step 1 — Get to a Mac

Building, signing and uploading an iOS app can only happen on macOS with
Xcode. Apple permits no alternative. Everything else in this project was
deliberately done on Linux to get as far as possible without one.

Your options, honestly compared:

**Borrow one.** Best first move. You need it for perhaps three sessions: the
first build, device testing, and the upload. A friend's or a colleague's Mac
for an afternoon is enough to find out whether the app works at all — which
you want to know before spending anything.

**Rent one in the cloud.** MacStadium, MacInCloud and similar rent a Mac by
the hour or month. Fine for building. **Poor for this app specifically**,
because the tests that matter involve a physical iPhone playing sound
overnight, and you cannot plug your phone into a Mac in a datacentre.

**Buy a Mac mini.** The cheapest Mac Apple sells and entirely sufficient. But
do not buy hardware to find out whether an unproven app works — borrow first,
buy later if it is worth it.

**Requirements:** a Mac from roughly the last five years, running a macOS
recent enough for the current Xcode. About **40 GB of free disk** — Xcode is
enormous.

> **You do not need to pay Apple anything to complete steps 1–7.** A free
> Apple Account lets you build and run the app on your own iPhone. Find out
> whether it works before you spend $99.

---

## Step 2 — Get an Apple Account with two-factor authentication

You almost certainly already have one (it is what you sign into your iPhone
with). It will become your developer identity, so decide now whether you want
to use your personal one or make a fresh one for this.

1. If making a new one: [account.apple.com](https://account.apple.com) →
   *Create Your Apple Account*.
2. **Turn on two-factor authentication.** Apple will not let you enrol as a
   developer without it. On your iPhone: *Settings → [your name] → Sign-In &
   Security → Two-Factor Authentication*.

**Worked when:** signing in asks for a code from your phone.

---

## Step 3 — Install the tools on the Mac

**Xcode**, from the Mac App Store. It is 10–15 GB and can take an hour or
more. Start it and go and do something else.

When it finishes, open it once and let it install the additional components it
asks for. Then, in Terminal:

```bash
xcode-select --install
```

If it says the tools are already installed, that is fine.

**Node.js**, from [nodejs.org](https://nodejs.org) — take the LTS version.
Verify in Terminal:

```bash
node --version    # expect v20 or newer
```

**Git** comes with the Xcode command line tools.

You do **not** need CocoaPods. This project uses Swift Package Manager.

---

## Step 4 — Get the code onto the Mac

```bash
git clone https://github.com/peterboggild/BrokildApps.git
cd BrokildApps
git checkout ios-sleeper-agent
```

Then build the iOS project:

```bash
cd ios-apps/sleeper-agent
npm install
npm run sync
npm run open
```

`npm run sync` copies the web app into the native project. `npm run open`
opens Xcode.

**Worked when:** Xcode opens showing a project called **App**, and after a
minute or two of "Resolving Package Dependencies" at the top, that message
goes away.

---

## Step 5 — The first build, and the errors you should expect

In Xcode, at the top left, choose a simulator from the device menu (any
iPhone). Then press **⌘B** (Product → Build).

**It will probably fail.** The Swift audio engine in this project was written
on a machine with no Apple compiler, so it has never been checked. One
critical pass was done by reading it back — which found and fixed five real
defects, including one that could not have compiled at all — but that is not
the same as building it.

When it fails:

1. In Xcode's left sidebar, click the ⚠️/❌ icon (Issue Navigator).
2. You will see a list of errors with file names and line numbers.
3. **Copy the whole list.** Right-click → Copy, or screenshot it.

Then either fix them yourself if they look obvious, or hand them to an AI
assistant with this:

> In this repository on branch `ios-sleeper-agent`, the Swift in
> `ios-apps/sleeper-agent/ios/App/App/Native/` has never been compiled. Here
> are the errors from the first Xcode build. Please fix them. Read
> `ios-apps/sleeper-agent/START_HERE.md` first.
>
> [paste errors]

Repeat until it builds. This is normal first-build work, not a sign the
project is broken.

**Worked when:** Xcode says **Build Succeeded**.

---

## Step 6 — Run it in the simulator

Press **⌘R** (Product → Run). The simulator boots — slowly, the first time —
and the app appears.

Check these, which are cheap and catch a lot:

- The launch screen is **dark**, not white
- No "BrokildApps" link anywhere (that is web-only chrome)
- Tapping play shows *"Preparing brown…"* for a few seconds, then sound
- Tapping play a second time starts immediately
- The blackout button gives a genuinely black screen with a dim clock

**Do not conclude anything about background audio from the simulator.** Its
audio behaviour is not a phone's. That is step 7.

Full list: `IOS_QA_CHECKLIST.md` §2.

---

## Step 7 — Run it on your actual iPhone (still free)

This is the step that tells you whether the whole project was worth it.

### Connect and trust

1. Plug the iPhone into the Mac with a cable.
2. Unlock the phone. It asks **"Trust This Computer?"** — tap **Trust** and
   enter your passcode.
3. If Xcode complains about Developer Mode: on the phone, *Settings → Privacy
   & Security → Developer Mode → on*. The phone restarts.

### Sign the app with a free account

Apple requires every app to be signed, even to run on your own phone. A free
account is enough.

1. **Xcode → Settings → Accounts → +** → *Apple ID* → sign in.
2. Close Settings. In the project navigator (left sidebar) click the blue
   **App** icon at the top.
3. Select the **App** target, then the **Signing & Capabilities** tab.
4. Tick **Automatically manage signing**.
5. In **Team**, choose your name — it will say *(Personal Team)*.
6. If it complains the bundle identifier is unavailable, change it to
   something unique to you, e.g. append your initials. **This is temporary and
   does not commit you to anything** — the real identifier is chosen in step 9.

### Run it

Choose your iPhone from the device menu at the top, then **⌘R**.

The first time, the phone refuses to open the app and mentions an untrusted
developer. On the phone: *Settings → General → VPN & Device Management* → tap
your Apple Account → **Trust**.

> A free account's signing expires after **7 days**. The app stops opening and
> you plug in and run again. That is fine for testing; it is one of the things
> the $99 removes.

### Now the test that matters

Open `IOS_QA_CHECKLIST.md` and work **§4 — background audio**. In particular:

- Start it playing, lock the screen, and check it is still playing after five
  minutes, after an hour, and after a whole night.
- Check the lock screen shows what is playing and how long is left.

**This is the decision point.** If the app plays reliably through a night, the
project works and the rest of this guide is administration. If it does not,
stop and fix that before spending any money — the checklist tells you what to
capture from the Xcode log to diagnose it.

Also worth doing now, because it is easy: §7 (put the phone in airplane mode
and confirm everything still works) and §11 (listen for clicks and for the
loop repeating).

---

## Step 8 — Publish the privacy and support pages

Apple requires a **privacy policy URL** and a **support URL**, and it checks
that they load. Both pages are written, but they currently exist only on the
`ios-sleeper-agent` branch, so they are not live yet.

Merge the branch to `main`, which is what GitHub Pages publishes:

```bash
git checkout main
git merge ios-sleeper-agent
git push origin main
```

Wait a couple of minutes, then confirm both of these open in a browser:

- `https://peterboggild.github.io/BrokildApps/health-apps/sleep-noise/privacy.html`
- `https://peterboggild.github.io/BrokildApps/health-apps/sleep-noise/support.html`

**Read them both before pointing Apple at them.** They are written in your
name and make factual claims about the app. The contact address on them is
`brokildapps@gmail.com`.

---

## Step 9 — Enrol in the Apple Developer Program ($99/year)

Only do this once step 7 convinced you the app is worth shipping.

Go to [developer.apple.com/programs/enroll](https://developer.apple.com/programs/enroll/)
and sign in with your Apple Account.

### The one real decision: Individual or Organization

**Individual** — $99/year, usually approved within a day or two, needs only
your ID. **Your own legal name is displayed publicly** on the App Store as the
seller. This is the right choice for a first app by one person.

**Organization** — same price, but requires a legally registered company and a
**D-U-N-S number** (a free company identifier from Dun & Bradstreet that can
take a week or two to obtain). Your company name is shown instead of yours.

Pick Individual unless you specifically need a company name shown. You can
move to an Organization account later; it is not a one-way door, though it is
not effortless either.

### If you are in the EU — read this before you enrol

EU law (the Digital Services Act) requires anyone distributing apps to EU
users to declare **trader status** and provide contact details — name, address,
phone, email — which Apple then **displays publicly on your App Store listing**.
Apps without this can be removed from EU storefronts.

This surprises people: it means a home address and phone number can end up
public. Options are to use a business address if you have one, or to accept
it. Look at Apple's current guidance before enrolling, because this area has
changed more than once.

### Then

Pay, and wait for approval — normally a day or two, occasionally longer.

**Worked when:** [developer.apple.com/account](https://developer.apple.com/account)
shows an active membership.

---

## Step 10 — Set up the business side

In [App Store Connect](https://appstoreconnect.apple.com) → **Business**
(sometimes called *Agreements, Tax, and Banking*):

1. **Accept the Paid Applications Agreement.** Without this you can only ship
   free apps.
2. **Tax forms.** Which ones depends on your country. Apple prompts you.
3. **Banking details** — where your money goes.

Apple will not pay you, and in some cases will not let you set a price, until
all three are complete.

### Apply to the Small Business Program

Separately, at
[developer.apple.com/app-store/small-business-program](https://developer.apple.com/app-store/small-business-program/).

If you earn under $1 million a year from the App Store, this reduces Apple's
commission from **30% to 15%**. It is not automatic — you have to apply, and
approval takes effect from the start of the following month.

**Do this now.** It is a form, it takes five minutes, and forgetting it costs
you 15% of everything.

---

## Step 11 — Create the app record

App Store Connect → **My Apps** → **+** → **New App**.

| Field | What to enter |
|---|---|
| Platforms | iOS |
| Name | `Sleeper Agent` — must be unique across the whole App Store. If taken, see below |
| Primary Language | English (U.K.) |
| Bundle ID | Create a new one: `com.peterboggild.sleeperagent` |
| SKU | Anything private to you, e.g. `sleeper-agent-001` |
| User Access | Full Access |

> **The bundle identifier cannot be changed or reused, ever.** Check the
> spelling before you click create. If you use a different one from the line
> above, you must also change it in Xcode (target → General → Bundle
> Identifier) and in `capacitor.config.json`.

If the name **Sleeper Agent** is already taken, you can be listed under a
variant (`Sleeper Agent: Sleep Sound`) — the name shown under the icon on the
phone is set separately in Xcode and can stay short.

---

## Step 12 — Fill in the listing

All the copy is written for you in **`APP_STORE_METADATA.md`**. Open it
alongside App Store Connect and copy the fields across: subtitle, promotional
text, description, keywords, category, copyright.

Three things in that file are **your decision**, and it explains the trade-off
for each:

- **Category** — Health & Fitness or Utilities
- **The keyword `tinnitus`** — recommended against, and the reasoning is given
- **Price** — a starting point of about €4.99 is suggested, with the argument
  for it; you set the actual number

Also enter:

- **Privacy Policy URL** and **Support URL** — the two from step 8
- **App Review contact** — your name, `brokildapps@gmail.com`, and a phone
  number Apple can reach you on. This is not public; it is for the reviewer
- **Review notes** — paste the block from `APP_STORE_METADATA.md` §11. It
  explains the background-audio permission and why this is a native app rather
  than a wrapped website, which is the objection most likely to be raised

### The privacy questionnaire

App Store Connect → your app → **App Privacy**.

The answer is **"Data Not Collected"** — answer *No* to the first question and
the questionnaire ends. **`APP_PRIVACY_NOTES.md`** explains why that is true
and gives you the commands to verify it yourself before you sign it. These are
legally meaningful declarations, so satisfy yourself rather than taking anyone's
word for it.

### Age rating

Answer the questionnaire; everything is *None*. You should land on **4+**.

---

## Step 13 — Screenshots

Required: **6.9-inch iPhone**, portrait. App Store Connect lists the exact
pixel dimensions and which other sizes it wants — read them there rather than
here, because they change.

To capture them:

1. In Xcode, choose the matching simulator (the largest current iPhone Pro Max).
2. **⌘R** to run.
3. Set the app up for each shot.
4. **⌘S** in the simulator saves a screenshot to your desktop at exactly the
   right size.

`APP_STORE_METADATA.md` §10 lists five frames worth taking and what each should
show — the mixer, the presets, the sleep timer, blackout mode, the ambient
themes.

Screenshots must have **no transparency**. Simulator screenshots are fine.

---

## Step 14 — Build the thing you actually ship

In Xcode:

1. Click the blue **App** icon → **App** target → **General**.
2. Set **Version** to `1.0.0` and **Build** to `1`.
   - Version is what customers see. Build is Apple's internal counter.
   - **Every upload needs a higher build number than the last.** Uploading
     build 1 twice is rejected. Rejected build? Bump to 2 and re-upload.
3. **Signing & Capabilities** → Team: now select your paid team, not
   *(Personal Team)*. Bundle Identifier must match step 11 exactly.
4. Confirm **Background Modes → Audio** is still ticked. If the section is
   missing, add it: **+ Capability** → *Background Modes* → tick **Audio,
   AirPlay, and Picture in Picture**.

Then work `IOS_QA_CHECKLIST.md` §12 — a short list of things that are painful
to discover after upload rather than before.

---

## Step 15 — Archive and upload

1. In the device menu at the top, choose **Any iOS Device (arm64)** — not a
   simulator. Archive is unavailable otherwise.
2. **Product → Archive.** Takes a few minutes.
3. The **Organizer** window opens with your archive listed.
4. Click **Validate App** first. Fix anything it reports and archive again.
   Validation catches problems locally that would otherwise come back as an
   email an hour later.
5. Click **Distribute App** → **App Store Connect** → **Upload**. Accept the
   defaults.

**Worked when:** the upload completes, and after 10–30 minutes the build
appears in App Store Connect under **TestFlight**. You may get an email about
it. Warnings about missing "encryption compliance" should not appear — the
project already declares it.

---

## Step 16 — TestFlight, and actually sleeping with it

Do not skip this. It is the only way to run the shipping build the way a user
will.

1. App Store Connect → your app → **TestFlight**.
2. Under **Internal Testing**, create a group, add yourself, and enable the
   build for it.
3. Install **TestFlight** from the App Store on your iPhone, and install the
   app from it.
4. **Delete the version you installed from Xcode first**, so you know which one
   you are testing.

Now use it for **at least three real nights**. Repeat `IOS_QA_CHECKLIST.md` §4,
and check in the morning:

- Did it play all night?
- Did the timer stop it when it should have?
- How much battery did it use?
- Did anything odd happen with calls, alarms or headphones?

Fix anything you find, bump the build number, and upload again.

---

## Step 17 — Submit for review

App Store Connect → your app → the version → **Add for Review** →
**Submit for Review**.

Before you click:

- Every field has a green tick
- The right build is selected
- **Release: Manually release this version** — recommended. It means approval
  at 3 a.m. does not put the app live before you have looked at it

Review usually takes a day or two. You will get an email.

### If it is rejected

Normal, and usually fixable. Apple names the guideline. Read what they wrote,
fix the actual problem, and reply in Resolution Center — politely and
factually. Do not argue and do not try to work around a rule.

The most likely objection for this app is **guideline 4.2, Minimum
Functionality** — "this looks like a website in a wrapper". The review notes
in `APP_STORE_METADATA.md` answer that up front, which is why they are worth
pasting in. If it comes up anyway, the substance of the reply is: the app
bundles everything and works offline, and the audio engine, sleep timer,
fades, lock-screen integration and interruption handling are all native
AVFoundation code, not web code.

### When it is approved

If you chose manual release, click **Release This Version**. It appears on the
store within a few hours.

---

## Step 18 — Afterwards

**To ship an update:** change the code, bump **Build** (and **Version** if
users should see a change), archive, upload, submit. Steps 14–17 again, and
much faster the second time.

**Watch:** App Store Connect shows crashes and reviews. Support email comes to
`brokildapps@gmail.com`.

**Remember the $99 recurs.** If the membership lapses, the app is removed from
sale.

---

## If you get stuck

- Build errors → step 5, and hand them to an AI assistant with the prompt there
- Something not working on the phone → `IOS_QA_CHECKLIST.md`, which lists ten
  known-unknowns and where to look first
- "Why is it built this way?" → `IOS_PORT_NOTES.md` §4
- "What am I allowed to change?" → `README-iOS.md` §12
- Apple's own rules → the
  [App Store Review Guidelines](https://developer.apple.com/app-store/review/guidelines/)

And `git log ios-sleeper-agent` explains why almost every decision in this
project was made, which is often the thing you actually want to know.
