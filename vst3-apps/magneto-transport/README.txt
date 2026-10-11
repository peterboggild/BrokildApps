MAGNETO TRANSPORT  -  build 261011.11
Brokild  -  a physically modelled tape echo for Windows (VST3 + standalone)


WHAT IT IS

A tape echo in which nothing is faked. There is no wow LFO, no saturation
curve and no noise generator. The machine is simulated, and the character
is what the simulation does.

The tape is a magnetic medium in four layers of half-micron cells. A record
head with a real gap field and AC bias magnetises it through a Preisach model
of the oxide, so saturation is the oxide running out of magnetisation, and
under- and over-bias behave as they do on a real machine. Playback heads read
the flux back through gap, spacing and thickness losses. Hiss is the
statistics of the particles in the coating: ten times as many particles,
10 dB less hiss, and at the top of the PARTICLES control the coating is a
continuum and the tape is silent.

The transport is mechanics. Wow, flutter and scrape flutter come from the
capstan's runout and bearings, the reels' eccentricity and tension, and the
tape sticking and slipping over the heads. A speed change glides with the
motor's inertia, so every echo on the tape bends in pitch. Three playback
heads slide along a rail, with the Doppler shift that moving them causes.

FLUX MEMORY is the feature no tape echo has had. On a closed loop of tape,
DUB lowers the bias so what is already on the loop stays under what you play
now, and the ERASE head takes as much of the old take as you set. An erase
field takes what it can reach and no more, so it acts once: a second pass at
the same setting takes nothing further, turning it up takes more, turning it
down brings nothing back. Tape has no undo.


INSTALLING

Copy the "Magneto Transport.vst3" folder into your system VST3 folder (the
usual place is Common Files\VST3 under Program Files) and rescan your
plug-ins. "Magneto Transport.exe" is the standalone and needs no
installation.

The panel needs the Microsoft WebView2 runtime, which is present on almost
every up-to-date Windows. If it is missing, the plug-in says so and gives
you the download link.


WHAT IS IN IT

  REEL and LOOP transports. REEL plays an open reel whose packs wind across
    as it runs. LOOP runs a closed loop of 0.5 to 6 m, which can be cut to
    a number of bars at the host tempo.
  Tape speed 4.75 to 76 cm/s, continuously variable, or synced: choose a
    playback head as the tempo head and the motor runs at the speed that
    puts it on the note value.
  Three playback heads with their own level and pan, and HEAD PATTERN to
    snap them to rhythmic ratios such as the Space Echo's 1:2:3.
  Feedback from playback back into the record amplifier, with tone controls
    in the loop. It can self-oscillate, and the tape's own saturation holds
    it, not a limiter.
  Four tape stocks: a 1970s studio tape, a 1960s low-output tape, the studio
    tape shedding its oxide, and cheap echo-machine tape. Plus WEAR,
    PARTICLES, and a loop that wears a little on every pass.
  TRANSPORT CONDITION and the three causes behind it (CAPSTAN, REELS,
    SCRAPE), an AC synchronous or DC servo motor, and a weighted wow and
    flutter meter reading what they do. OK / BROKEN moves the whole machine
    towards a wreck: clogged heads, a hunting motor, crinkled tape.
  REPRO, for use as a tape machine on an insert: head 1 replays and its
    delay is reported to the host, so the track stays in time.
  MECH SOUNDS: the machine's own motor, capstan, reels, splice, carriages
    and solenoids, driven by the transport. Off by default; at zero the
    output is the machine bit for bit.
  ECO, NORMAL and HIGH quality (the tape runs at the host rate, 2x or 4x).
  24 factory presets, from clean to strange, each levelled to the loudness
    of what goes in. Your own presets are files in
    Documents\Brokild patches\Magneto Transport.
  A card under every control: hover for what it does, keep hovering for
    the physics underneath, with the equations, live figures and the
    measured numbers.


MEASURED

Every claim on the cards is checked by a bench of 202 tests that runs with
every build. A few of the numbers:

  unity at the operating level            +0.025 dB
  delay against head distance / speed     within 0.72 microseconds
  maximum output level, re 185 nWb/m      +12.3 dB, as the offline
                                            reference model gives
  hiss, studio stock, 19 cm/s             -61.9 dBA
  wow and flutter, freshly serviced       0.036 % (DIN/IEC weighted, 2 sigma)
  aliasing, 9 kHz 12 dB over 0 VU         -63 dBFS (NORMAL)
  cost at 48 kHz, worst case
    (76 cm/s, three heads, feedback)      about 20 % of one core (NORMAL)


WHAT IT IS NOT

Print-through and self-demagnetisation are left out: they are inaudible at
the time scales of an echo loop. The bias is simulated explicitly only in
the offline reference model; the real-time model uses a validated
equivalent, checked against the reference.


Brokild. Free software under the GNU AGPLv3, source at
https://github.com/peterboggild/BrokildApps. Unsigned builds, offered as is.
