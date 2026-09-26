HATS OFF - the hi-hat and cymbal synthesiser (VST3 + standalone)
================================================================

Build 260926.4. Free. Every hat and cymbal is synthesised; there are no
samples in it.


INSTALL
-------

Copy the folder

    Hats Off.vst3

into

    C:\Program Files\Common Files\VST3\

and rescan plug-ins in your DAW. It shows up as an instrument (Brokild,
Hats Off). Hats Off.exe is the same instrument as a standalone application:
click the cymbal (the middle is the bell, the rim is the edge), press space,
C, P, O, B or E, or play it from a MIDI keyboard.


WHAT IS IN HERE
---------------

    Hats Off.vst3           the plug-in
    Hats Off.exe            the standalone application
    Hats-Off-Manual.pdf     the manual, 16 pages
    README.txt              this file


THE CONTROLS
------------

    METAL      PITCH, SIZE, BRONZE, DENSITY, BLOOM, TRASH, DECAY, NOISE
    HAT        OPEN, SIZZLE, CHICK, CHOKE
    HIT        STRIKE, STICK
    EQ         CUT, AIR
    ERA        GRIT, ROOM, WIDTH
    ECHO       ECHO, TIME (synced to the host tempo)
    TRANSIENT  ATTACK, SUSTAIN
    DRIVE      ENGINE (IDLE BURN, HYPERDRIVE, RAZOR WING, SUPERNOVA),
               DRIVE, COLOUR
    COMP       COMP, SPEED
    OUT        LEVEL, VELO, KEYS

Hover over any control for a one-line explanation. The manual has the rest.

KEYS decides what a MIDI note means:
    FIXED      every note plays the cymbal as dialled
    GM KIT     42 closed, 44 pedal, 46 open, 53 bell, 49/52/55/57 edge
               (the default)
    CHROMATIC  the note tunes the cymbal, F#1 (42) = PITCH

User patches are saved to  Documents\Brokild patches\Hats Off.


PRESETS
-------

44 factory presets in six banks, plus Init: ACOUSTIC (hats, rides, crashes,
splash, china), VINTAGE (in the spirit of the 808, 909, 606, CR-78,
LinnDrum, 707/727 and SP-1200), TECHNO, DUB, DUBSTEP and MODERN. The machine
names belong to their makers; the cymbals are synthesised here from scratch.


LICENCE
-------

AGPLv3. The corresponding source for this binary is at

    https://github.com/peterboggild/BrokildApps/tree/main/vst3-apps/hats-off/plugin

and the full terms are in LICENSE and LICENSING.md at the repository root.
