# Hairball roadmap

Done (v0.1)
- C++ core with 280 checks; knob changes are audible within 2 ms; worst case about 5 % of one core
- Interactive mockup with the same engine in JavaScript, 11 starting-point presets
- Parameter table as one source for engine, mockup and manual

Done (v0.1.1)
- Colour stages (Drive, Transistor, Fuzz) run at 4x oversampling with zero-latency polyphase allpass half-band filters. Measured against the harmonics at Drive/Transistor 100 percent: aliasing -19 dB before, -36 to -38 dB after; Fuzz 100 percent -23 dB before, -45 dB after. Passband flat within 0.1 dB up to 20 kHz; latency stays 0. Core tests check this (288 checks).

Done (v0.1.0)
- JUCE plugins Hairball (instrument) and Hairball FX (effect) with the window from the mockup, factory and user presets, drag and drop, MIDI in
- Windows, macOS and Flatpak installers with uninstallers, PDF manual with interface tour, release workflow (built by CI, not yet run on real machines)

Next
1. Test in real hosts (Bitwig, Ableton, Reaper): instrument versus FX behaviour, state recall, file samples
2. ~~Oversampling on the Colour stage~~ done: 4x, zero latency, aliasing 17 to 23 dB lower (see Done v0.1.1)
3. Listening tests in Bitwig and Ableton; tune presets by ear (current ones are starting points)
4. Check the installers and uninstallers on real machines
5. Dance stage tuned against real dance-music material (current numbers are my own approximation)
6. Edge handling when a frozen grain crosses the buffer seam

Known limits
- No real-DAW or ear test yet; riser tone checked with synthetic sources only
- Colour oversampling is 4x with a 2x-rate transition band from 20 kHz at 48 kHz; at 44.1 kHz the passband edge is slightly lower in Hz
- Latency reported as 0 samples (no lookahead anywhere in the chain)
