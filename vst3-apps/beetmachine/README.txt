BEETMACHINE - the kit that plays the three drum synthesisers (VST3 + standalone)
================================================================================

Build 260926.4. Free. Every hit is synthesised; there are no samples in it.


INSTALL
-------

Copy the folder

    Beetmachine.vst3

into

    C:\Program Files\Common Files\VST3\

and rescan plug-ins in your DAW. It shows up as an instrument (Brokild,
Beetmachine). Beetmachine.exe is the same instrument as a standalone
application.

Kickstart, Snare Tactics and Hats Off are built INTO Beetmachine: you do not
need to install them separately to use it.


WHAT IS IN HERE
---------------

    Beetmachine.vst3          the plug-in
    Beetmachine.exe           the standalone application
    Beetmachine-Manual.pdf    the manual
    README.txt                this file


HOW IT WORKS
------------

Eight slots. Each is empty (and costs nothing) or holds a Kickstart, a Snare
Tactics or a Hats Off. Click a slot and that drum's own full panel opens
underneath. By convention the kicks sit in slots 1-2, the snares in 3-4 and the
hats and cymbals in 5-8, but any slot can hold any drum.

NOTES     C3 upwards (Ableton's C3 = MIDI 60): slot 1 on C3 ... slot 8 on G3.
          GM switches to the General MIDI drum notes. LEARN takes a key.
CHOKE     each slot's CHOKED BY row: a hit on a lit slot silences this one.
          In every kit the closed hat (5) chokes the open hat (6).
OUT       MIX (the stereo mix), OWN (the slot's own output pair), BOTH.
          Stereo is the default; enable the extra outputs in your DAW.
KITS      STUDIO, 808, 909, EIGHTIES, BOOM BAP, TECHNO, DUB, INDUSTRIAL,
          TRAP, LO-FI. A kit never changes your notes or routing.
STOP      the red emergency stop: every slot fades out at once.

Hover over any control for a one-line explanation. The manual has the rest.


LICENCE
-------

AGPLv3. The corresponding source for this binary is at

    https://github.com/peterboggild/BrokildApps/tree/main/vst3-apps/beetmachine/plugin

(the three drums' sources are beside it, in vst3-apps/kickstart,
vst3-apps/snare-tactics and vst3-apps/hats-off) and the full terms are in
LICENSE and LICENSING.md at the repository root.
