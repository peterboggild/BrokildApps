# ARTEFACT B2311.1 — graphics I would like from ChatGPT

Everything the object *does* is computed and stays that way: the face is a live
cross-section of a lattice of nine thousand counters, and every hand on it is a
real phase that moves. No bitmap could keep being correct. What a generated
image *can* do here is the thing procedural drawing is worst at — **make the
plate and the dials look like machined objects that have been somewhere** —
and that is the whole of this list.

Priority order. **(1) is by far the biggest gain**; (2) and (3) are next; (4)
and (5) are polish and can be skipped.

**Two rules that matter more than any content note below.** Resolution is free,
because everything is rescaled on the way in — but **aspect is not**, so a
square part must be delivered square. And **anything meant to tile must have no
edges, no corners, no vignette and no end caps**: the first walnut cheek ordered
for Full Metal Racket came back as a shape with rounded ends, and the side of
the machine read as a stack of blocks.

---

## 1 · THE COUNTER DIAL — one part, used a thousand times  (the big one)

The face is a 32 × 32 grid of counters. Each one is currently a dark disc drawn
in code with a line across it, and it is the hero of the whole panel: there are
1024 of them and the eye lands there first. A real etched dial face, repeated,
would transform it.

**Format:**
- **512 × 512, square, PNG, transparent background.**
- One dial face, **perfectly centred, filling the frame edge to edge** — the
  disc's silhouette is the inscribed circle. No margin, no drop shadow.
- **No hand, no pointer, no needle.** I draw that live; it is the data.
- **No numerals and no digits of any kind.** Nothing on this object was labelled
  by whoever made it.
- Straight-on, orthographic. No perspective, no lighting from a scene.

**Content:** a small precision instrument dial in dark oxidised brass or blackened
steel, of the kind found in laboratory counting apparatus. A shallow concave
face catching a soft light from the upper left. Around the rim, a ring of fine
engraved tick marks — **48 of them, evenly spaced, unlabelled**, some slightly
worn away. A very faint concentric turning texture on the metal. A hairline
bright edge on the upper-left rim and a soft dark edge lower-right, so it reads
as recessed into a plate.

Dark overall — it sits on a near-black panel and the bright thing on it is the
glowing hand, which I add. If in doubt make it darker than feels right.

---

## 2 · THE PLATE — a seamless tile

The panel behind everything. Currently a CSS gradient with a faint diagonal
scratch pattern.

**Format: 1024 × 1024, square, PNG, seamlessly tileable.** No corners, no edges,
no vignette, no lighting hotspot — a hotspot repeats and reads as wallpaper.

**Content:** milled dark metal, close to black, with a fine unidirectional
machining grain running at a shallow diagonal. Occasional shallow scuffs and
one or two very faint circular tool witnesses. Restrained: this is the quietest
surface on the panel and its job is to not look like a flat colour.

---

## 3 · WEAR — a seamless mask

**Format: 1024 × 1024, square, PNG, greyscale, seamlessly tileable.** White is
untouched, black is worn.

**Content:** the pattern of handling on an object that has been in a crate for
two years and in a rille before that — dust settled in recesses, a rubbed patch
where a hand would rest, faint tide-lines. No specific shapes, nothing that
reads as an image. This is multiplied over everything, so it must be subtle: if
it looks like a texture on its own, it is too strong.

---

## 4 · THE NAMEPLATE — one engraved strip

**Format: 1536 × 256, PNG, transparent background.** Aspect matters; the strip
is used at 6:1.

**Content:** a small brass plate, dark with age, screwed at each end, engraved
in a plain sans capital face:

> **PROXIMS CENTAURI B · ARTEFACT B2311.1 · ACCESSION 001**

Note the spelling **PROXIMS**, which is deliberate and is how the other two are
labelled. Engraving filled with dark oxide, catching light on the upper edge of
each stroke. No logos, no borders beyond the plate itself, no other text.

---

## 5 · THE CRATE CORNER — one bracket

**Format: 512 × 512, square, PNG, transparent background.**

**Content:** a stamped steel corner bracket of the kind on a shipping case, seen
straight on, occupying the **top-left** corner of the frame with two visible
rivets. Dark, scuffed, slightly bent at one edge. I mirror it for the other
three corners, so it must be a clean single corner and not a full frame.

---

## What NOT to send

- No full panel mock-ups or faceplates. An image generator cannot hit exact
  pixel coordinates, and a whole panel arrives unusable — this is the lesson
  that cost the most on Full Metal Racket.
- No numerals, dials with numbers, clock faces with 12/3/6/9, or any writing
  except on the nameplate at (4).
- Nothing organic, nothing biological, no Giger.
- No screens, no glow, no light emission: everything luminous on this panel is
  the object's live state, drawn in code over the top.

## Where they go

Drop them into `assets/ab1-decals/` with a `DELIVERED.md` naming each file, as
before. I will crop, rescale, downsample to about four times the on-panel size,
and wire them in — nothing needs to be positioned or sized correctly on your
side beyond the aspect rules above.
