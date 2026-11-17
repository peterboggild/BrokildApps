# Snip — a small, fast audio editor

Double-click **Snip.cmd** to open it in its own window (Edge app mode; works offline). You can also open `index.html` in Edge or Chrome. Drop a file on the window, or press Ctrl+O.

- **Open:** WAV (8/16/24/32-bit int, 32/64-bit float, RF64). MP3, FLAC, OGG and M4A are decoded at their native rate.
- **Edit:** set the cursor, select, cut/copy/paste (works across files), delete, trim, normalise to −X dB, gain, fade in/out, silence, undo/redo.
- **Convert:** resample with a Kaiser windowed-sinc filter (about 90 dB stopband). Save as WAV 16-bit (TPDF dither), 24-bit, 32-bit int or 32-bit float, or as MP3 at 128–320 kbps.
- **Channels:** split stereo into separate mono files, keep left or right, mix to mono, swap channels, merge with a second mono file into stereo, or duplicate mono to stereo.

Press `?` in the app to see all shortcuts.

The MP3 encoder is `vendor/lame.min.js` (lamejs 1.2.1, LGPL). Run `node test/core.test.js` to test the WAV I/O and the resampler.

## Open WAV and MP3 files by double-clicking

Open the site version in Edge or Chrome (https://peterboggild.github.io/BrokildApps/music-apps/snip/) and choose **Install** (the app icon at the right of the address bar, or ⋯ → Apps → Install Snip). The installed app works offline and registers with Windows for .wav, .mp3, .flac and .ogg. Right-click an audio file → **Open with** → **Choose another app** → **Snip**, and tick **Always** to make it the default. The first time, the browser asks whether Snip may open that file type. Ctrl+S then saves straight back to the file.
