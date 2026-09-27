GRAVITY WELL  -  build 260927.8
Brokild  -  a bassline synthesiser for Windows (VST3 + standalone)


WHAT IT IS

One control called MASS sets a Schwarzschild radius. Everything else follows
from it by the real formula: each modulator sits at a RADIUS and runs at
sqrt(1 - rs/r) of its nominal rate, so moving one closer to the hole slows it
down, and taking it inside the horizon freezes it and holds its last value.
Because that factor is non-linear in r, the RATIOS between modulators change -
the movement re-orders itself rather than simply slowing.

The redshift sinks a formant bank and never touches the pitch. That one
separation is what makes the idea usable on a bass instrument: the weight
arrives without the tuning going with it.

At MASS 0 the well is flat and this is an ordinary, very good bass synth.


INSTALLING

Copy the "Gravity Well.vst3" folder into your system VST3 folder - the usual
place is Common Files\VST3 under Program Files - and rescan your plug-ins.
"Gravity Well.exe" is the standalone and needs no installation.

The panel needs the Microsoft WebView2 runtime, which is present on almost
every up-to-date Windows. If it is missing, the plug-in says so and gives you
the download link rather than showing a broken page.


WHAT IS IN IT

  143 host parameters, plus five BWFX macros
  32 factory presets, ordered from the ordinary to the strange, and 168
    numbered slots (32-199) for your own - one file each in
    Documents/Brokild patches/Gravity Well, shared by every project;
    the factory bank cannot be overwritten. The PATCH CHART loads, saves
    and clears: click a slot to aim at it, arrows slide the columns, and a
    cleared patch is kept in a Deleted folder
  7 filter circuits in two banks: series, parallel or split
    - the DIODE circuit is a 303 ladder: 18 dB/oct, and its resonance KEEPS
      the bass (+2.33 dB at 40 Hz, where a Moog ladder loses 16.94)
    - and it clips asymmetrically, so it makes even harmonics: the 2nd from
      a pure sine is -33.7 dB against -63.5 for a symmetric circuit
  a cockpit panel: the well on the viewscreen, cutoff, resonance and
    circuit always on the left pillar, the six macros on the right, and
    seven consoles on the desk below (HELM, REACTOR, HORIZONS, CHRONO,
    ORBITS, MATRIX, OUTPUT), each flanked by line displays drawn from
    the engine: waveforms, filter response and signal path, envelopes,
    LFOs, a live modulation network and output meters (amber when the
    ceiling is shaping the peaks); WORLD FX and SETTINGS (panel
    brightness) sit beside them
  LFOs sync to the host tempo in straight, dotted and triplet divisions,
    locked to the bar while the transport runs
  acid lines are played from your piano roll: overlapping notes slide at
    the GLIDE time and velocity sets how hard each note hits
  two oscillators, sub, noise, ring mod, sync and phase modulation
    - the sub sits one or two octaves below oscillator A
    - the wavetable engine reads precomputed band-limited tables, so even
      an eight-voice wavetable swarm costs a few percent of a core
  MASS past noon is heard directly: the top sinks and the sub gathers weight
  REDSHIFT sinks every filter circuit, not only the formant bank
  the formant bank morphs u-o-a-e-i, a path with no jumps
  a unison "accretion disk" on Keplerian radii rather than a flat detune
  a ringdown built from Schwarzschild quasi-normal modes: more mass rings
    lower and longer
  four drives, every one power-neutral - the make-up is measured at prepare,
    not typed, so turning DRIVE up does not turn the instrument up
  the Brokild World FX rack
  DRIVE sits before the filters by default, or after them (POSITION)
  the amp envelope sits after the filters and the drive: level only
  every voice has its own filter envelope, and every factory patch wires it
  each LFO can drive a target of its own from ORBITS (TARGET + AMT)
  oscillator OCTAVE spans +-3; TRANSPOSE moves everything an octave
    either way, and the pitch wheel moves exactly its BEND RANGE
  TIME is neutral at noon
  RINGDOWN rings out after the note instead of being cut off
  the flight manual (PDF) is in this download


A NOTE ON THE WELL

The picture in the middle of the panel is Flamm's paraboloid, the real
embedding diagram of the Schwarzschild metric, and the lights orbiting it are
the modulators at their own radii, each turning at a Keplerian rate slowed by
the very dilation the engine is applying to it.

It is a readout, not decoration: when a light stops on the screen, the
modulator has stopped in the sound. Drag to orbit, wheel for elevation.


Free. Source: https://github.com/peterboggild/BrokildApps
Peter Boggild, September 2026.
