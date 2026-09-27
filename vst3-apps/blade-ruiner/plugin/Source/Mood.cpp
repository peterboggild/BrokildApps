#include "Engine.h"

/*  The mood organ: seed -> patch, and seed -> a line of text.

    Both are pure functions of the seed. No clock, no static state, no
    floating-point accumulation across calls — a xorshift stepped a fixed
    number of times. Seed 481 is the same instrument on every machine and in
    every session, which is the only thing that makes a three-digit number
    worth writing on a piece of paper.                                       */

namespace br
{

namespace
{
    // ---- the grammar -----------------------------------------------------
    /*  Each template carries its own vocabulary, chosen so that every
        combination inside it is grammatical. That is why there are eight
        small tables rather than one big one: a shared noun list would
        eventually put "gratitude for one's own obsolescence" next to
        "willingness to weather", and half of it would read as nonsense. */

    /*  Every head here has to take "of" with every noun below it. That rules
        out a lot of otherwise good words — "reluctant admiration" takes for,
        "renewed interest" takes in, "grief" takes at — so they live under
        the templates that fit them instead. */
    const char* A_OF[] = {                       // "<A_OF> of <N_OF>"
        "awareness", "placid recognition", "dim apprehension", "clear-eyed acceptance",
        "quiet dread", "unearned confidence", "the exact shape",
        "a working knowledge", "the calm", "a long memory", "a thorough inventory",
        "the faint pleasure", "a small, clean sense", "settled tolerance",
        "an accurate estimate", "second-hand memory", "the private economy",
        "some late understanding", "wary appreciation", "a full accounting",
        "the cost", "steady knowledge", "the slow arithmetic", "a clear view"
    };
    const char* A_IN[] = {                       // "<A_IN> in <N_OF>"
        "renewed interest", "a professional interest", "quiet faith",
        "diminishing confidence", "a stubborn belief", "no confidence whatever"
    };
    const char* N_OF[] = {
        "one's own obsolescence",
        "the manifold possibilities open to me in the future",
        "weather that is not dust",
        "the neighbour's long silence",
        "a room where the television has just been turned off",
        "what the animal on the roof is for",
        "other people's mornings",
        "the distance to the nearest real thing",
        "a debt that will not be called in",
        "the hour before the shift begins",
        "machinery that has outlived its purpose",
        "the empty half of the apartment",
        "a kindness that cost nothing",
        "the dust settling exactly as it did yesterday",
        "the difference between wanting and being told to want",
        "a colour seen only in advertisements",
        "the last honest hour of the evening",
        "everything one has agreed not to notice",
        "the weight of an ordinary Tuesday",
        "a name one has stopped using",
        "the building's opinion of its tenants",
        "small print agreed to years ago",
        "the exact price of a real one",
        "someone else's certainty",
        "the corridor at four in the morning",
        "an apology composed but not delivered"
    };
    /*  The subset short enough to carry a coda without the line becoming a
        paragraph. Same words, chosen by length. */
    const char* N_SHORT[] = {
        "one's own obsolescence", "weather that is not dust",
        "the neighbour's long silence", "what the animal on the roof is for",
        "other people's mornings", "a debt that will not be called in",
        "the hour before the shift begins", "the empty half of the apartment",
        "a kindness that cost nothing", "a colour seen only in advertisements",
        "the last honest hour of the evening", "the weight of an ordinary Tuesday",
        "a name one has stopped using", "small print agreed to years ago",
        "the exact price of a real one", "someone else's certainty",
        "the corridor at four in the morning"
    };

    /*  Mostly nothing. The codas only attach cleanly to the "of" template,
        and only a few of them survive every noun above — " in all matters"
        reads beautifully after wisdom and absurdly after a corridor. */
    const char* CODA[] = {
        "", "", "", "", "", ", which passes", ", and of the fact that it will pass",
        ", briefly"
    };

    const char* A_AT[] = {
        "mild indignation", "quiet alarm", "unexpected delight", "professional irritation",
        "slow astonishment", "faint embarrassment", "unhurried surprise", "dry amusement",
        "polite disbelief", "a flicker of resentment"
    };
    const char* N_AT[] = {
        "the price of things", "how easily it was believed", "the neighbour's new animal",
        "one's own reflection, unprepared", "the length of the corridor",
        "being addressed by name", "the cheerfulness of the broadcast",
        "how little was required", "the confidence of the catalogue",
        "weather arriving on schedule"
    };

