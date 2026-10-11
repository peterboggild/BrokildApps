# 08 — Transport mechanics (stage 5)

Wow and flutter are not modulators here. Every control sets a physical CAUSE —
a runout in µm, a bearing's roughness, a tension variation in newtons, a head's
friction — and the modulation depth is what those causes do to the tape. The
tape is a medium (03), so record-time and playback-time speed errors combine
by themselves: a flutter component of frequency f is heard through
2 sin(π f τ) for a head τ seconds downstream, the comb every real tape echo has.

## The capstan motor

**AC synchronous (default).** The rotor is pulled toward the rotating field by
a synchronising torque on the angle error δ, and damped by the damper winding's
torque on slip (ω − Ω_f):

    J ω' = −K δ − D (ω − Ω_f),     δ' = ω − Ω_f

so the speed follows the field as Ω_f (2ζω_h s + ω_h²)/(s² + 2ζω_h s + ω_h²):
hunting at f_h with damping ζ, and a zero from the damper that is physical.
f_h = 1.2 Hz, ζ = 0.8. The varispeed is a frequency converter whose output
slews at most 120 cm/s per second (a synchronous motor driven harder would
fall out of step). The motor's torque ripple at twice the mains (100 Hz)
reaches the tape through the flywheel as a small speed ripple.

**DC servo.** A PI loop on a tachometer: J ω' = K_p (Ω_t − ω) + K_i ∫(Ω_t − ω) is
the same equation, with a faster, better damped loop (4 Hz, ζ = 0.9) and the
tachometer's slot ripple (24 slots per revolution) as its signature.

## Flutter (CAPSTAN): what the capstan assembly does to the tape speed

- capstan shaft runout e_c on a 5 mm radius: e_c / r_c, once per revolution
  (6 Hz at 19 cm/s);
- pinch-roller runout e_p on a 12.5 mm radius, coupled by about a third;
- bearing roughness: band-limited random speed error (3–30 Hz);
- the motor's ripple (above).

## Wow (REELS): tension becomes strain

On a REEL, the supply and take-up reels' eccentricity and the holdback's
friction vary the tape tension; tension is strain (ΔT / EA, EA ≈ 1000 N for
1/4-inch tape), and strain between the reels and the capstan is a speed
variation at the heads, at the reels' rotation rates (0.2–1 Hz) and below.
A LOOP has no reels; its guides' friction varies the loop's tension a little.

## Scrape flutter (SCRAPE): stick-slip at the heads

Friction at each head excites the tape span's longitudinal resonance (a few
kHz: c = √(E/ρ) ≈ 1.8 km/s over spans of 10–30 cm). Each head (record and the
three playbacks, independently) moves the tape locally by a narrow-band
random displacement around 3.2 kHz — a few nanometres on a well-kept
machine, 50 nm rms worn out. It does not show on a weighted W&F meter (the weighting is
gone by 200 Hz); it is heard as the fuzz around a tone.

## The controls

TRANSPORT CONDITION (0–100 %, default 15 % = freshly serviced) scales every
wear-related cause together; CAPSTAN, REELS and SCRAPE (0–200 %, default
100 %) shift the balance between them. CONDITION 0 is an ideal transport (no
causes) for a clean delay. The causes at CONDITION 100 % and the cause
controls at 100 %: capstan runout 5 µm, roller runout 20 µm, bearing
roughness 0.04 %, motor ripple 0.01 %, reel tension variation 0.8 N, tension
noise 0.3 N, scrape displacement 50 nm rms.

## The meter

A record–play W&F meter as DIN 45507 / IEC 60386 measures it: the
instantaneous ratio of the playback speed to the recording speed of the tape
under the first active head, w(t) = v(t) / v(t − τ) − 1, weighted by an
approximation of the IEC curve (a band-pass at 4 Hz), reported as 2σ
(2 × rms of the weighted deviation, over about two seconds). A speed
change is not wow: while the motor is more than 0.3 % off its target, and
for the record-to-play lag plus a second after it arrives, the meter shows
SETTLING; then it averages from scratch, so the first reading is not a slow
climb. The bench
checks it against an independent measurement: a 3150 Hz tone recorded and
played, its frequency demodulated from the zero crossings, weighted the same
way.

## Measured (bench section 18, 48 kHz, 19.05 cm/s, head 1 at 4 cm)

| | |
|---|---|
| CONDITION 0 | speed equals the target exactly (an ideal transport) |
| freshly serviced (CONDITION 15) | 0.0363 % weighted W&F, demodulated from a 3150 Hz tone; the meter reads 0.0367 % |
| worn out (CONDITION 100) | 0.243 % (x6.7) |
| CAPSTAN 0 -> 200 % at CONDITION 50 | 0.031 -> 0.226 % |
| REELS 0 -> 200 % | unweighted wow 0.021 -> 0.152 % rms |
| SCRAPE 0 -> 200 % | sidebands around a 9 kHz tone -66.8 -> -55.4 dB; weighted W&F 0.118 -> 0.121 % |
| AC SYNC, 9.5 -> 19.05 cm/s | 17.7 % overshoot, within 0.1 % after 1.14 s, hunting at 0.72 Hz |
| DC SERVO, the same step | 11.6 % overshoot, within 0.1 % after 0.38 s |
| meter through 19 -> 38 cm/s | SETTLING during the glide, reading again 1.96 s after the change, 0.031 % at the end |
| in the running plugin | 0.035 % fresh, 0.216 % worn out (live check over CDP) |
| cost | 12.5-13.3 % of one core with everything on (target 15 %) |

The rest of the machine (record-head tables, equalisers) follows the motor's
speed, not every flutter cycle: wow and flutter move the tape under the heads,
which is where they belong.

## Not modelled, said plainly

Tape longitudinal waves as such (the span is treated through its resonance,
not as a wave equation); slip at the capstan; the splice bump of a loop
(stage 7); azimuth (with wear, stage 6).
