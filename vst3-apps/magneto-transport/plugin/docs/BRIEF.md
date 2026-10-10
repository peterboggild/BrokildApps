# MAGNETO TRANSPORT — the brief as given (2026-10-10)

Peter's brief, verbatim. (He later asked for the name in title case, "Magneto Transport"; the brief keeps his original wording.) His answers to the open questions, the decisions they
force and the tolerances are in `BrokildApps/MAGNETO-TRANSPORT-DESIGN.md`, which
takes precedence where the two differ.

---

0. Ground rules (read first)
Physics in, character out. Every instability, nonlinearity and artefact (saturation, hysteresis, wow, flutter, scrape flutter, dropouts, head bumps, gap loss, spacing loss, bias effects, print-through, tape hiss) must emerge from the simulated mechanics and magnetics. No added random noise generators, no LFOs labelled "wow", no static waveshapers labelled "saturation".
Stochastic phenomena (hiss, modulation noise, dropouts) must come from simulated physical causes: finite particle count / particle-size distribution in the coating, coating thickness variation along the tape, tape-surface roughness against the head, bearing and capstan irregularities. Use a deterministic, seeded "tape stock" (a generated physical profile of the tape) so a given tape behaves the same every time, like a real reel.
Two-tier modelling. First build an offline reference model (high oversampling, small time steps, no shortcuts). Then build the real-time model and prove, by measurement, that it matches the reference within the tolerances in §6. Reduced-order models are allowed only if they are derived from and validated against the reference.
Studio-grade by default. With character controls at minimum, the plugin must be clean enough for mastering-adjacent use. Character is something the user dials in, not a tax they pay.
No hidden gain. Unity in = unity out at the reference operating level with all character at default. Every gain stage is documented.

1. The physical machine
Model a real-feeling machine with these components. For each, write a short design note (equations, parameters, references) in docs/physics/ before coding it.

Tape transport

Supply and take-up reels (changing radius and inertia as tape winds), capstan + pinch roller, guide rollers, tension arms.
Capstan motor as an electromechanical system: drive (AC synchronous or DC servo, selectable), rotor inertia, cogging, speed regulation loop, capstan eccentricity and bearing irregularity.
Tape as an elastic medium between contact points (longitudinal stiffness, damping) so that scrape flutter arises from stick-slip friction over the heads, and wow/flutter arise from the mechanical parts — not from modulation sources.
Speed changes (delay time changes) must glide with real motor/inertia dynamics, producing authentic pitch sweeps.
Optional: tape loop of finite physical length (see §4).

Tape

Ferric oxide (γ-Fe₂O₃) coating with parameters: coercivity, remanence, saturation magnetisation, squareness, particle size distribution, coating thickness, base thickness, surface roughness. Provide 3–4 stock presets (e.g. a vintage low-output stock, a 1970s high-output studio stock, a worn tape).
Magnetic hysteresis via an established model (e.g. Jiles–Atherton or a Preisach model — justify the choice). Saturation must come from the oxide reaching its physical magnetisation limit.
Print-through and self-demagnetisation over the loop's storage time, if feasible.

Heads (erase, record, three playback heads)

Record head: gap field (Karlqvist or better), AC bias at a realistic frequency with adjustable bias level, so that under/over-bias behaviour emerges physically. Simulate bias properly at sufficient oversampling or use a validated equivalent; document which.
Playback heads: gap loss, spacing loss, thickness loss, head-contour (head-bump) response, azimuth, head wear parameter. Each head's position along the tape path is a physical distance; delay times follow from distance / tape speed.
Erase head: field strength and decay, so incomplete erasure is physically possible.
Heads may be moved along a rail (user-controllable), changing delay ratios with physically correct Doppler while moving.

Electronics

Record and playback amplifiers with realistic but high-quality topology (equalisation per a chosen standard, e.g. IEC/NAB at the chosen speed). Keep the electronics clean unless the user drives them; the tape is the star.

2. Signal path and features
I/O modes: mono→mono, mono→stereo, stereo→stereo. Stereo uses a two-track head stack with physically modelled inter-track crosstalk and azimuth.
Head selection: any combination of the three playback heads (Space-Echo-style mode selector + free per-head on/off, level, pan).
Controls (minimum): Input drive, Record level, Bias, Tape speed (with musical sync option that sets motor speed), Head positions, Feedback (intensity) with a physically placed feedback path (playback → record), Tone (bass/treble in feedback path), Tape stock, Tape age/wear, Transport condition (new ↔ worn mechanics), Mix (equal-power, plus kill-dry), Output.
Feedback stability: self-oscillation must be possible and musical but must not run away to digital clipping; containment must come from the tape's physical saturation, not from a limiter.
Parameter smoothing on everything; no zipper noise.
Sample rates: 44.1–192 kHz, behaviour identical (within tolerance) across rates.
Latency reported correctly to the host. Bypass is click-free.
CPU budget: state target as < X% of one core at 48 kHz stereo on a modern machine (measure and report the actual number; offer a "Quality: Eco / Studio / Reference" switch if needed).