    const char* A_FOR[] = {
        "gratitude", "affection", "a soft spot", "reluctant sympathy", "quiet regard",
        "unaccountable fondness", "respect", "a steady patience", "reluctant admiration",
        "an embarrassing weakness"
    };
    const char* N_FOR[] = {
        "a machine that has done nothing to earn it", "rain that means it",
        "the sound of someone else's routine", "objects that keep working",
        "a stranger's competence", "the smell of rain on hot concrete",
        "anything that is not a copy", "the person who did not ask",
        "instructions that can be followed exactly", "a job that ends at a fixed hour"
    };

    const char* V_TO[] = {                       // "the desire to <V>" / "willingness to <V>"
        "watch television, no matter what is on it",
        "go outside without first checking the dust",
        "admit the animal is not real",
        "be looked at by something that can see",
        "answer honestly, once",
        "leave the set on for the company",
        "walk somewhere with no destination",
        "begin again on a Tuesday",
        "say the true thing to the wrong person",
        "keep the receipt for something that cannot be returned",
        "stand at the window for longer than is reasonable",
        "be of use to somebody",
        "stop rehearsing the conversation",
        "let the telephone ring",
        "read the whole catalogue without buying",
        "take the stairs, for once"
    };

    /*  All of these have to read as a quantity, because the template is
        "<DUR> of <STATE>" and "most of a Sunday of administrative hope" is
        not a sentence anybody would write down. */
    const char* DUR[] = {
        "six hours", "a full morning", "twenty minutes", "an afternoon",
        "four minutes", "an hour", "a working week", "half a day",
        "one evening", "three days"
    };
    const char* STATE[] = {
        "self-accusatory depression", "businesslike optimism", "unfocused benevolence",
        "productive dissatisfaction", "ceremonial calm", "well-earned indifference",
        "dutiful cheerfulness", "clean, uncomplicated grief", "administrative hope",
        "borrowed enthusiasm"
    };

    const char* PP[] = {                         // "the suspicion of having <PP>"
        "been tested, and of not having noticed",
        "agreed to something in advance",
        "been replaced quietly",
        "misread the entire situation",
        "been the reasonable one for too long",
        "already used up the good years",
        "been described accurately by a stranger"
    };

    const char* CLAUSE[] = {                     // "the conviction that <CLAUSE>"
        "the street outside is genuinely inhabited",
        "somebody is keeping records",
        "the broadcast is meant for you personally",
        "this is the version that will be remembered",
        "the machine is fond of you",
        "tomorrow is administratively different",
        "the dust will stop of its own accord"
    };

    const char* GER[] = {                        // "...for when you cannot face <GER>"
        "dialling", "the morning", "the empty flat", "being sensible",
        "the catalogue", "another honest answer", "the stairs"
    };

    template <int N> constexpr int len (const char* const (&)[N]) { return N; }

    /*  Seventeen written by hand. Five are Dick's, near enough — 3, 382, 481,
        594 and 888 — and the manual says so rather than letting anyone
        wonder which of the thousand are real. */
    struct Written { int code; const char* text; };
    const Written WRITTEN[] = {
        {   3, "a stimulus to dial, for when you cannot face dialling" },
        /*  The second, and last, borrowed from a neighbouring shelf. Written
            as a mood rather than as a joke: this instrument is rain and
            melancholy, and a punchline here would puncture it. Anyone who
            knows the number will know. Anyone who does not reads past it.  */
        {  42, "the calm of having the answer, and no longer the question" },
        { 102, "the ecstatic first taste of a morning you have not earned" },
        { 127, "placid recognition of one's own obsolescence" },
        { 249, "willingness to admit the animal is not real" },
        { 304, "gratitude for weather that is not dust" },
        { 382, "six hours of self-accusatory depression" },
        { 401, "the conviction that the street outside is genuinely inhabited" },
        /*  Not Dick, and not ours either: 451 belongs to a different book by a
            different author, and it would be a poor sort of dystopia that did
            not nod to the other one. The manual says so.  */
        { 451, "the wish to remember a book by heart, in case" },
        { 481, "awareness of the manifold possibilities open to me in the future" },
        { 512, "curiosity about what the neighbour keeps on the roof" },
        { 594, "pleased acknowledgment of a spouse's superior wisdom in all matters" },
        { 623, "acceptance of a long walk with no destination" },
        { 700, "the suspicion of having been tested, and of not having noticed" },
        { 741, "the wish to be looked at by something that can see" },
        { 802, "affection for a machine that has done nothing to earn it" },
        { 888, "the desire to watch television, no matter what is on it" },
        { 911, "alertness, without an object" },
        { 964, "the calm of a room where the television has just been turned off" }
    };
    constexpr int NUM_WRITTEN = (int) (sizeof (WRITTEN) / sizeof (WRITTEN[0]));

