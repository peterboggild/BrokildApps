# Thirty Thousand Years — requirements ledger

Every line of the brief (2026-09-23), with its status. DONE means built and
measured (bench check number where there is one); DEFERRED means designed,
not built, with the reason; BLOCKED names the dependency. Updated with every
build. Nothing disappears from this list.

Legend: **DONE** · **PART** (partly, what is missing stated) · **DEFERRED** · **BLOCKED**

## 2. Strata and the shared environment
| item | status | note |
|---|---|---|
| Four strata coexist, excite each other, respond to measured aspects of each other | DONE | STRUCTURE exciter = MASS/SIGNAL/MEMORY/LOOP/AUX; the LIFE NETWORK detectors (energy/brightness/transient per stratum) drive named couplings; bench [8] |
| Environment participates in generation (feedback, distortion, propagation, reverb) | DONE | the FEEDBACK LOOP returns into MIX / STRUCTURE exciter / MEMORY capture / SIGNAL ring; bench [7] |
| Example interaction (MASS→STRUCTURE→MEMORY density→SIGNAL) understandable, attenuable, freezable, undoable | DONE | COUPLING / AUTONOMY scale it, HOLD LIFE freezes it, UNDO restores the patch |
| A basic patch works without imported audio; procedural factory sources | DONE | VOICE, BROADCAST, CHOIR, MACHINE built at prepare |

