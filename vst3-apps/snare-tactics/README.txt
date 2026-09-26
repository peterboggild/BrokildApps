SNARE TACTICS - the snare drum synthesiser (VST3 + standalone)
================================================================

Build 260926.2. Free. Every snare is synthesised; there are no samples in it.


INSTALL
-------

Copy the folder

    Snare Tactics.vst3

into

    C:\Program Files\Common Files\VST3\

and rescan plug-ins in your DAW. It shows up as an instrument (Brokild,
Snare Tactics). Snare Tactics.exe is the same instrument as a standalone
application: click the HIT pad (the chrome hoop is a rim shot), press space,
R, X or C, or play it from a MIDI keyboard.


WHAT IS IN HERE
---------------

    Snare Tactics.vst3          the plug-in
    Snare Tactics.exe           the standalone application
    Snare-Tactics-Manual.pdf    the manual, 16 pages
    README.txt                  this file


THE CONTROLS
------------

    HEAD       TUNE, DROP, BEND, DECAY, WAVE, SKIN, RING, BODY
    WIRES      WIRES, SIZZLE, TENSION, AIR
    HIT        STRIKE, STICK, CLAP, SPREAD
    ERA        GRIT, ROOM, GATE
    ECHO       ECHO, TIME (synced to the host tempo)
    TRANSIENT  ATTACK, SUSTAIN
    DRIVE      ENGINE (IDLE BURN, HYPERDRIVE, RAZOR WING, SUPERNOVA),
               DRIVE, COLOUR
    COMP       COMP, SPEED
    OUT        LEVEL, VELO, KEYS

Hover over any control for a one-line explanation. The manual has the rest.

KEYS decides what a MIDI note means:
    FIXED      every note is the snare
    GM KIT     38 snare, 40 rim shot, 37 cross-stick, 39 clap (the default)
    CHROMATIC  the note tunes the head, D1 (38) = TUNE

User patches are saved to  Documents\Brokild patches\Snare Tactics.


PRESETS
-------

44 factory presets in seven banks, plus Init: ACOUSTIC, MACHINES (in the
spirit of the 808, 909, 707, LinnDrum, DMX, SP-1200, MPC60, CR-78 and the
Simmons SDS-V), TECHNO, DUB, INDUSTRIAL, HARDCORE and MODERN. The machine
names belong to their makers; the snares are synthesised here from scratch.


LICENCE
-------

AGPLv3. The corresponding source for this binary is at

    https://github.com/peterboggild/BrokildApps/tree/main/vst3-apps/snare-tactics/plugin

and the full terms are in LICENSE and LICENSING.md at the repository root.