    int clampSeed (int s) { return s < 0 ? 0 : (s >= NUM_MOODS ? NUM_MOODS - 1 : s); }

    /*  -1 means "no mood has been dialled": the patch on screen is the one
        the plugin ships with, or one loaded from disk, and it is not any of
        the thousand. */
    bool noMood (int s) { return s < 0; }

    /*  Two independent streams from one seed. The line and the patch must not
        move together — if they did, neighbouring codes would describe
        neighbouring sounds and the dial would stop being a surprise. */
    Rng streamFor (int seed, uint32_t salt)
    {
        Rng r;
        r.seed ((uint32_t) (seed + 1) * 2654435761u ^ salt);
        for (int i = 0; i < 8; ++i) r.next();      // let the low bits mix
        return r;
    }
}

namespace { std::string generate (int seed); }

bool moodIsWritten (int seed)
{
    if (noMood (seed)) return false;
    seed = clampSeed (seed);
    for (const auto& w : WRITTEN) if (w.code == seed) return true;
    return false;
}

/*  Drawing the slots at random gave 664 distinct lines out of 1000 — a third
    of the dial repeating itself, which is not what a thousand settings should
    feel like. So the mapping is injective by construction instead.

    The seed is permuted (397 is coprime with 1000, so it is a bijection and
    neighbouring codes land nowhere near each other), the result is split
    between the templates by a fixed allocation, and each template enumerates
    its own combinations through a stride coprime with its capacity. Distinct
    seed in, distinct line out, every time, and no two codes share a
    description.                                                            */
std::string moodLine (int seed)
{
    if (noMood (seed)) return {};
    seed = clampSeed (seed);
    for (const auto& w : WRITTEN) if (w.code == seed) return std::string (w.text);

    std::string line = generate (seed);

    /*  A handful of the generated lines land exactly on a hand-written one —
        "awareness of the manifold possibilities…" is, after all, assembled
        from the same words. Those seeds take over the slot belonging to the
        written code instead, which nothing else uses, because that code
        returns its own text above. Still one line per seed, still no
        repeats.                                                            */
    for (const auto& w : WRITTEN)
        if (line == w.text) return generate (w.code);

    return line;
}

namespace {
std::string generate (int seed)
{
    int k = (seed * 397 + 131) % NUM_MOODS;

    // of · of+which passes · of+briefly · in · at · for · desire · willing · dur · pp · clause · ger
    static const int SHARE[] = { 500, 30, 27, 120, 90, 90, 16, 16, 90, 7, 7, 7 };
    int t = 0;
    while (t < 11 && k >= SHARE[t]) { k -= SHARE[t]; ++t; }

    // spread each template's picks across its whole space rather than the front of it
    auto spread = [] (int idx, int stride, int cap) { return (idx * stride) % cap; };

    const int nOf = len (N_OF);
    switch (t)
    {
        case 0:
        {
            const int c = spread (k, 373, len (A_OF) * nOf);   // 373 prime, coprime with 24*26
            return std::string (A_OF[c % len (A_OF)]) + " of " + N_OF[c / len (A_OF)];
        }
        case 1: case 2:
        {
            /*  The codas only go on the short nouns. "no illusions about the
                value of the difference between wanting and being told to
                want, which passes" is a hundred and five characters and
                nobody would write it down. */
            const int c = spread (k, 373, len (A_OF) * len (N_SHORT));
            return std::string (A_OF[c % len (A_OF)]) + " of " + N_SHORT[c / len (A_OF)]
                 + (t == 1 ? ", which passes" : ", briefly");
        }
        case 3:
        {
            const int c = spread (k, 7, len (A_IN) * nOf);
            return std::string (A_IN[c % len (A_IN)]) + " in " + N_OF[c / len (A_IN)];
        }
        case 4:
        {
            const int c = spread (k, 7, len (A_AT) * len (N_AT));
            return std::string (A_AT[c % len (A_AT)]) + " at " + N_AT[c / len (A_AT)];
        }
        case 5:
        {
            const int c = spread (k, 7, len (A_FOR) * len (N_FOR));
            return std::string (A_FOR[c % len (A_FOR)]) + " for " + N_FOR[c / len (A_FOR)];
        }
        case 6:  return std::string ("the desire to ")    + V_TO[k % len (V_TO)];
        case 7:  return std::string ("willingness to ")   + V_TO[k % len (V_TO)];
        case 8:
        {
            const int c = spread (k, 7, len (DUR) * len (STATE));
            return std::string (DUR[c % len (DUR)]) + " of " + STATE[c / len (DUR)];
        }
        case 9:  return std::string ("the suspicion of having ") + PP[k % len (PP)];
        case 10: return std::string ("the conviction that ")     + CLAUSE[k % len (CLAUSE)];
        default: return std::string ("a stimulus to dial, for when you cannot face ")
                      + GER[k % len (GER)];
    }
}
} // namespace

std::vector<MoodValue> moodPatch (int seed)
{
    seed = clampSeed (seed);
    Rng r = streamFor (seed, 0x5F356495u);
    auto uni = [&r] (float lo, float hi) { return lo + r.uni() * (hi - lo); };
    auto pick = [&r] (int n) { return (int) (r.next() % (uint32_t) n); };
    auto maybe = [&r] (float p) { return r.uni() < p; };

    std::vector<MoodValue> out;
    out.reserve (44);
    auto put = [&out] (const char* id, float v) { out.push_back ({ id, v }); };

    /*  Which layers are live is part of the mood. LOS ANGELES is favoured
        because it is the bed the other two lie on. */
    bool la = maybe (0.85f), dk = maybe (0.70f), rp = maybe (0.55f);
    if (! (la || dk || rp)) la = true;

    bool laKeyed = maybe (0.12f);
    bool rpKeyed = maybe (0.15f);

    /*  Every mood has to make a sound on its own. "At least one layer live"
        is not enough: DECKARD needs keys, and a keyed LOS ANGELES or a keyed
        REPLICANT needs them too, so a perfectly legal combination can leave
        the dial silent until somebody plays a note. Measured before this was
        here: mood 42, of all of them, said nothing at all. Turning a dial and
        hearing silence reads as a broken plugin rather than as a mood, so if
        nothing would be free-running, the city comes on and stays on. */
    if (! ((la && ! laKeyed) || (rp && ! rpKeyed)))
    {
        la = true;
        laKeyed = false;
    }

    put ("laon", la ? 1.0f : 0.0f);
    put ("dkon", dk ? 1.0f : 0.0f);
    put ("rpon", rp ? 1.0f : 0.0f);

    // ---- Los Angeles
    put ("laroot",   (float) (pick (11) - 5));
    put ("lachord",  (float) pick (NUM_CHORDS));
    put ("lagate",   laKeyed ? 1.0f : 0.0f);
    put ("lasprawl", uni (0.20f, 0.85f));
    put ("lasmog",   uni (0.15f, 0.80f));
    put ("lakipple", uni (0.00f, 0.70f));
    put ("larain",   uni (0.05f, 0.70f));
    put ("laneon",   uni (0.00f, 0.70f));
    put ("ladecay",  uni (0.45f, 0.95f));
    put ("ladrift",  uni (0.10f, 0.85f));
    put ("lasub",    uni (0.20f, 0.80f));

    // ---- Deckard
    put ("dkwave",   (float) pick (NUM_WAVES));
    put ("dkoct",    (float) (pick (3) - 1));
    put ("dkbright", uni (0.20f, 0.80f));
    put ("dkres",    uni (0.05f, 0.60f));
    put ("dkfenv",   uni (0.20f, 0.90f));
    put ("dkatk",    uni (0.05f, 0.70f));
    put ("dkdec",    uni (0.20f, 0.80f));
    put ("dksus",    uni (0.40f, 0.95f));
    put ("dkrel",    uni (0.30f, 0.90f));
    put ("dkring",   maybe (0.30f) ? uni (0.05f, 0.40f) : 0.0f);
    put ("dkens",    uni (0.35f, 1.00f));
    put ("dkglide",  maybe (0.30f) ? uni (0.05f, 0.35f) : 0.0f);
    put ("dkvib",    uni (0.00f, 0.60f));
    put ("dkspace",  uni (0.30f, 0.90f));
    put ("dkdetune", uni (0.10f, 0.60f));

    // ---- Replicant
    put ("rpnexus",  (float) pick (64));
    put ("rprate",   (float) (2 + pick (5)));
    put ("rpsteps",  (float) (8 + pick (9)));
    put ("rpoct",    (float) (pick (3) - 1));
    put ("rpgate",   rpKeyed ? 1.0f : 0.0f);
    put ("rpmetal",  uni (0.10f, 0.90f));
    put ("rpdecay",  uni (0.05f, 0.70f));
    put ("rpglitch", uni (0.00f, 0.60f));
    put ("rpmenace", uni (0.10f, 0.80f));
    put ("rpspread", uni (0.30f, 1.00f));
    put ("rpspace",  uni (0.15f, 0.70f));

    return out;
}

} // namespace br
