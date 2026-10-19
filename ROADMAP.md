# Hairball roadmap

Done (v0.1)
- C++ core with 280 checks; knob changes are audible within 2 ms; worst case about 5 % of one core
- Interactive mockup with the same engine in JavaScript, 11 starting-point presets
- Parameter table as one source for engine, mockup and manual

Done (v0.1.0)
- JUCE plugins Hairball (instrument) and Hairball FX (effect) with the window from the mockup, factory and user presets, drag and drop, MIDI in
- Windows, macOS and Flatpak installers with uninstallers, PDF manual with interface tour, release workflow (built by CI, not yet run on real machines)

Next
1. Test in real hosts (Bitwig, Ableton, Reaper): instrument versus FX behaviour, state recall, file samples
2. Oversampling on the Colour stage (aliasing not measured yet)
3. Listening tests in Bitwig and Ableton; tune presets by ear (current ones are starting points)
4. Check the installers and uninstallers on real machines
5. Dance stage tuned against real dance-music material (current numbers are my own approximation)
6. Edge handling when a frozen grain crosses the buffer seam

Known limits
- No real-DAW or ear test yet; riser tone checked with synthetic sources only
- Latency reported as 0 samples (no lookahead anywhere in the chain)
