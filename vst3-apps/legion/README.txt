LEGION — vocal harmoniser (VST3, in development)
================================================

"My name is Legion, for we are many."

One voice in, a choir of it out: up to four copies, each with its own pitch AND
its own body size, because a voice shifted without separating those two is a
chipmunk and not a singer.


INSTALL
-------

Copy the folder

    Legion.vst3

into

    C:\Program Files\Common Files\VST3\Brokild\

and rescan plug-ins in your DAW. There is no installer and nothing is written
outside that folder.


WHAT IS IN HERE
---------------

    Legion.vst3        the plug-in
    README.txt         this file


WHAT IT DOES
------------

    PITCH      +/-24 semitones per voice
    FINE       +/-100 cents per voice
    FORMANT    +/-12 semitones, INDEPENDENT of pitch — move the singer's body
               without moving the note, or the note without the body
    FOLLOW     0-100 %, how much the body rides the pitch. 0 = the same person
               singing higher. 100 = plain resampling, the chipmunk, on purpose
    LEVEL,
    PAN, DELAY per voice
    HUMANISE   uncorrelated slow detune, level shimmer and timing stagger
               across the voices: the difference between four singers and one
               singer that got louder
    MIX        equal-power dry/wet. At MIX 0, or with every voice switched off,
               the output is the input, delayed by the reported latency and
               otherwise untouched, to the bit
    DETAIL     TIGHT / NATURAL / SMOOTH analysis window, 21 / 43 / 85 ms at
               48 kHz. Lower voices want a longer one
    BWFX ON    the Brokild World rack sits on the HARMONY bus by default, so it
               grinds the copies and leaves the lead alone. Switch to MASTER
               for the whole choir


THE LEVELLER
------------

A vocal compressor that works from both ends, off by default:

    TOP        where the downward compression starts (RMS, -40 .. 0 dB)
    RATIO      how hard the loud parts come down above TOP (1:1 .. 10:1)
    LIFT       the most a quiet part is lifted toward TOP (0 .. 18 dB)
    FLOOR      below this nothing is lifted, so breaths and room noise
               between phrases stay where they were (-80 .. -30 dB)
    SPEED      how quickly it lets go (release 800 ms .. 60 ms)

It listens to the input before Legion's delay and sets the gain after it, so
it looks ahead by the latency Legion already has and adds none of its own. It
only changes the volume: no saturation, and no filter on the audio. Both
channels get the same gain. Switch every voice off and it is a leveller on its
own. Switched off, it does not touch the audio at all.


STATUS — PLEASE READ
--------------------

This is IN DEVELOPMENT and it has NOT BEEN HEARD IN A DAW. 128 engine checks
and 27 wrapper checks pass, the panel renders and the rack round-trips, but
nobody has put a real scream through it. The name is provisional.

There is no standalone application and no handbook yet. What there is instead
is the source, which is the point of the Experimental Collection: these exist
to ask a question about how sound can be made, and you are welcome to take one
further.


LICENCE
-------

AGPLv3. You may read, modify and build any of this; if you distribute a
modified build, publish your changes under the same licence.

The corresponding source for this binary is at

    https://github.com/peterboggild/BrokildApps/tree/main/vst3-apps/legion/plugin

and the full terms are in LICENSE and LICENSING.md at the repository root.
The artwork is not licensed for reuse.


                                                          Brokild - 2026
