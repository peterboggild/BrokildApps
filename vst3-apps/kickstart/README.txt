KICKSTART - the kick drum synthesiser (VST3 + standalone)
==========================================================

Build 260926.4. Free. Every kick is synthesised; there are no samples in it.


INSTALL
-------

Copy the folder

    Kickstart.vst3

into

    C:\Program Files\Common Files\VST3\

and rescan plug-ins in your DAW. It shows up as an instrument (Brokild,
Kickstart). Kickstart.exe is the same instrument as a standalone application:
click the HIT pad, press space, or play it from a MIDI keyboard.


WHAT IS IN HERE
---------------

    Kickstart.vst3          the plug-in
    Kickstart.exe           the standalone application
    Kickstart-Manual.pdf    the manual
    README.txt              this file


THE CONTROLS
------------

    BODY       PITCH, SWEEP, BEND, DECAY, CURVE, WAVE, SKIN
    HIT        CLICK, TONE
    ERA        GRIT, ROOM
    TRANSIENT  ATTACK, SUSTAIN
    DRIVE      ENGINE (IDLE BURN, HYPERDRIVE, RAZOR WING, SUPERNOVA),
               DRIVE, COLOUR
    COMP       COMP, SPEED
    OUT        LEVEL, VELO, KEY TRACK

Hover over any knob for a one-line explanation. The manual has the rest.

Every MIDI note plays the kick. With KEY TRACK on, the note sets the pitch
(C1 = PITCH), so the Trap 808 preset plays as a bass.

User patches are saved to  Documents\Brokild patches\Kickstart.


PRESETS
-------

31 factory presets in three banks: ACOUSTIC, CLASSIC (in the spirit of the
808, 909, LinnDrum, DMX, Linn 9000, SP-12, MPC60, KR-55 and friends) and
MODERN (techno rumble, gabber, hardstyle, dubstep, trap 808, psytrance and
more). The machine names belong to their makers; the kicks are synthesised
here from scratch.


LICENCE
-------

AGPLv3. The corresponding source for this binary is at

    https://github.com/peterboggild/BrokildApps/tree/main/vst3-apps/kickstart/plugin

and the full terms are in LICENSE and LICENSING.md at the repository root.
