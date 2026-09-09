# Decisions only you can make

Everything else has been decided and written down. These are the ones that are
yours — because they are legally meaningful, irreversible, cost money, or are
simply a matter of taste that nobody should guess at on your behalf.

Each has a recommendation and the reasoning behind it. None has been acted on.

---

## Before anything is built

### 1. How you get to a Mac
**Recommendation: borrow one for an afternoon first.**

You need it for three sessions at most. Do not buy hardware to find out
whether an unproven app works. `SHIPPING_GUIDE.md` step 1 compares the four
options; note that cloud-rented Macs are a poor fit here specifically, because
the tests that matter need a physical iPhone plugged in.

---

## Before you spend money

### 2. Whether the app is worth shipping at all
**This is a real decision, and it comes earlier than you might think.**

`SHIPPING_GUIDE.md` steps 1–7 cost nothing: a free Apple Account will build
and run the app on your own iPhone. Only after it has played through a night
reliably is the $99 worth spending.

If it does not survive a night, the whole premise fails and the answer is to
fix that, not to pay Apple. `IOS_QA_CHECKLIST.md` §4 is the test and says what
to capture if it fails.

---

## At enrolment

### 3. Individual or Organization
**Recommendation: Individual.**

$99/year either way. Individual is approved in a day or two and needs only
your ID; **your legal name is shown publicly as the seller.** Organization
shows a company name instead but requires a registered company and a D-U-N-S
number, which can take a week or more.

For a first app by one person, Individual. Not irreversible, but not effortless
to change either.

### 4. EU trader status — what becomes public
**Read Apple's current rules before enrolling.**

EU law requires anyone distributing to EU users to declare trader status and
supply a name, address, phone and email that Apple **publishes on your App
Store page**. A home address can end up public. If you have a business address,
use it. This area has changed more than once, so check rather than assume.

---

## In App Store Connect

### 5. The bundle identifier — **irreversible**
**Proposed: `com.peterboggild.sleeperagent`**

Once registered with Apple it can never be changed or reused. Check the
spelling before creating it. If you choose differently, it must also be changed
in Xcode and in `capacitor.config.json`.

### 6. Price
**Recommendation: paid once, around €4.99, no subscription, no in-app purchase.**

Paid-once removes a startling amount of complexity — no receipt validation, no
StoreKit, no restore-purchases flow, no paywall — and it is consistent with the
app's actual argument, which is that it does nothing behind your back.

€4.99 sits above the impulse tier that attracts "it's just noise" one-star
reviews and below where people expect a subscription and a library of
recordings. It is a starting point, not a recommendation you should feel bound
by. Reasoning in `APP_STORE_METADATA.md` §12.

### 7. Category
**Recommendation: Health & Fitness, with Utilities secondary.**

It is where sleep-sound apps live and where people look. The trade-off:
Health & Fitness attracts more scrutiny of health claims. The app makes none —
audited, and the listing copy was written to keep it that way — so that
scrutiny should cost nothing. If you would rather sidestep the question
entirely, Utilities is defensible and loses only discoverability.

### 8. The keyword `tinnitus`
**Recommendation: drop it.**

People with tinnitus genuinely search for sound maskers, so it is a term of
intent rather than a claim, and keywords are not visible copy. But it is
adjacent to a medical condition the description deliberately never mentions.
The app does not need the word, and consistency between keywords and copy is
worth more than one search term. Both keyword lists are in
`APP_STORE_METADATA.md` §5.

### 9. App Review contact phone number
**Not guessed at.**

Apple requires a phone number for the reviewer to reach you. It is not public.
`brokildapps@gmail.com` covers the email half.

### 10. The listing copy itself
**Drafted in full, awaiting your read.**

`APP_STORE_METADATA.md` §4 has the description. It is written in the app's own
voice and makes no health claims. Change anything you do not recognise as
yours — it goes out under your name.

### 11. The privacy declarations
**Answer: "Data Not Collected". You must sign it, not me.**

`APP_PRIVACY_NOTES.md` gives the answer, the reasoning, and the commands to
verify it yourself. Apple treats these as commitments. Satisfy yourself.

### 12. Release strategy
**Recommendation: manual release.**

Costs nothing, and means an approval at an awkward hour does not put the app
live before you have looked at it.

---

## Not a decision, but worth knowing

**Apply to the Small Business Program.** It cuts Apple's commission from 30% to
15% for anyone under $1M/year. It is not automatic, it takes five minutes, and
forgetting it costs you 15% of everything. `SHIPPING_GUIDE.md` step 10.

**The `ios-sleeper-agent` branch has not been merged.** The privacy and support
pages exist only on it, and Apple needs those two URLs live. Merging is step 8
of the shipping guide.

**PR #26 is still open** — the five-fader mixer, on a separate branch. The iOS
branch contains that work too, so merging the iOS branch effectively lands
both. Decide whether you want them merged separately for a cleaner history.
