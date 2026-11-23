# PLANET DESIGNER: design a star and a habitable world

*(The working title was "Planet Studio"; the owner named the app **Planet Designer** on 23 November 2026. The file keeps
its old name so links still work.)*

**Status: DESIGN ONLY, 2026-11-23. The owner's answers to the open questions are in §14.** No code yet. This is the specification and handover for the Claude session
that builds it. It was written at the end of the Illusion Studio session (see §15 for that handover). Read this
file, `vr-apps/CLAUDE.md` and `BROKILD-VR-HANDOVER.md` before writing a line.

> **Til Peter (dansk resumé).** Dette er kravspecifikationen til *Planet Designer*: et værksted, på PC og i VR, hvor
> man designer en stjerne og et solsystem med én eller to planeter i den beboelige zone, og bygger planeten
> indefra: kerne, kappe, skorpe, hav, atmosfære, rotation, hældning og måner. Appen regner hele tiden på, om
> planeten kan bære liv, og især mennesker, og viser *hvorfor* i en kæde af årsager (masse → tyngde →
> undvigelseshastighed → kan den holde på luften → tryk → drivhuseffekt → temperatur …). Målet er at det er sjovt
> og samtidig lærer brugeren, hvor svært det er at lave en Jord. For at starte en ny session: vælg repoet
> `peterboggild/BrokildApps` og skriv fx *"Læs PLANET-STUDIO-DESIGN.md og vr-apps/CLAUDE.md, og byg milepæl M0."*
> Dine svar på de åbne spørgsmål står i §14.

---

## 0. What it is, and what it is not

**It is** a design studio for star systems, built around one question: *can this world host life, and could a
human walk on it without a suit?* The user designs the star, the orbit and the planet down to its core. Every
change flows through physics models that are honest about what they can do, and the consequences are shown as
a chain of causes, not as a magic score.

It has two goals, and both are binding:

1. **Interesting.** A vast planetary workshop, physical tools, and a world you can stand on and look up from.
2. **Teaching.** This is the long-term goal. The user should come away knowing *why* Earth works: why the star's
   mass sets everything, why Mars lost its air, why Venus boiled, why a red dwarf's habitable zone is a trap, why
   bigger is not better. It should be usable in a classroom (upper secondary to first-year university).

**It is not:**

- **A game with a win screen.** There are challenges (§9), but the core is free design with feedback.
- **A star-system toy that lets anything work.** If a design fails, the app says so and why. Fun comes from
  getting it right, not from the app pretending.
- **A research climate model.** It uses well-known, cited, simplified models (§5), says where they stop being
  valid, and marks results that are scientifically contested (§6.3). It never presents a guess as a fact.
- **A copy of a commercial planet game.** No names, art or mechanics lifted from existing titles. The look is
  its own (§10).

Scope limit agreed with Peter: **one star** (a binary option is in "later", §13), **one or two planets in the
habitable zone**, other planets allowed outside it (giants, rocky, icy) because they matter (impacts, water
delivery, orbital stability). **Orbits must not be excessive:** eccentricity has a hard cap (§4.3).

---

## 1. Who uses it, on what

| Platform | How | Role |
|---|---|---|
| **PC (desktop browser)** | Mouse and keyboard, side panels, dashboards and charts | Full design, teaching with a projector, export of images and reports |
| **Quest 3 (WebXR, Quest browser)** | Controllers or hand tracking, physical tools in a workshop | Hands-on design, standing on the planet, the "wow" |
| **Mixed reality (passthrough)** | The orrery and the planet on your real table | Classroom and demo use |

**Same page, same file set, same design JSON.** A design made on the PC opens in the headset and the other way
round (local storage plus a JSON export/import, and a share link with the design compressed into the URL hash).

Levels, following the Atomic Workbench precedent (Kid / Adult / Researcher): **Explorer**, **Student** and
**Scientist**. The level decides how many parameters are shown and how much of the maths. The physics is the
same in all three.

---

## 2. The experience: the Planetary Workshop

A vast hall, kilometres long: an orbital shipyard for worlds. It is not a menu with a 3D view; the tools *are*
the interface. The panel (§11) holds the numbers.

### 2.1 The bays (VR and desktop share the layout; on the PC each bay is a camera position plus its panel tab)

1. **The Star Forge** (far end of the hall). The star hangs in a containment ring. A big **mass dial** sets its
   mass; age and metallicity are smaller dials. The star's colour (black body at its effective temperature,
   §10.2) **lights the whole hall**: design a red dwarf and the workshop goes orange-red. This is the first lesson,
   felt rather than read.
2. **The Orrery Table** (centre). A round table, several metres across, holding the system. The habitable zone is
   painted on it as a ring: **dark green for the conservative zone, pale green for the optimistic zone** (§5.3),
   with the snow line as a frosty ring and the tidal-lock radius as a dashed ring. Planets are beads on their
   orbits. Drag a bead along the radius to set the semi-major axis; pull the orbit's handle to make it eccentric
   (it turns amber, then red, and stops at the cap). Two "HZ slots" glow when the zone has room for a second
   habitable planet with a stable spacing (§5.6).
3. **The Planet Cradle** (the main bay). The selected planet hangs in a gantry at 3 to 4 m diameter. Tools:
   - **Core drill / cutaway knife**: slice a wedge out to see the layers (inner core, outer core, mantle, crust,
     ocean, atmosphere), each with a scale bar and its thickness in km. Sliders, or grabbing the layer boundaries,
     set core mass fraction and composition.
   - **Spin lever and tilt lever**: day length and axial tilt (obliquity), shown on the planet with an axis rod.
   - **Water tap**: fills oceans in units of Earth oceans; the sea level rises over the terrain you sculpted.
   - **Terrain brushes**: raise or lower land, paint continents. This reuses the Illusion Studio idea of
     distortions applied to a mesh with stable UVs (§15).
   - **Gas canister rack**: physical canisters of N₂, O₂, CO₂, H₂O, Ar and CH₄. Plug a canister into the
     atmosphere bell to add that gas; a gauge shows partial pressures. Tangible and hard to forget.
   - **Moon dock**: pick a moon from a shelf and toss or place it into orbit; its distance and mass are handles.
4. **The Instrument Wall** (along one side). The **Habitability Ledger** (§6) as a big board, the **Causality
   Map** (§7), and live charts: the HZ diagram, the climate by latitude, the atmosphere-retention plot (the
   "cosmic shoreline"), the timeline. Labels and leader lines sit beside the data, never over it (owner rule).
5. **The Time Machine** (a lever and a big clock face, "4.6 billion years"). Runs the system forward and back
   (§8): the star brightens, the habitable zone drifts outward, the core cools, the dynamo may die, the air may
   leave, the oceans may boil.
6. **The Lander** (a hatch in the floor). Drops you onto the planet's surface at human scale (§2.3).

### 2.2 Scale: the Illusion Studio rig, extended

Illusion Studio has a user rig whose scale runs from dollhouse to ant (§15). Here the same rig gives three named
scales and anything between:

- **Workshop scale**: you are a giant in the hall and the system is on the table.
- **Planet scale**: the planet fills the cradle; you walk round it.
- **Human scale**: you stand on the surface (the Lander).

Astronomy cannot be shown true to scale on a table, so **every view states its scale**:
"orbit distances: logarithmic", "planet sizes: ×2000 relative to orbits", "atmosphere thickness: ×20". A **True
scale** button shows the real proportions once (the planets become dust, the star a point), because that
emptiness is itself a lesson. Height exaggeration must always be stated on screen (owner rule).

### 2.3 Standing on the planet (the Lander)

At human scale on a patch of the surface (a tile of procedural terrain with the sea level, the sky and clouds):

- **The sky** gets its colour from the star's spectrum, the surface pressure and the composition (Rayleigh
  scattering, §10.3). The sun's **angular size and colour** are correct for the star and the distance; so is the
  day length (a time-lapse slider).
