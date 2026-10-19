# Hairball roadmap

Done (v0.1)
- C++ core with 272 checks; knob changes are audible within 2 ms; worst case about 5 % of one core
- Interactive mockup with the same engine in JavaScript, 11 starting-point presets
- Parameter table as one source for engine, mockup and manual

Next
1. JUCE plugin: parameters from `HairballParams.h`, MIDI in, audio file load and drag and drop, host sync
2. Oversampling on the Colour stage (aliasing not measured yet)
3. Listening tests in Bitwig and Ableton; tune presets by ear (current ones are starting points)
4. PDF manual with the interface tour, installers (Windows, macOS, Flatpak), release workflow
5. Dance stage tuned against real dance-music material (current numbers are my own approximation)
6. Edge handling when a frozen grain crosses the buffer seam

Known limits
- No real-DAW or ear test yet; riser tone checked with synthetic sources only
- Latency reported as 0 samples (no lookahead anywhere in the chain)