3. Mechanical sounds (optional output)
Physically driven sound of the machine itself: motor whir tied to actual rotational speed, roller and bearing noise, relay/solenoid clunks on mode changes, tape slap and head-shifting chunks.
Synthesised or derived from the mechanical state — they must follow what the machine is doing.
Separate level control, off by default, never leaks into the audio when off.

4. Standout feature (never on a tape delay before)
Implement FLUX MEMORY — a variable-strength erase head on a finite physical tape loop.

The tape is a closed loop of user-selectable physical length (e.g. 0.5–6 m). The erase head's field can be turned down from full erasure to zero.
With partial erase, earlier recordings survive as remanent magnetisation and come back around every loop period, then get re-recorded over through the real hysteresis model. New audio is literally layered onto magnetic history, so old echoes fade, smear and saturate according to physics, not a decay parameter.
Delay-line echoes (from the heads) and loop-period "ghosts" (from the tape's memory) coexist, creating a second, slower rhythmic and textural layer that no tape echo has offered.
Include "Freeze" (erase off, record off: the loop plays forever, degrading only via physically modelled self-demagnetisation and wear if enabled).
If, after analysis, you find a stronger physically grounded candidate, write a one-page comparison and ask me before switching. Other candidates you may consider: live under/over-bias as a performance control; per-head azimuth control for physically real stereo decorrelation; moving heads with true Doppler.

5. User interface
The UI shows the machine: reels (radius shrinking/growing), capstan, pinch roller, guide rollers, tension arms, three playback heads on their rail, erase and record heads, and the tape moving between them — all driven by the simulation state (not canned animations).
Smooth 60 fps, GPU-friendly, resizable, sharp on HiDPI.
Build first with clean placeholder graphics so everything works before artwork exists.
Aesthetic: ancient, high-quality studio recording equipment crossed with secret spy/military hardware. Sinister, scratched, worn, serious.

Decal brief (deliverable, then stop)
Once the UI layout is fixed, produce docs/decal_brief.md: a self-contained description I can paste into an image generator. For each asset, specify:

Exact pixel dimensions (at 1× and 2×), transparent vs opaque background, file name.
What it is, where it sits in the layout, what overlaps it.
Materials (e.g. black crinkle-finish steel, anodised aluminium, bakelite, engraved brass plates), lighting direction (consistent across assets), wear (scratches, fingerprints, paint chips, rust, faded stencil), colour palette with hex values.
All text that appears (model name "MAGNETO TRANSPORT", serial-plate details, stencilled warnings, classification markings, control labels with exact wording and fonts/style), so labels match the controls.
Which parts must be separate layers (anything that moves or rotates: reels, rollers, knobs, needles, heads), and their pivot points.
Also give a one-paragraph master style description to paste before every asset prompt for consistency.

Then stop and wait for me to return the artwork. Integrate it when I do.

6. Measurement and verification
Build an offline test harness (C++ renderer + Python analysis) and produce a measurement report (docs/measurements.md with plots). At minimum:

Gain structure: define the reference: −18 dBFS RMS sine at 1 kHz = 0 VU = chosen reference fluxivity (e.g. 250 nWb/m; state it). Show unity gain at reference with defaults; document headroom at each stage.
Frequency response per head, per speed, at −20 dB re reference, with and without character.
THD and THD+N vs level (1 kHz and 100 Hz), showing MOL (3% THD) and saturation curve; compare to published data for the modelled tape type.
Wow and flutter (DIN/IEC weighted) per transport-condition setting; show it emerges from the mechanics.
Noise floor / hiss spectrum from the particle model, and signal-to-noise.
Aliasing: high-level sweeps at 44.1 kHz; aliased components < −100 dBFS at defaults in Studio quality.
Stereo: crosstalk, phase/azimuth behaviour, mono compatibility.
Reference vs real-time model: error metrics on a test set (sines, sweeps, drums, voice) — state tolerance and show it's met.
Stability: feedback at max for 10 minutes, no NaN/inf/denormals, no runaway; automation torture test; sample-rate switch; pluginval at strictness 10.
CPU and latency figures.
Fix anything that fails, re-measure, and report honestly what remains imperfect.

7. Delivery
Commit in logical steps to the repository [REPO NAME/URL].
Write the user manual in the same style, structure and format as the existing manuals in the repo (read them first).
Draft the GitHub README / homepage description and show it to me for approval before pushing it.
Final summary: what was built, key measurements, known limitations, next steps.
Working style
Start by reading the repo and writing a short plan with milestones; then proceed without waiting unless you hit a real decision.
Keep a docs/DECISIONS.md log of physics and engineering choices.
When in doubt between "sounds impressive" and "physically correct", choose physically correct and tell me.