## 3. Performance modes and musical foundation
| item | status | note |
|---|---|---|
| DRONE: explicit switch, root, chord, layer participation, latch; no autonomous loud drone on a new instance | DONE | bench [2]: a fresh instance is EXACTLY silent |
| INSTRUMENT: mono, legato, poly; velocity, bend, sustain, aftertouch, MPE; eight voices with explicit limits | DONE | `vmode`, `voices` 1..8; MPE bench [9] |
| EXCITATION: external audio excites the engine; companion VST3 effect target | PART | the AUX input bus excites STRUCTURE and feeds the capture; the separate EFFECT target is DEFERRED (the instrument's AUX bus covers hosts that route audio to instruments; an FX-format twin sharing the DSP is a second target in CMake, not done in this build) |
| Tuning: semitones, cents, frequency; scale constraint; free intervals; Scala import | DONE | `tune`, `fine`, `scale` (9 scales), `.scl` import; bench [9] 5-TET |
| Root lock | DONE | the drone root voice and the lowest voice ignore world-mod detune |
| Persistent drone plus playable overlay with a voice-budget policy | DONE | the chord takes its size, keys the rest |
| Central mono bass anchor, low-frequency stereo narrowing, per-layer cuts, mixer (mute/solo/gain/pan/width/sends) | DONE | the sub stays centred; `bassmono`; bench [7] |
| Spectral room by user filtering and sidechain ducking, not automatic mastering | DONE | per-stratum hp/lp; AUX DUCK |

## 4. Sound engines
| item | status | note |
|---|---|---|
| MASS: two VA oscillators + sub; sine/tri/saw/pulse; sync; detune; restrained unison | DONE | polyBLEP; unison 1..3 |
| MASS: pitch drift, phase, pulse-width drift, amplitude variation, temporal correlation, defined range | DONE | Ornstein–Uhlenbeck walks with DRIFT TIME and DRIFT (cents) |
| MASS: slow beating in Hz separate from cents | DONE | bench [3]: 0.5 Hz beats every 2.0 s at two notes |
| MASS: harmonic reinforcement | DONE | REINFORCE |
| MASS: characterful LP + multimode, saturation, stable resonance | DONE | ZDF ladder / TPT SVF with tanh in the state |
| SIGNAL: two morphable wavetables, band-limited, interpolated; original tables | DONE | 8 tables × 8 frames × 9 mips |
| SIGNAL: two-operator PM, ring, 64-partial additive; spread/odd-even/tilt/cluster/gaps/motion | DONE | bench [4] |
| SIGNAL: stable fundamental while partials misalign | DONE | FUNDAMENTAL; bench [4] |
| SIGNAL: frequency shift (Hz) vs pitch shift (ratio) as different controls | DONE | bench [3] |
| SIGNAL: INTERFERENCE with level compensation | DONE | measured RMS tracker |
| MEMORY: stereo granular with the listed controls; clocked/irregular/clustered | DONE | pool of 96, overload steals the quietest |
| MEMORY: 5 ms–2 s, cost validated | DONE | cost table |
| MEMORY: rolling capture (30 s), selectable source, REMEMBER makes it persistent, never serialised from the audio thread | DONE | snapshot on the message thread; base64 in state only after REMEMBER |
| MEMORY: STFT mode with freeze, smear, tilt, thinning, displacement; WOLA normalisation; frozen vs evolving; latency shown | DONE | 2048/512 Hann; EVOLVE; `memLatency` in initialState; bench [6] |
| MEMORY: Erosion with separately accessible components | DONE | bench [6] |
| MEMORY: original/procedural factory material | DONE | |
| STRUCTURE: modal banks and comb/waveguide; templates acknowledged as designed resonances | DONE | 7 bodies; bench [5] |
| STRUCTURE: the listed controls; exciters incl. friction; friction documented, not oversold | DONE | a simplified stick–slip curve, documented; bench [5] sustains |
| STRUCTURE: stress → fracture; STRIKE; Sustain Energy | DONE | bench [5] |

## 5. Environment
| item | status | note |
|---|---|---|
| Four source channels, two reorderable insert lanes, shared spatial send, dedicated feedback loop; acyclic ordinary routing | DONE | |
| Saturation/asym clipping level-matched; wavefolding; three-band with protected low; frequency shifter fine near zero; modulated delay with tape and pitch-preserving modes; comb/diffusion; bit/rate degradation with defined anti-alias | DONE | bench [7] level match |
| Algorithmic reverb: early reflections, diffusion, frequency-dependent decay, modulation, freeze, dry/wet | DONE | bench [7] RT60 |
| Feedback network as synthesis engine with send/return/delay/filter/shift/saturation/damping/energy | DONE | bench [7] 30 s at full |
| Lossless/contractive construction first; bounded losses; time-varying elements validated | DONE | Householder FDN; the loop's tanh + DC + guards inside |
| Apparent Scale separate from amount; Distance separate from width | DONE | bench [7] early spacing |
| Protection inside the network; metered protective limiter with latency reported | DONE | `setLatencySamples`; bench [7] |
| Panic fades, clears, stays stopped | DONE | bench [9] |

## 6. LIFE
| item | status | note |
|---|---|---|
| 8 LFOs, 4 envelopes, 4 drawable multisegment envelopes, 4 stochastic, 2 followers, 4 event lanes; global vs per-voice explicit | DONE | LFO SCOPE, ENV TRIGGER, SHAPE trig |
| Time ranges to 20-minute cycles; audio-rate modulation restricted | DONE | 0.0008 Hz; control rate = 32 samples, stated |
| Stochastic options incl. bounded chaos; irregularity vs chaos distinct | DONE | CHAOS is a guarded Lorenz trajectory |
| Signed depth, offset, curve, slew, range clamp, source, via; base + effective range shown; drag-to-assign; searchable matrix | DONE (engine) / PART (panel: see the panel probe) | bench [8] |
| Utility modules: add, multiply, threshold, S/H, lag | PART | VIA (multiply), OFFSET (add), SLEW (lag), range clamp (threshold-ish) are in every slot; a dedicated S/H utility is the RANDOM S&H source |
| Life Network from the engine's own audio with smoothing, hysteresis, refractory, depth limits | DONE | bench [8] |
| Autonomy, Coupling, Recovery; freeze evolution while sound continues | DONE | HOLD LIFE |

## 7. HISTORY
| item | status | note |
|---|---|---|
| Four snapshots, renameable; big control; manual, host automation, synced and free durations 1 s–30 min | DONE | bench [8] |
| Musical-domain interpolation; exclusions | DONE | normalised space is log for Hz/ms, linear for semitones; F_NS excluded |
| Discrete changes step, not interpolate | DONE | LIST/SW/INT step at the midpoint |
| Hold History / Hold Life / Freeze Audio separated | DONE | |
| Deterministic and free-running; seeds per subsystem; defined transport reset | DONE | bench [9] bit-identical |
| Reproducible offline renders within tolerance | DONE | tolerance: exact (same start, rate, quality, block layout); seeking is a reset, documented |

## 8. Macros
| item | status | note |
|---|---|---|
| Eight macros with visible, remappable assignments; consistent meanings | DONE | default maps + per-preset overrides |
| HUMANITY patch-specific | DONE | 9 presets override it |
| History, Scale, root, latch, output, Freeze, Panic kept separate | DONE | |
| VIOLENCE not just louder | DONE | level-matched stages |
| Mutate, Undo/Redo, A/B with locks | PART | Mutate with group locks, one-step UNDO, A/B done; a REDO stack is DEFERRED (undo swaps with the current state, so one undo of an undo is a redo) |

## 9. Visual and interaction design
| item | status | note |
|---|---|---|
| Console look; palette; typography; 1440×900 scalable; layout; views; spectral record; reduced motion; identical with editor closed | see the panel probe (`test/uiprobe.js`) | the engine never depends on the editor |
| Plain subtitles, numeric entry, fine drag, reset, units, keyboard nav, contrast, automation ids | PART | automation ids DONE (stable ids, names); the rest in the panel probe |

## 10. Engineering
| item | status | note |
|---|---|---|
| Pinned JUCE and CMake; Windows x64 VST3 + standalone first; portability preserved | DONE | JUCE 8.0.13 (the fleet's), CMake 4.4; no Windows-only code outside the WebView2 guard |
| Engine / parameters / state / assets / GUI separated; headless render/test executable | DONE | `ttytest`, `ttyrender`, `ttyprobe` |
| No allocation, locks, I/O, logging in real time; background preparation; bounded handoff | DONE | atomic clip pointers; the capture snapshot on the message thread |
| Stable ids; versioned state; APVTS outside real-time; modulation separate from host values | DONE | `eff` vs base |
| Sample-offset MIDI | DONE | |
| Automation timing audited vs the wrapper | PART | JUCE's VST3 wrapper applies parameter changes per block; the engine smooths at control rate (32 samples). NOT sample-accurate automation; stated. |
| Variable/zero blocks, rate changes, bypass, transport jumps, offline, suspension | DONE | bench [9] blocks; jumps reseed in deterministic mode |
| Antialiased oscillators and tables; selective oversampling; ADAA considered | DONE / DEFERRED | polyBLEP + mip tables; 2×/4× half-band around the lanes' shapers (bench [7]: folded 3rd −75 dB at 4×); ADAA not used (the shapers are oversampled instead) |
| Live/Studio/Render profiles; no silent quality change during export; stable latency | DONE | quality is a parameter the user sets; latency = the limiter's lookahead, constant |
| 44.1/48/88.2/96 kHz | DONE | bench [9] |
| CPU stated against hardware, patch, voices, rate, buffer, mode | DONE | bench [11] cost table, this machine |
| Presets include tuning, routing, macro maps, scenes, seeds, source references; portable bundle; missing-file recovery | PART | JSON patches carry all of it and the REMEMBERED capture; an import is referenced by path with a notice when missing (a bundle that embeds the import file is DEFERRED) |
| Dependency licences | DONE | JUCE (the fleet's licence position), BWFX (Brokild's own); no other dependencies |
| Neural audio | DEFERRED | not required; not built |

## 11. Factory sounds
| item | status | note |
|---|---|---|
| 16 hero presets with macros, tonal centre, four-scene journey, sensible level, note | PART | 16 heroes exist with notes and macro maps; 8 of 16 carry explicit four-scene journeys (the others' scenes = the base until stored) |
| At least 48 useful presets across the listed categories; a quarter usable in an arrangement; dry and understated ones | PART | 36 in this build (see Presets.cpp); growing toward 48 |
| Rendered demonstrations: 90 s flagship, quiet 60 s, playable phrase, dense example; WAVs + event data | DONE | `ttyrender` writes the four with `.txt` timelines |
| Listening review | BLOCKED | no listening tool in this environment; the renders exist and the author has not heard them |

## 12. Acceptance
| item | status | note |
|---|---|---|
| No NaN/inf/stuck voices/uncontrolled growth under random sweeps and soaks | DONE | bench [10]: 120 random machines, 90 s soak |
| Click-free ordinary edits; documented discontinuous gestures | PART | parameters are read at control rate (32 samples) with the strata's own smoothing; a click bench (max sample step across a parameter jump) is DEFERRED |
| Measured aliasing | DONE | bench [7] |
| Bass stability and mono compatibility | DONE | bench [7] bass mono |
| State/preset round trip, missing assets, deterministic render comparison | DONE / PART | deterministic bench [9]; state round trip via the console host (test/host) |
| Worst-case processing times at 64/128/512/irregular; caps under overload | DONE | bench [9] and [11] |
| pluginval strictness 10; real host testing | BLOCKED / PART | pluginval is not installed here; the console host (`test/host`) exercises the installed bundle; DAW: Ableton on this machine, Peter's |
| Every control implemented and audible | DONE | the panel builds from the same table; no mock controls |
| Ledger, architecture guide, build instructions, licence inventory, musician manual | this file, `BrokildApps/THIRTY-THOUSAND-YEARS-DESIGN.md`, `README.md`, the manual | |