- **A gravity drop**: pick up a ball and drop it; it falls at the planet's g. A rope and a stone swing as a
  pendulum with the right period. Jumping height is shown as a ghost figure.
- **The life-support strip** on the wrist: breathable or not (pO₂, pCO₂, total pressure), temperature and
  wet-bulb temperature, UV index, radiation dose rate, all against the human limits of §4.11.
- **Moons in the sky** at their true angular size and phase. Tides are shown as a tide gauge on the shore.

---

## 3. The design object (data model)

One JSON object, versioned, the only thing saved, shared or exported. **SI units internally**; display units
are a presentation concern. Every derived number lives in a separate `derived` object that is recomputed, never
saved as truth.

```json
{
  "app": "PlanetDesigner", "v": 1, "name": "Second Earth",
  "star": { "massSun": 0.85, "ageGyr": 4.0, "metallicityFeH": 0.0, "activity": "auto" },
  "planets": [
    {
      "id": "p1", "name": "Tellus", "role": "hz",
      "orbit": { "aAU": 0.62, "e": 0.03, "incDeg": 1.0, "argPeriDeg": 0 },
      "spin": { "dayHours": 26, "obliquityDeg": 21, "locked": "auto" },
      "bulk": { "massEarth": 1.1, "coreMassFraction": 0.32, "waterMassFraction": 0.0003,
                "radiogenicRel": 1.0, "fe2mgRel": 1.0 },
      "surface": { "oceansEarth": 1.0, "terrainSeed": 7, "terrainEdits": "…", "landAlbedo": 0.25 },
      "atmosphere": { "pN2bar": 0.78, "pO2bar": 0.21, "pCO2bar": 0.0004, "pH2Obar": "auto",
                      "pArbar": 0.009, "pCH4bar": 0.0000018 },
      "moons": [ { "massMoon": 1.0, "aEarthRadii": 60 } ]
    },
    { "id": "j1", "name": "Bulwark", "role": "other", "type": "gasGiant",
      "orbit": { "aAU": 4.2, "e": 0.04, "incDeg": 0.5 }, "bulk": { "massEarth": 300 } }
  ],
  "level": "student"
}
```

`role` is `hz` (at most two planets) or `other`. Planets other than the HZ worlds are simplified (owner's decision,
§14): they have only **mass, orbit and a type**, `"type": "rocky" | "icy" | "iceGiant" | "gasGiant"`, which sets a
default density (hence radius) and colour and how much they count for stability, impacts, water delivery and the snow
line. They are not habitable worlds here (moons of giants could be, later, §13).

---

## 4. Design parameters, and everything that bears on life

This is the core of the specification. For each parameter: what the user sets, the allowed range in the app, the
Sun or Earth reference value, what it drives downstream, and the thresholds that matter for life and for
humans. **"Set"** means the user controls it; **"Derived"** means the app computes it. All thresholds marked
*(contested)* must be shown as a band with the note "scientists disagree", never as a sharp line (§6.3).

### 4.1 The star (set)

