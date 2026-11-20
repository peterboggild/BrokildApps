DRUMEQ - EQ and compression for an acoustic drum kit (VST3 + standalone)
=========================================================================

Build 261120.2. Free.

One plug-in for every mic of a rock or metal kit: Kick In, Kick Out, Snare
Top, Snare Bottom, Tom 1-3, Hi-hat, Ride, Room and Overheads. Put one on each
drum track and pick the drum. The instances share a KIT, so the whole kit can
be saved, loaded and tuned from one panel.


INSTALL
-------

Copy the folder

    DrumEQ.vst3

into

    C:\Program Files\Common Files\VST3\

and rescan plug-ins in your DAW. It shows up as an effect (Brokild, DrumEQ).
DrumEQ.exe is the same effect as a standalone application.


WHAT IS IN HERE
---------------

    DrumEQ.vst3          the plug-in
    DrumEQ.exe           the standalone application
    DrumEQ-Manual.pdf    the manual
    README.txt           this file


QUICK START
-----------

1. Put DrumEQ on a drum track and click the drum on the top row
   (KICK IN ... OH). The bands, their names and the presets follow the drum.
2. Pick a preset with < and > (five per drum).
3. Play the song and press FIND DRUM (SET LEVEL on hi-hat, ride, room and
   overheads). It listens for six seconds, sets the input so the hits peak at
   -12 dBFS, sets the gate between the spill and the drum, and on kick,
   snare and toms puts the body band on the drum's own note.
   KEEP TRACK LEVEL makes OUT give back what IN took, so faders already
   balanced do not move.
4. LINK joins the kit. SAVE KIT / LOAD KIT store or recall every drum at once.
5. MASTER (with LINK on) turns the panel blue: the drum row then picks which
   drum you edit, and every linked track on that drum follows. HOME marks
   this track's own drum.

DrumEQ never decides which drum a track is: the SOURCE is always your choice.


THE CHAIN (zero latency)
------------------------

    IN (trim, polarity) - GATE - HIGH/LOW-PASS - EQ (4 bands) - COMPRESSOR - OUT

User presets:  Documents\Brokild patches\DrumEQ\<drum>
Kits:          Documents\Brokild patches\DrumEQ\Kits\*.drumkit


LICENCE
-------

AGPLv3. The corresponding source for this binary is at

    https://github.com/peterboggild/BrokildApps/tree/main/vst3-apps/drumeq/plugin

and the full terms are in LICENSE and LICENSING.md at the repository root.
