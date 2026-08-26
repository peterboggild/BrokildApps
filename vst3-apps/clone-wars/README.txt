CLONE WARS
Sixteen analogue clones of almost, but not quite, the same oscillator -
drawn up in two armies of eight, aimed at each other, and slid into
battle with one fader.
Brokild Heavy Industries · Drone Division  -  build 260826.6  -
August 2026  -  Windows 64-bit


WHAT IS IN HERE
---------------
  Clone Wars.vst3             the plugin (a FOLDER - copy the whole thing)
  Clone Wars.exe              the same instrument, standalone
  Clone-Wars-Manual.pdf       the idea, every panel, the service bay


INSTALL
-------
  1. Copy the whole "Clone Wars.vst3" FOLDER into your system VST3
     directory:  C:\Program Files\Common Files\VST3\
     A sub-folder such as ...\VST3\Brokild\ is fine; hosts look inside.
  2. Rescan plugins in your DAW. It appears as an INSTRUMENT called
     Clone Wars, by Brokild.
  3. Or just run Clone Wars.exe, which picks its own audio and MIDI
     devices from its options menu, and has the field keyboard built in.


THIRTY SECONDS OF INSTRUCTIONS
------------------------------
  - A fresh instance is SILENT - a VST3 has manners. Hold a note (or
    press KEYS for the field keyboard) and sixteen clones wake on it.
  - Hold up to THREE notes: each strip's NOTE 1-2-3 buttons pick which
    held note that clone follows. Voice a chord across the armies.
  - HOLD (on the keyboard plate) or LATCH A/B keep notes standing after
    release. That is the drone way.
  - PATCHES opens five seed families - abyss, swarm, engines, cathedral,
    rust. Every seed number is the same sound on every machine, forever.
  - The MORPH knob glides the whole console between seed A and seed B.
  - THE WAR slides the mix from Army A to Army B; SLEW stretches the
    journey up to five minutes. Set it moving and go make coffee.
  - THE RANKS (below THE WAR) is a sync/desync slider: left pulls every
    continuous control toward its army's average - hard left is sixteen
    perfect copies - and right exaggerates the differences, up to full
    mutiny. Non-destructive: centre always returns your exact patch.
    Discrete rows (wave, footage, notes) and the faders are untouched.
  - FOOTAGE runs 64' to 2' - from three octaves below 8' to two above,
    for extreme registrations.
  - GROWL / SCREAM / LADDER pick each army's filter circuit. Same patch,
    three different animals.
  - Shift-click controls in one row to LOCK them into a group. Ctrl-click
    locks a whole half-row (an army's eight) in UNISON at the clicked
    value; shift-ctrl-click locks it keeping its intervals; the lock
    tools on the side rails do the same per army. The dice reroll a row;
    the fan draws a ramp across clones 1-16.
  - ENGINE cycles LOW / HQ / XHQ render quality. Offline bounces are
    ALWAYS rendered XHQ, whatever the panel says.


THE HULL REMEMBERS
------------------
  Every instance keeps an odometer - UNIT AGE, top right. While the unit
  plays, it ages, and as it ages the panel scars: dings, chips, paint
  peel, rust, fingerprints - placed deterministically from the unit's
  own serial, saved per instance inside your DAW project. There is no
  off switch. Shift-click the odometer to open the SERVICE BAY: REPAIR
  restores factory condition and restarts the clock; TOUR simulates a
  year of hard touring right now.


NOTES
-----
  The interface is drawn in a WebView2 surface, which every current
  Windows 10 and 11 already has. If the window comes up blank or shows
  an error page, install the free Microsoft Edge WebView2 Runtime and
  reopen the plugin. The audio keeps working either way.

  If Windows refuses to load the plugin and mentions an "Application
  Control policy", that is Smart App Control. It decides per FILE from a
  cloud reputation lookup; most machines do not have it enabled at all.

  The output has a transparent soft ceiling that is always in place.
  Nothing here - not sixteen screaming filters, not a frozen spring into
  full drive - can put a spike above full scale into your monitors.

  Free. No installer, no account, no telemetry, no network access at
  all. Play unsafely, unwisely, and unboringly.

  Peter Boggild / Brokild, August 2026.