| Parameter | Range in app | Sun | Drives |
|---|---|---|---|
| Mass M★ | 0.08 to 2.0 M☉ (warn > 1.4, outside the HZ model's validity) | 1 M☉ | Luminosity, radius, temperature, colour, lifetime, HZ position, tidal locking, flare activity |
| Age | 0.01 Gyr to the end of the main sequence | 4.57 Gyr | Current luminosity (it brightens), activity (it calms), time already spent in the HZ |
| Metallicity [Fe/H] | −1.0 to +0.5 dex | 0 | How likely rocky planets and giants are, and the Fe/Mg/Si available to the planet's core and mantle (§4.6) |
| Activity | auto (from mass and age) or manual low/normal/high | low today | XUV flux (air loss), flares, stellar wind (magnetosphere, §4.10) |

**Derived:** luminosity L★, radius R★, effective temperature T_eff, spectral class (O to M with subclass),
main-sequence lifetime, peak wavelength, the fraction of light in the 400 to 700 nm band that plants use (PAR),
the XUV history, the HZ boundaries (§5.3), the snow line, the tidal-lock radius.

**Life constraints from the star:**

- **Lifetime.** Earth needed about 4 Gyr from its formation to complex animals, and about 2 Gyr before oxygen
  accumulated. Stars above about 1.5 M☉ leave the main sequence in under ~3 Gyr; A stars in under ~1 Gyr. Show
  the remaining main-sequence lifetime and the time the planet will have spent in the HZ (§5.7).
- **Red dwarfs (M < ~0.5 M☉)** have long, very bright pre-main-sequence phases that can strip water from
  planets that later sit in the HZ (Luger & Barnes 2015), frequent flares, strong XUV for Gyr, and an HZ so close
  that planets become tidally locked. They are a teaching case, not a forbidden one.
- **Hot stars** give more UV, which drives ozone formation but also damage; the HZ sits farther out and the
  zone is wider.
- **Spectrum and photosynthesis.** The same total flux from a cooler star carries less visible light and more
  infrared. Show the PAR fraction and the photon flux in the 400 to 700 nm band, compared with Earth.

### 4.2 The system's architecture (set, partly)

| Parameter | Range in app | Solar System | Drives |
|---|---|---|---|
| Number of planets | 1 to 8 | 8 | Orbital stability, impacts |
| Planets in the HZ | 1 or 2 (hard limit) | 1 (Earth; Mars at the outer edge) | The design brief |
| Giant planet(s) | none, or 1 to 3 with mass and orbit | Jupiter 318 M⊕ at 5.2 AU | Stability, water delivery, impact shielding *(contested)*, perturbations of the HZ planets |
| Asteroid or comet belt | none, inner belt, outer belt | both | Impact rate, late water delivery |

**Derived:** snow line (≈ 2.7 AU × (L★/L☉)^0.5, a heuristic), mutual spacing in Hill radii between neighbours,
resonances (period ratios near small integers), a qualitative impact-risk level.

### 4.3 The orbit (set)

| Parameter | Range in app | Earth | Notes |
|---|---|---|---|
| Semi-major axis a | 0.01 to 50 AU (HZ planets restricted to inside the optimistic HZ ± 30 %) | 1 AU | Sets the incoming flux |
| Eccentricity e | 0 to **0.4 hard cap** (owner: "orbits cannot be excessive") | 0.0167 | Seasons from distance, flux extremes |
| Inclination | 0 to 10° relative to the system plane | 0° (reference) | Stability; large values warned |

**Derived:** orbital period (Kepler: P² = a³ / M★, with P in years, a in AU, M★ in M☉), mean flux
S = L★ / a² in Earth units, **annual-mean flux ∝ 1 / √(1 − e²)**, flux at periastron and apoastron
(ratio ((1+e)/(1−e))²), whether the whole orbit stays inside the HZ, the Hill radius
r_H = a(1 − e)(m / 3M★)^(1/3), and the tidal-locking timescale (§5.8).

Eccentricity thresholds: green ≤ 0.1, amber 0.1 to 0.3, red > 0.3 *or* whenever periastron or apoastron leaves
the optimistic HZ. (Eccentric planets can stay habitable on the annual mean if oceans buffer the swings; show
that as a nuance in the explanation card, not as a pass.)

### 4.4 Spin and orientation (set)

| Parameter | Range in app | Earth | Drives |
|---|---|---|---|
| Day length (sidereal rotation) | 4 h to tidally locked | 23.93 h | Day/night temperature swing, the Coriolis effect and the width of the climate belts, dynamo (weakly), circadian fit for humans |
| Obliquity (axial tilt) | 0° to 90° | 23.44° | Seasons, polar ice, habitable latitudes |
| Obliquity stability | derived | ±1.3° with the Moon | Chaotic obliquity without a large moon (Laskar et al. 1993; later work, Lissauer et al. 2012, finds smaller swings *(contested)*) |

**Tidal locking** is derived (§5.8): if locked, day length = orbital period, and the climate model switches to a
day-side/night-side mode (§5.9).

### 4.5 Moons (set)

| Parameter | Range in app | The Moon | Drives |
|---|---|---|---|
| Number | 0 to 3 | 1 | |
| Mass | 0.001 to 0.1 M⊕ | 0.0123 M⊕ | Tides, obliquity stabilisation |
| Distance | Roche limit to about 0.4 r_H (prograde stability; Domingos et al. 2006) | 60.3 Earth radii | Tidal height (∝ m / d³), angular size in the sky, recession over time |

**Derived:** Roche limit (fluid) d ≈ 2.44 R_p (ρ_p / ρ_moon)^(1/3), tidal range relative to Earth's lunar tide,
whether the moon stabilises the obliquity (a qualitative rule: a moon of more than ~1 % of the planet's mass
within ~100 planet radii), apparent angular size versus the star's (total eclipses possible or not, a nice touch).

### 4.6 Bulk and interior (set)

| Parameter | Range in app | Earth | Drives |
|---|---|---|---|
| Mass M_p | 0.05 to 10 M⊕ | 1 M⊕ (5.97 × 10²⁴ kg) | Everything below |
| Core mass fraction (CMF) | 0 to 0.8 | 0.32 (Mercury ~0.7, Mars ~0.2) | Radius, density, dynamo, mantle depth |
| Water mass fraction (bulk) | 0 to 0.5 | ~0.0002 in the oceans (more in the mantle) | Ocean depth, ocean world or desert, high-pressure ice on the seafloor |
| Radiogenic heating (U, Th, ⁴⁰K) relative to Earth | 0.1 to 3 | 1 | Mantle convection, volcanism, plate tectonics, how long the dynamo lasts |
| Fe/Mg relative to Earth | 0.5 to 2 (default: from the star's metallicity) | 1 | Core size versus mantle, links the star to the planet |

**Derived:**

- **Radius** from a mass–radius relation for a rocky body with the given CMF and water fraction (Zeng et al. 2016;
  Seager et al. 2007; use a table, not a single power law). Rule of thumb: R ≈ M^0.27 for Earth-like composition
  between 1 and 8 M⊕.
- **Bulk density**, **surface gravity** g = GM/R², **escape velocity** v_esc = √(2GM/R).
- **Gas-envelope warning**: above ~1.6 R⊕ (about 5 M⊕ for an Earth-like composition) most known planets carry
  thick hydrogen envelopes (Rogers 2015; the radius valley, Fulton et al. 2017). Mark as likely mini-Neptune,
  not a rocky world.
- **Interior structure** for the cutaway: inner core, outer core, mantle, crust, with depths (a two- or
  three-layer model with representative densities is enough; show it as approximate).
- **Core state and dynamo** *(model with care)*: whether the outer core is liquid and convecting, from a
  parameterised thermal history (heat flow from the core versus the adiabatic conduction limit). Output: dynamo
  on, weak or off over time, and a dipole moment relative to Earth (8 × 10²² A m²). Small planets cool fast and
  lose the dynamo (Mars, about 4 Gyr ago). Use scaling laws (e.g. Olson & Christensen 2006) and label the result
  as an estimate.
- **Tectonic regime** *(contested)*: plate tectonics, stagnant lid or episodic. It depends on mass, water (which
  weakens the lithosphere), mantle temperature and age. Whether super-Earths favour plate tectonics is disputed
  (Valencia et al. 2007 say yes; O'Neill & Lenardic 2007 say no). Show a likelihood band with both views. It
  matters because the carbonate–silicate cycle, the long-term thermostat, needs it (§5.10).
- **Outgassing and volcanism** level over time (feeds CO₂ and water to the atmosphere).

### 4.7 Surface (set)

| Parameter | Range in app | Earth | Drives |
|---|---|---|---|
| Ocean inventory | 0 to 50 Earth oceans (1 ocean = 1.4 × 10²¹ kg) | 1 | Sea level, land fraction, climate buffering, weathering |
| Terrain | seed plus brushes (continents, mountains, basins) | 29 % land | Land fraction, continent placement (polar continents favour ice ages), albedo |
| Land albedo | 0.1 to 0.4 | ~0.25 to 0.3 | Energy balance |

**Derived:** land fraction (from the terrain's hypsometry and the sea level; with Earth-like terrain, land
largely disappears beyond about 2 to 3 Earth oceans, a heuristic), mean ocean depth, **high-pressure ice at the
seafloor** for very deep oceans (it cuts water–rock chemistry: amber for life), surface albedo map, ice cover
(from the climate model).

### 4.8 Atmosphere (set)

Composition as partial pressures at the surface. The total pressure is the sum.

| Gas | Range in app | Earth | Role |
|---|---|---|---|
| N₂ | 0 to 10 bar | 0.78 bar | Bulk, pressure broadening, keeps the total pressure up; narcosis at high partial pressure |
| O₂ | 0 to 2 bar | 0.21 bar | Breathing, ozone, fire; needs a biosphere to persist (it is not geologically stable) |
| CO₂ | 0 to 100 bar | 0.00042 bar (420 ppm) | Greenhouse, weathering cycle, toxic to humans above ~0.5 % at 1 bar |
| H₂O | auto (from temperature; Clausius–Clapeyron) | ~0.01 bar near the surface, variable | The strongest feedback; runaway and moist greenhouse limits |
| Ar | 0 to 1 bar | 0.0093 bar | Inert, a radiogenic tracer (fun fact) |
| CH₄ | 0 to 0.01 bar | 1.9 ppm | Greenhouse; was important on the early Earth |

**Derived:** total surface pressure; atmosphere mass M_atm = P × 4πR² / g (the same pressure needs much more air
on a big planet); mean molar mass; **scale height** H = RT / (μg) (Earth ~8.5 km); greenhouse warming from the
climate model; ozone column (present when pO₂ is above ~1 % of today's, roughly); sky colour; liquid water
possible at all (above the triple point, 611 Pa); the boiling point of water at the surface (it matters for
"water world in a thin atmosphere").

**Retention** (the "can it hold its air" question, central to the teaching):

- **Jeans escape parameter** λ = v_esc² / v₀², with v₀ = √(2kT_exo / m), for each gas, at the exosphere
  temperature T_exo (Earth ~1000 K, rising with the star's XUV). Thresholds for N₂ and O₂: green λ ≥ 50, amber
  20 to 50, red < 20. Hydrogen escapes Earth (λ ~ 7); that is fine and expected, but it is how water is lost if
  water vapour reaches the upper atmosphere (moist greenhouse, §5.3).
- **Hydrodynamic and XUV-driven escape** for strong irradiation: an energy-limited escape estimate (Watson et al.
  1981; Lammer et al. 2009) over the star's XUV history, giving atmosphere mass lost over time.
- **The cosmic shoreline** (Zahnle & Catling 2017): an empirical line in the plane of escape velocity versus
  cumulative XUV (or insolation) that separates bodies with and without atmospheres. Plot the design on it,
  together with the Solar System bodies. This is one of the best single teaching charts in the app.
- **Stellar-wind stripping and the magnetic field** *(contested)*: a magnetosphere is widely held to protect the
  atmosphere, but measured escape rates from Earth, Venus and Mars are similar (Gunell et al. 2018). Show both
  views.

### 4.9 Climate (derived)

| Output | Human/life relevance |
|---|---|
| Equilibrium temperature T_eq = T_eff √(R★ / 2a) (1 − A)^(1/4) | Earth 255 K with albedo 0.3: the baseline before the greenhouse effect |
| Global mean surface temperature | Liquid water; Earth 288 K (15 °C) |
| Temperature by latitude and season (energy balance model, §5.9) | Where humans could live, ice lines, seasons |
| Day–night contrast (from day length and pressure; for locked planets, a day side and a night side) | Survivability, a terminator zone for locked worlds |
| Ice cover and ice-albedo feedback (snowball states, which have hysteresis) | Snowball Earth episodes happened; the app should be able to produce one |
| Runaway or moist greenhouse states | Venus |
| Maximum wet-bulb temperature | Human survival limit (§4.11) |
| Habitable surface fraction (land and sea, for microbes; land in the human comfort range, for people) | The headline numbers |

### 4.10 Radiation and the space environment (derived)

| Output | Reference | Relevance |
|---|---|---|
| Surface UV (UV index, using the ozone column) | Earth peak UV index 10 to 12 in the tropics | DNA damage, sunburn |
| Surface particle radiation dose (galactic cosmic rays and stellar particles, shielded by the atmosphere's column mass and the magnetic field) | Earth ~2.4 mSv/yr in total from natural sources; Mars surface ~0.64 mSv/day (Hassler et al. 2014) | Humans: ICRP occupational limit 20 mSv/yr |
| Magnetopause stand-off distance (pressure balance between the field and the stellar wind) | Earth ~10 Earth radii | Red-dwarf HZ planets may have compressed magnetospheres |
| Flare frequency and energy | Sun: rare large flares | Red dwarfs: frequent; ozone destruction, UV spikes |

### 4.11 Life, and humans in particular (derived thresholds)

**For life in general** (liquid-water, carbon-based life as on Earth; other chemistries are out of scope,
§14):

| Criterion | Green | Amber | Red |
|---|---|---|---|
| Liquid water on the surface | stable oceans or lakes | only seasonal, or only under ice | none |
| Time in the continuously habitable zone (§5.7) | ≥ 4 Gyr | 1 to 4 Gyr | < 1 Gyr |
| Climate thermostat (carbonate–silicate cycle: land + water + tectonics) | active | partial *(contested)* | absent |
| Energy for photosynthesis (PAR photon flux relative to Earth) | ≥ 0.5 | 0.1 to 0.5 | < 0.1 |
| Elements for life (C, H, N, O, P, S) | present (default) | — | the "dry, carbon-poor" toggle at Scientist level |
| Atmosphere retention (§4.8) | λ ≥ 50 | 20 to 50 | < 20 |
| Surface radiation (UV + particles) | shielded | strong (life in the sea, or underground) | sterilising at the surface |

**For humans without a suit** (the strict tier). Sources are listed in §16; where the data are thin, say so:

| Criterion | Green | Amber | Red | Note |
|---|---|---|---|---|
| Surface gravity | 0.8 to 1.2 g | 0.5 to 0.8 or 1.2 to 1.5 g | < 0.5 or > 1.5 g | No long-term human data away from 1 g. Low g costs bone and muscle; high g strains the heart and the joints. Label this "best guess". |
| O₂ partial pressure | 16 to 30 kPa | 11 to 16 kPa (like 2 to 5 km altitude) or 30 to 50 kPa | < 11 or > 50 kPa (long-term pulmonary toxicity) | Sea level on Earth 21 kPa |
| O₂ fraction (fire) | < 25 % | 25 to 35 % | > 35 % (fires very hard to control) | A fire risk, not a breathing limit; the thresholds are approximate |
| CO₂ partial pressure | < 0.5 kPa | 0.5 to 3 kPa | > 3 kPa | 0.5 % at 1 bar is the 8-hour occupational limit; above ~3 % breathing is forced, ~5 % is dangerous. Note the tension: a cold planet may need CO₂ for warmth that humans cannot breathe. |
| N₂ partial pressure | < 2 bar | 2 to 3 bar | > 3 bar (narcosis) | Divers feel narcosis from ~3 to 4 bar |
| Total pressure | 0.5 to 2 bar | 0.3 to 0.5 or 2 to 5 bar | < 6.3 kPa (the Armstrong limit: body fluids boil) or > 5 bar | Breathability is judged on the partial pressures above |
| Global mean temperature | 5 to 25 °C | −15 to 5 or 25 to 35 °C | outside | Earth 15 °C |
| Maximum wet-bulb temperature in inhabited zones | < 31 °C | 31 to 35 °C | > 35 °C | 35 °C is the theoretical limit (Sherwood & Huber 2010); measured tolerance in young adults is nearer 31 °C (Vecellio et al. 2022) |
| Day length | 20 to 28 h | 12 to 20 or 28 to 72 h | < 12 h, > 72 h, or locked | The human clock entrains to roughly ±1 to 2 h around 24 h; artificial light helps. Locked planets: amber at the terminator, red elsewhere. |
| Radiation dose at the surface | < 5 mSv/yr | 5 to 20 mSv/yr | > 20 mSv/yr | 20 mSv/yr is the ICRP occupational limit |
| UV | ozone layer (UV index < 12) | UV index 12 to 20 | no ozone | |
| Habitable land | ≥ 10 % of the surface is land in the comfort range | 1 to 10 % | < 1 % (ocean world) | |
| Fresh water | from rain on land (automatic on an Earth-like climate) | marginal | none | |

---

## 5. Models and equations

**Rule: every model is a pure function in `physics/`, in SI units, with a unit test against the benchmarks of
§5.12, and the source quoted in a comment.** The app never calls a model outside its validity range without
showing "outside the model's range" next to the result.

### 5.1 Main-sequence stars

Use the tabulated mean dwarf sequence of **Pecaut & Mamajek (2013, updated table maintained by E. Mamajek)** for
T_eff, L, R and spectral class versus mass, interpolated in log space. Do not derive these from power laws alone.
For orientation (approximate rows; take the real values from the table):

| Class | Mass (M☉) | T_eff (K) | L (L☉) |
|---|---|---|---|
| F5V | ~1.33 | ~6500 | ~3 |
| G2V | 1.00 | ~5770 | 1.00 |
| K5V | ~0.70 | ~4400 | ~0.15 |
| M0V | ~0.57 | ~3850 | ~0.07 |
| M5V | ~0.16 | ~3050 | ~0.003 |

Main-sequence lifetime: t_MS ≈ 10 Gyr × (M/M☉)^−2.5 as a first estimate (it is crude for M > 1.5 M☉ and
irrelevant for red dwarfs, which outlive the Universe's present age). Luminosity versus age for Sun-like stars
follows **Gough (1981)**: L(t) = L_now / [1 + 0.4 (1 − t / t_now)], with t measured in units of the star's own
main-sequence lifetime (for the Sun, t_now = 4.57 Gyr). Red dwarfs: add a pre-main-sequence phase with a declining luminosity (Baraffe et al. 2015 tracks, or
a fitted curve), because it drives the early water loss (Luger & Barnes 2015).

XUV: saturated at L_XUV / L_bol ≈ 10^−3 while the star is young, then declining as a power law of age; red dwarfs
stay saturated longer (Jackson et al. 2012). Label it an estimate.

Star colour for rendering: black body at T_eff converted to sRGB (§10.2).

### 5.2 Orbits and fluxes

Kepler's third law; flux S = L / (4πa²); annual mean ∝ 1/√(1−e²); positions from Kepler's equation for the
orrery (Newton iteration, no N-body integration in the default mode).

### 5.3 The habitable zone

**Kopparapu et al. (2013, ApJ 765:131, with its erratum) and Kopparapu et al. (2014, ApJL 787:L29)**:
S_eff = S_eff☉ + aT + bT² + cT³ + dT⁴ with T = T_eff − 5780 K, valid for 2600 ≤ T_eff ≤ 7200 K, and the distance
d = √(L / S_eff) AU. Five limits: recent Venus (optimistic inner), runaway greenhouse (conservative inner), moist
greenhouse, maximum greenhouse (conservative outer), early Mars (optimistic outer). The 2014 paper gives the
runaway-greenhouse limit for 0.1, 1 and 5 Earth masses: use the planet's mass.

The coefficients must be copied from the papers (or a trusted tabulation) and checked by the benchmark test
(§5.12: for the Sun, inner conservative ≈ 0.95 to 0.99 AU, outer conservative ≈ 1.67 AU, recent Venus ≈ 0.75 AU,
early Mars ≈ 1.77 AU). Do not type them from memory.

Outside 2600 to 7200 K: extrapolate, but grey out the ring and say "outside the model".

### 5.4 Planet structure

Mass–radius from **Zeng et al. (2016)** (tables for rocky compositions by CMF, with water layers) or
**Seager et al. (2007)**. Interpolate a precomputed table; do not solve the structure equations in the browser.
Layer boundaries for the cutaway from the same model, or from a two-layer approximation with a stated error.

### 5.5 Atmospheric escape

Jeans parameter per gas (§4.8); energy-limited XUV escape (mass loss rate = ε π R³ F_XUV / (G M), with an
efficiency ε ≈ 0.1 to 0.3, stated); the cosmic-shoreline chart from Zahnle & Catling (2017). The integrated loss
over the XUV history decides the atmosphere's survival in the time machine.

### 5.6 Orbital stability and two habitable planets

- Two planets on near-circular orbits are Hill stable if their separation exceeds 2√3 ≈ 3.46 mutual Hill radii
  (Gladman 1993), with R_H,m = [(m₁+m₂)/(3M★)]^(1/3) (a₁+a₂)/2.
- Systems of three or more planets need ≳ 8 to 10 mutual Hill radii to survive for Gyr (Chambers et al. 1996).
- Thresholds: green ≥ 10, amber 3.46 to 10, red < 3.46 or crossing orbits.
- There is plenty of room for two Earth-mass planets in the Sun's HZ (around 1.0 and 1.4 AU they are about 25
  mutual Hill radii apart), less around red dwarfs (TRAPPIST-1 packs three or four into its HZ in resonant
  chains). The orrery lights the second HZ slot only where a stable spacing exists.
- Optional later: a short N-body check (Wisdom–Holman or a symplectic leapfrog in a worker) for the Scientist
  level. It is heavy, so it runs only on request (§10.6).

### 5.7 The continuously habitable zone

The range of distances that stays inside the HZ from some starting age (e.g. 1 Gyr after formation, to allow
for the early bombardment) until now, or for a chosen duration, as the star brightens. For the planet: how many
Gyr it has spent, and will spend, inside the zone. This number is the honest answer to "is there time for life?"

### 5.8 Tidal locking

Despinning time t_lock ≈ ω a⁶ I Q / (3 G M★² k₂ R⁵) (Gladman et al. 1996), with I ≈ 0.33 M_p R² for a rocky planet,
Q ≈ 100, k₂ ≈ 0.3, and ω the initial spin (a day of about 10 to 13 h). Locked if t_lock < age. The result is
uncertain by orders of magnitude through Q; show it as a band. As a rule, HZ planets of stars below ~0.5 to 0.6
M☉ lock within the star's age. Eccentric planets can end in spin–orbit resonances (Mercury's 3:2): mention it,
do not model it in v1.

### 5.9 Climate: a latitudinal energy balance model

A one-dimensional, seasonal **energy balance model** (North 1975; North, Cahalan & Coakley 1981; with the
extensions of Williams & Kasting 1997 for other planets): temperature T(latitude, time) from

C ∂T/∂t = S(φ, t)(1 − α(T)) − OLR(T, pCO₂) + ∂/∂x[D(1 − x²) ∂T/∂x], with x = sin(latitude)

- Outgoing longwave radiation: a linear form OLR = A + B·T (Budyko; A ≈ 203 W m⁻², B ≈ 2.09 W m⁻² K⁻¹ with T in
  °C, for Earth), with the CO₂ forcing added as −5.35 ln(pCO₂ / 280 ppm) W m⁻² (Myhre et al. 1998), or the
  polynomial fits of Williams & Kasting (1997) for a wider CO₂ range. State the valid range.
- Albedo: ice when T < −10 °C (≈ 0.6), ocean or land otherwise (≈ 0.3), with a smooth step; this gives the
  ice–albedo feedback and the snowball hysteresis.
- Heat capacity from the ocean fraction (mixed layer ~50 m); diffusion D scaled with pressure, rotation and heat
  capacity following Williams & Kasting (1997) (slower rotation, more transport).
- **Runaway-greenhouse check**: if the absorbed stellar flux S(1 − A)/4 exceeds the runaway limit of
  ≈ 282 W m⁻² for an Earth-like planet (Goldblatt et al. 2013), the EBM is not valid and the planet is flagged
  "runaway greenhouse": the oceans evaporate (Venus). This takes priority over the EBM's temperature.
- **Tidally locked mode**: a two-box (day/night) or a coarse substellar-angle model, with heat transport rising
  with pressure (thick atmospheres equalise; thin ones collapse onto the night side). Label it qualitative.
- Water vapour: relative humidity fixed (about 0.8) for the surface value and the wet-bulb temperature.

It runs in a Web Worker: a seasonal run to a steady cycle takes about 1 s, which is fast enough for live updates
on the PC but not on every slider tick in VR (§10.6).

### 5.10 The carbonate–silicate thermostat

Weathering removes CO₂ faster when it is warm and wet: W ∝ exp((T − T₀) / T_e) with T_e ≈ 13 K (Walker, Hays &
Kasting 1981; Abbot et al. 2012 for land fraction), balanced against volcanic outgassing from §4.6. At steady state
this sets pCO₂, which feeds the EBM. In the time machine it is what keeps the climate stable while the star
brightens, if there is land, water and active tectonics. Without them the CO₂ is whatever the user set and
the climate drifts. This is one of the key lessons; show the feedback loop on the Causality Map.

### 5.11 Composite indices (optional, never the headline)

The **Earth Similarity Index** (Schulze-Makuch et al. 2011) from radius, bulk density, escape velocity and
surface temperature, with the weights given in the paper (check them against it). Show it in a corner with the
note that it measures similarity to Earth, not habitability. **The headline is the Ledger (§6), not a score.**

### 5.12 Benchmarks (unit tests, required)

| Test | Expected |
|---|---|
| Sun, 4.57 Gyr: L, T_eff, R | 1 L☉, ~5772 K, 1 R☉ |
| Sun's HZ (Kopparapu) | recent Venus ~0.75 AU, runaway ~0.95 to 0.99 AU, max greenhouse ~1.67 AU, early Mars ~1.77 AU |
| Earth: g, v_esc, T_eq (A = 0.3), scale height | 9.81 m/s², 11.19 km/s, ~255 K, ~8.5 km |
| Mars: g, v_esc | 3.71 m/s², 5.03 km/s |
| Earth's EBM at 420 ppm, obliquity 23.44°, 1 AU | global mean ~15 °C, ice lines near 70° |
| Earth's EBM with S × 0.9 from an ice-free start | stays warm; from a frozen start: stays frozen (hysteresis) |
| Venus-like: S = 1.9 S⊕ | runaway flag |
| Earth–Moon: Roche limit (fluid) | ~2.9 R⊕ (~18 000 km) |
| Two 1 M⊕ planets at 1.0 and 1.4 AU around 1 M☉ | ~25 mutual Hill radii, green |
| Jeans λ for N₂ on Earth at 1000 K | ~200 (retained); H at 1000 K: ~7 (escapes) |
| 0.3 M☉ star | HZ ~0.1 AU, locked within age, flare warning |

---

## 6. The Habitability Ledger

The verdict is a board of criteria, not a number. Each row: the criterion, its value, the green/amber/red
status, one line "because …" (the chain of causes, clickable into the Causality Map) and the source.

### 6.1 Tiers

The ledger groups criteria into five tiers, each with its own lamp:

1. **Liquid water is possible** (in the HZ, pressure above the triple point, not runaway).
2. **Habitable for microbes** (1 + time in the CHZ + atmosphere retained + some shielding).
3. **Habitable for complex life** (2 + ≥ 4 Gyr + oxygen possible + a climate thermostat + moderate obliquity
   swings + stable orbit).
4. **Habitable for humans without a suit** (3 + every row of the human table in §4.11).
5. **Earth twin** (4 with every row green, plus the moon and the ocean/land balance).

A design can fail a tier for one reason; the lamp says which.

### 6.2 Explanation cards

Each row opens a short card: what the criterion is, why it matters, what Earth, Mars and Venus do, and one
thing to try ("move the planet 0.1 AU outward and watch the ocean"). Written for the Explorer level first, with a
"more" fold for Student and Scientist (the equation, the source, the uncertainty).

### 6.3 Contested science

Rows marked *(contested)* in §4 show a hatched band and a "scientists disagree" chip that opens both views with
their sources. This is a feature, not a weakness: students should see where knowledge stops.

---

## 7. The Causality Map

A node-link diagram of the parameters and the derived quantities, laid out left to right: **star → orbit → planet
bulk → interior → surface → atmosphere → climate → life → humans**. Selecting a node lights its downstream
effects. Changing a parameter animates a pulse along the edges it touches, so the user sees *what* a change did,
not just the final numbers. Nodes carry their current value and colour. On the PC it is a panel; in VR it is the
big board on the Instrument Wall. Leader lines and labels sit beside the nodes, never over them.

A minimal edge list to start with (extend freely):

- M★ → L★, T_eff, lifetime, HZ, tidal-lock radius, activity
- age → L★(t), activity(t), time in the CHZ
- a, e, L★ → flux → T_eq, climate, HZ membership
- M_p, CMF, water → R → g, v_esc, density
- g, v_esc, XUV → atmosphere retention → pressure
- pressure, composition, flux → greenhouse → T_surface
- M_p, radiogenic heat, age → mantle convection → tectonics → carbonate–silicate cycle → pCO₂ → T_surface
- core size and cooling → dynamo → magnetic field → radiation at the surface, (contested) atmosphere loss
- day length, obliquity, moon → seasons, climate belts, circadian fit
- pO₂ → ozone → UV
- everything → the Ledger rows

---

## 8. Time evolution (the Time Machine)

A timeline from 0 to the end of the star's main sequence (capped at ~15 Gyr for display), with markers: now,
when the planet entered the HZ, when it will leave, dynamo shutdown, oceans lost, oxygenation possible.

Tracked: L★(t), XUV(t), HZ(t), the planet's flux, the interior heat flow and dynamo, outgassing, atmosphere mass
(escape integrated), pCO₂ from the weathering cycle, the EBM mean temperature, the ocean inventory (lost to a
moist or runaway greenhouse), the obliquity band.

It is a **heavy computation**: it runs in a worker, step by step, and is shown as a set of curves on the
Instrument Wall plus a "scrub" handle that plays the planet's appearance (ice, oceans, sky) through time.

The famous lessons it must reproduce: the faint young Sun (the early Earth needed more greenhouse gas), Mars
losing its dynamo and air, Venus losing its oceans, the Earth leaving the HZ in roughly 1 to 2 Gyr as the Sun
brightens (estimates differ with the model), red-dwarf planets dried out before the star settled.

---

## 9. Teaching design

- **Free design** is the default.
- **Challenges** (each with a goal, a starting system and the ledger tier to reach):
  1. *A second Earth around a K dwarf* (the "easy" star: long-lived, calm, wide enough HZ).
  2. *Two planets in the habitable zone* (stable spacing, both green at tier 3).
  3. *Rescue Mars*: start from a Mars clone and find the smallest set of changes that makes it tier 3.
  4. *Why did Venus fail?* Start from a Venus clone; move it, change its water, its CO₂.
  5. *Life under a red dwarf*: a tidally locked world with a habitable terminator.
  6. *The super-Earth question*: a 4 M⊕ planet, and what more mass costs and buys.
  7. *Snowball*: freeze an Earth and get it out again (the hysteresis lesson).
  8. *Design for humans*: tier 4 with the fewest amber rows.
- **Misconception checks** woven into the cards: seasons come from tilt, not distance; "in the HZ" does not mean
  "habitable"; bigger is not safer; a magnetic field is not obviously required *(contested)*; oxygen comes from
  life, not from rocks.
- **Teacher notes** in a `docs/` page: learning goals per challenge, the models used and their limits, the
  sources. English first; Danish when Peter decides (§14).
- **Reports**: export the design with its ledger as a one-page PNG/PDF (reuse the Illusion Studio export path).

---

## 10. Look and rendering

### 10.1 The workshop

Its own look: a huge, quiet hall of dark metal and glass, warm work lights, the star's light flooding in from
the Forge, instruments glowing in the dark. As a quiet nod to the sibling app, the floor of the Orrery bay can
carry a slow-drift pattern from Illusion Studio (owner's choice).

### 10.2 The star

A sphere with limb darkening, granulation noise and a corona glow, coloured by the black-body colour of T_eff
(use a CIE 1931 colour-matching integration converted to sRGB and normalised, or a fitted table). It lights the
scene with that colour and an intensity that is clamped for comfort (never blinding in VR).

### 10.3 The planet

- A cube-sphere mesh (no pole pinching) with a procedural height field plus the user's brush layer, oceans at the
  sea level, ice from the climate model, clouds as a separate shell. Biome colours from temperature and
  precipitation proxies once life is possible (Scientist level can switch it off: "no life assumed").
- An atmosphere shell with single-scattering Rayleigh (and simple Mie) whose colour comes from the star's
  spectrum and the column density: thin air looks dark, thick air white, a red dwarf's sky is not Earth's blue.
- The cutaway: a clipping plane with capped layers, each layer with its colour and a scale bar.
- Exaggeration factors (terrain, atmosphere) are always stated next to the planet.

### 10.4 Charts

HZ diagram (flux versus T_eff, with the Solar System's planets and well-known exoplanets as reference points),
the cosmic shoreline, the EBM's temperature by latitude and season (a heat map with a choice of colour scales,
viridis family by default), the timeline curves. All with axes, units and scale bars (owner rule).

### 10.5 Performance budget (Quest 3)

72 Hz minimum. Keep draw calls low (one planet mesh, one atmosphere, one cloud shell, the orrery instanced),
shaders modest, the hall mostly baked lighting. Measure with a frame-time readout in a debug mode. The heavy
models never run on the render thread.

### 10.6 Heavy computations

Owner rule from earlier projects: **heavy views must not recompute behind the user's back.** When inputs
change, the old result stays visible, greyed out, with a round ↻ button in the middle; it recomputes when the
button is pressed. Auto-follow may exist as an opt-in. This applies to the seasonal EBM in VR, the time machine
and any N-body check. Closed-form results (§5.1 to §5.8) update live.

---

## 11. Interaction

### 11.1 VR

Reuse the Illusion Studio patterns (§15), improved:

- **Physical tools first, panel second.** Dials, levers, canisters, the cutaway knife and the orrery beads do
  the main design moves. The owner prefers hand gestures and few tools. The panel holds the exact numbers and
  the less common settings.
- **The panel**: a tablet with tabs (Star, Orbit, Planet, Interior, Surface, Air, Moons, Ledger, View), opened
  by Y/B or a wrist button, used by ray and trigger or by finger poke; − and + buttons on every slider with
  auto-repeat; destructive actions need a second press.
- **One UI schema for both platforms.** In Illusion Studio the desktop sidebar and the VR panel are written
  twice. Here, describe each tab once (a declarative list of rows: slider, toggle, chips, readout, with the
  parameter path, range, unit, level), and render it to the DOM on the PC and to the canvas panel in VR.
- **Movement**: grab the world (one hand moves, two hands scale and turn), left stick flies, right stick turns.
  **No controller button may reset the view; only the Meta button recentres** (owner rule).
- **Comfort**: no forced camera motion. Transitions between bays and the Lander are fades or a quick scale
  change with a stable horizon.

### 11.2 PC

Left: the tabbed parameter panel (same schema). Centre: the 3D view of the current bay with an orbit camera.
Right: the Ledger. Bottom (collapsible): the charts and the timeline. Keyboard: number keys for bays, `L` for
the Lander, `T` for the time machine.

---

## 12. Architecture and files

Follow the `vr-apps` conventions (plain JavaScript modules, three.js r170 vendored in `vendor/`, no build step,
cache-busting `?v=N` on every import, raised together), unlike Illusion Studio, which is one file using a CDN.

```
vr-apps/planet-designer/
  index.html            page shell, loads main.js?v=N
  main.js               boot, renderer, loop, the desktop/XR switch
  design.js             the design JSON: defaults, validation, migration, save/load, share link
  ui/schema.js          the one UI description (tabs, rows, parameter paths, ranges, levels)
  ui/dom.js             desktop renderer of the schema
  ui/vrpanel.js         canvas panel renderer of the schema (ported from Illusion Studio)
  xr/rig.js             dolly rig, scales, grab-the-world, sticks (ported from Illusion Studio)
  xr/tools.js           dials, levers, canisters, the cutaway knife, orrery handles
  scene/workshop.js     the hall and its bays
  scene/star.js, planet.js, atmosphere.js, orrery.js, lander.js
  physics/star.js       main sequence table, evolution, XUV, colour
  physics/hz.js         Kopparapu limits, CHZ
  physics/orbit.js      Kepler, fluxes, Hill and stability, tidal locking, moons
  physics/structure.js  mass–radius tables, layers, g, v_esc
  physics/interior.js   thermal history, dynamo, tectonic regime (with uncertainty bands)
  physics/atmosphere.js pressure, scale height, escape, shoreline, ozone
  physics/climate.js    the EBM (runs in a worker), runaway check, locked mode
  physics/ledger.js     criteria, tiers, explanations, sources
  physics/worker.js     the worker entry for the heavy models
  data/                 star table, mass–radius tables, HZ coefficients, reference bodies (with sources)
  test/physics.test.mjs benchmarks of §5.12 (node, no browser)
  test/flat.test.mjs    flat-preview checks (Playwright or puppeteer, software GL)
  test/xr.test.cjs      emulated Quest 3 (IWER), from Illusion Studio's tests/vr-test.cjs
  README.md             the app's own notes
```

`physics/` imports nothing from three.js: pure functions, testable in node. The data tables carry their source
and version in a header.

---

## 13. Milestones

Each milestone ends with a PR (owner rule: branch, PR, wait for a yes), a flat-preview test, and an honest list of
what was not tested in the headset.

| | Content | Done when |
|---|---|---|
| **M0** | `physics/` star, HZ, orbit, structure, atmosphere basics; the design JSON; node benchmark tests; a plain desktop page with the parameter panel and the Ledger as text | All §5.12 benchmarks pass except the EBM rows |
| **M1** | Desktop 3D: the star (colour), the orrery with HZ rings and draggable planets, two HZ slots with the stability rule | A K-dwarf system with two stable HZ planets can be built with the mouse |
| **M2** | The planet: cube-sphere terrain, oceans, cutaway with layers, gravity and escape-velocity readouts, the mass–radius tables | The cutaway shows the right layer depths for Earth and Mars |
| **M3** | Atmosphere and climate: retention, shoreline chart, the EBM worker, runaway check, sky colour | EBM benchmarks pass; a Venus clone flags runaway |
| **M4** | The full Ledger with tiers, explanation cards and the Causality Map | Every row has a card and a source |
| **M5** | VR: the workshop, the rig, the schema-driven panel, the Star Forge dial and the orrery in hand | The IWER test passes; ready for the owner's headset test |
| **M6** | Time machine: evolution of star, interior, atmosphere, weathering | Mars and Venus histories reproduce qualitatively |
| **M7** | The Lander: standing on the surface, the sky, the gravity drop, the life-support strip | |
| **M8** | Challenges, teacher notes, report export, Danish texts if wanted | |
| Later | Binary stars (S- and P-type HZ), habitable moons of giants, an N-body check, mixed reality on a real table, a shared classroom mode (Supabase, as in the Atomic Workbench) | |

Publish as a card on `vr-apps/index.html` from M5 (tag "Test"), and run `node tools/sync-site.js` after adding the
page (see `vr-apps/CLAUDE.md`).

---

## 14. Decisions by the owner (23 November 2026)

The six open questions were answered; they are binding for the build.

1. **Language: both, with a toggle** (Danish and English). Keep every text in a string table from M0 so nothing is
   hard-coded in one language; the ledger cards and the teacher notes need both too.
2. **Name: Planet Designer.** Folder `vr-apps/planet-designer/`, app id `"PlanetDesigner"` in the design JSON.
   Because the app is now called "Designer", the middle level needs another name (see 3).
3. **Audience: upper secondary students (gymnasium) *and* university students in sustainability and in
   physics, with several levels.** Consequences:
   - The three levels stay, but the middle one is renamed so it does not clash with the app's name; proposal
     **Explorer / Student / Scientist** (Danish: *Opdager / Studerende / Forsker*). Ask the owner to confirm the
     names when M0 shows them.
   - The sustainability students make the climate side matter as much as the astrophysics: the carbon–silicate
     thermostat (§5.10), CO₂ forcing, ice–albedo feedback, the runaway greenhouse and the human wet-bulb limits
     deserve good explanation cards and at least one challenge each (§9). A link from "why Earth works" to "what
     we are changing on Earth" is welcome, kept factual.
   - The physics students get the equations, units and sources in the Scientist fold, and the benchmark numbers.
4. **Life: water and carbon only, at first.** "Other chemistries" stays in Later (§13).
5. **Impossible designs are allowed and flagged red**, never blocked. The hard limits that remain are the ones
   the owner set himself: eccentricity ≤ 0.4 and at most two planets in the HZ.
6. **Planets other than the HZ worlds: mass, orbit and type only** (rocky, icy, ice giant, gas giant), see §3.

---

## 15. Handover from Illusion Studio (what to reuse and what was learned)

**Illusion Studio** (`vr-apps/illusion-studio/index.html`, live as a "Test" card, commit 17000bc, 23 November
2026) builds 3D scenes covered in self-moving optical-illusion patterns, exports JPGs up to 3000 px in print,
social and banner formats, and has a WebXR mode. It is one HTML file with three.js 0.164.1 and three-mesh-bvh
0.7.8 from jsDelivr. The owner's master copy lives on the owner's PC; the repo copy is the published one. Tell
the owner before changing it here, so the two do not drift.

**State:** tested in an emulated Quest 3 (IWER, 23 of 24 checks; the one failure was in the test's own aim),
**not yet tried in a real headset**. Things to ask the owner about: frame rate (the pattern shader is heavy at
headset resolution; there is a "VR render quality" slider), the position of the wrist button (placed from the
controller and hand specifications without seeing it), comfort when flying and when scaling the world.

**Reusable pieces (copy and adapt):**

- **The user rig with world scale.** Everything personal (camera, controllers, hands, panel, wrist button) is a
  child of a "dolly" group whose scale is 1/S. S < 1 is a dollhouse, S = 1 life size, S > 1 makes you small.
  `placeLocalAt(localPoint, worldPoint, yaw, S)` solves the rig so that a point in your space sits on a point in
  the world; everything else (two-hand grab and scale, snap turn about the head, standing on the surface below)
  is built on it. Because the panel lives in rig space it keeps its physical size at every scale.
- **Grab the world**: one hand keeps the grabbed world point in the hand; two hands keep both points, so their
  distance sets the scale and their direction the yaw. No feedback loop, because the targets are stored in world
  coordinates at the start of the grab.
- **The canvas panel**: a 1200 × 810 canvas texture on a 0.80 × 0.54 m plane, tabs, a scroll column with
  ▲ ▼ and a thumb, sliders with − and +, chips, toggles, list rows with inline buttons, a second press for
  destructive actions, ray hits by UV, drag capture by intersecting the panel's plane, finger poke with press and
  release depths (1 cm in, 2.2 cm out), hover by widget key so a rebuild does not lose it.
- **Carrying objects**: the object follows the controller rigidly (relative matrix stored at the start); its
  group matrix moves during the drag and the geometry is only rebuilt on release; the stick pushes and pulls
  along the ray.
- **BVH picking** (three-mesh-bvh) for rays against dense meshes; build the bounds tree lazily after a rebuild.
- **A render-on-demand desktop loop and an XR animation loop that never run together**; rebuilds in XR are
  throttled (~70 ms) so slider drags do not stall frames.
- **Export at full resolution**: resize the renderer to the export size, render once, read `toDataURL`
  synchronously, restore. A field of view that spans the long side keeps wide formats a slice of the same view.
- **Test recipe**: `vr-apps/illusion-studio/tests/vr-test.cjs` (node; it expects IWER and puppeteer-core in
  folders on the owner's PC, see its header: adapt the two `require` paths to wherever they are installed):
  serve the page with IWER injected, enter VR, aim the
  emulated controllers at panel widgets (a helper returns a widget's position in rig space), press buttons, move
  sticks, check state, take screenshots. Software GL runs at a few frames per second: thresholds must allow
  for it.

**Lessons (do not repeat):**

- With `renderer.xr.enabled`, any desktop render requested while presenting breaks the frame: guard the
  on-demand render path with "if in XR, the XR loop renders".
- `renderer.setSize` must not be called while presenting (guard the resize observer).
- Mixed reality needs `scene.background = null`, no fog and a clear alpha of 0; restore them on exit.
- After the session ends, restore the desktop camera yourself: XR overwrote its pose and field of view.
- `XRControllerModelFactory` and the hand models load from a CDN; in tests, 404s for profiles are expected noise.
- UVs in physical units with an integer number of pattern periods around closed directions avoid seams; a
  sphere with the pole axis horizontal keeps the poles out of typical views. (Relevant for planet textures:
  prefer the cube-sphere.)
- Heavy procedural fragment shaders cost a lot at Quest resolution: start Planet Designer with a performance
  budget and a frame-time readout (§10.5).

---

## 16. Sources (check each before quoting numbers in the app)

- Kopparapu, R. K. et al. 2013, ApJ 765, 131 (and erratum); Kopparapu et al. 2014, ApJL 787, L29: habitable zones.
- Kasting, J. F., Whitmire, D. P. & Reynolds, R. T. 1993, Icarus 101, 108: the classic HZ.
- Pecaut, M. J. & Mamajek, E. E. 2013, ApJS 208, 9 (and the maintained online table): main-sequence properties.
- Gough, D. O. 1981, Solar Physics 74, 21: the Sun's luminosity evolution.
- Baraffe, I. et al. 2015, A&A 577, A42: low-mass stellar tracks.
- Jackson, A. P., Davis, T. A. & Wheatley, P. J. 2012, MNRAS 422, 2024: X-ray activity versus age.
- Luger, R. & Barnes, R. 2015, Astrobiology 15, 119: water loss around M dwarfs.
- Zeng, L., Sasselov, D. & Jacobsen, S. 2016, ApJ 819, 127; Seager, S. et al. 2007, ApJ 669, 1279: mass–radius.
- Rogers, L. A. 2015, ApJ 801, 41; Fulton, B. J. et al. 2017, AJ 154, 109: the rocky limit and the radius valley.
- Zahnle, K. J. & Catling, D. C. 2017, ApJ 843, 122: the cosmic shoreline.
- Watson, A. J., Donahue, T. M. & Walker, J. C. G. 1981, Icarus 48, 150; Lammer, H. et al. 2009, A&ARv 17, 181:
  escape and "what makes a planet habitable".
- Gunell, H. et al. 2018, A&A 614, L3: magnetic fields and atmospheric escape *(the contested view)*.
- Gladman, B. 1993, Icarus 106, 247; Chambers, J. E., Wetherill, G. W. & Boss, A. P. 1996, Icarus 119, 261:
  orbital stability.
- Gladman, B. et al. 1996, Icarus 122, 166: tidal despinning.
- Domingos, R. C., Winter, O. C. & Yokoyama, T. 2006, MNRAS 373, 1227: stable moon orbits.
- Laskar, J., Joutel, F. & Robutel, P. 1993, Nature 361, 615; Lissauer, J. J., Barnes, J. W. & Chambers, J. E.
  2012, Icarus 217, 77: obliquity and the Moon.
- North, G. R. 1975, J. Atmos. Sci. 32, 2033; North, G. R., Cahalan, R. F. & Coakley, J. A. 1981, Rev. Geophys.
  19, 91; Williams, D. M. & Kasting, J. F. 1997, Icarus 129, 254: energy balance models.
- Myhre, G. et al. 1998, GRL 25, 2715: CO₂ forcing.
- Goldblatt, C. et al. 2013, Nature Geoscience 6, 661: the runaway greenhouse limit.
- Walker, J. C. G., Hays, P. B. & Kasting, J. F. 1981, JGR 86, 9776; Abbot, D. S. et al. 2012, ApJ 756, 178:
  the weathering thermostat.
- Valencia, D., O'Connell, R. J. & Sasselov, D. D. 2007, ApJL 670, L45; O'Neill, C. & Lenardic, A. 2007, GRL 34,
  L19204: plate tectonics on super-Earths *(contested)*.
- Olson, P. & Christensen, U. R. 2006, EPSL 250, 561: dynamo scaling.
- Schulze-Makuch, D. et al. 2011, Astrobiology 11, 1041: the Earth Similarity Index.
- Hassler, D. M. et al. 2014, Science 343, 1244797: radiation on the surface of Mars.
- Sherwood, S. C. & Huber, M. 2010, PNAS 107, 9552; Vecellio, D. J. et al. 2022, J. Appl. Physiol. 132, 340:
  wet-bulb limits.
- Meadows, V. S. & Barnes, R. K. 2018, in *Handbook of Exoplanets*: "Factors affecting exoplanet habitability", a
  good overview to start from.
- Shields, A. L., Ballard, S. & Johnson, J. A. 2016, Physics Reports 663, 1: habitability of planets around
  M dwarfs.
- Human limits: NASA-STD-3001 (spaceflight human-system standard) for pressures and gas partial pressures;
  ICRP Publication 103 for dose limits; occupational CO₂ limits (OSHA/NIOSH); the Armstrong limit.

---

## 17. Peter's brief, in substance (23 November 2026)

Design planets for a solar system, with one or two in the habitable zone, which depends on the sun, so the sun
needs design tools too. Useful on a PC and in VR. Interesting, and in the longer term pedagogical: designing a
planet as good as Earth is no small task. Set in a vast planetary workshop, with the possibility of engineering
both the core and the surface. Everything depends on gravity, the position in the habitable zone, the ability to
hold an atmosphere and many other things. Size and type of sun; size and type of planet, including the orbit,
which must not be excessive. Include every design parameter that in one way or another influences the
possibility of hosting life, and in particular humans. Written as a specification, saved in the Brokild VR
GitHub so the next Claude can find it, with inspiration from the Illusion app.
