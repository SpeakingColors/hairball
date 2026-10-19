<p align="center"><img src="assets/icon.svg" width="120" alt="Hairball icon"></p>

# Hairball

Freeze it, shred it, turn it into something else. A free grain-based audio transformer for dance, house, UK hardcore and hardstyle, with Linux first.

Works two ways:
- **Instrument**: drop an audio file, play it with MIDI, automate everything.
- **FX**: sits on a track, listens to what plays, and freezes, holds or rebuilds it live. Capture the frozen buffer into the plugin as a small sample.

## Status
Version 0.1.0 (test build): two VST3 plugins (Hairball, instrument; Hairball FX, effect) and a standalone app, from the same pure C++ engine (280 passing checks), with the real window built from the mockup, 11 factory presets, installers, and a PDF manual with an interface tour. Not tuned by ear and not tested in a real DAW yet. See [ROADMAP](ROADMAP.md).

## Download
Get the zip from the [Releases](../../releases) page: installers for Windows (.exe), macOS (.pkg), Linux (Flatpak extension and a plain .vst3 tarball), the manual and INSTALL.txt. Everything works offline. Free and open source; if you like it, you can support it on [Ko-fi](https://ko-fi.com/studio87).

## What it does
- Grain controls: Attack, Hold, Grain spacing, Wave spacing, Pan; LFO on wave spacing; Rand
- Transients: grains snap to hits (Off, Regions, Detect)
- Time: Loop, Key to (Pitch, Percent, Step, Transient), Start, Root key
- Pitch, fine tune, pitch envelope, glide
- **Riser**: grains shrink and get closer until they fuse into a pitched tone; pitch and filter climb with it; follows the key you play
- Thicken: mid/side width, bass mono, 3 band EQ, filter, Drive, Transistor and Fuzz (same stages as PurrOne), Dance compressor
- FX mode: up to 8 s buffer, Freeze, Hold, Lag, Capture to sample
- Dry/wet (bit exact at 0 %), safety limiter, zero reported latency, 57 automatable parameters

## Try the mockup
Open `docs/mockups/Hairball.html` in a browser. It runs the same engine (JavaScript port, checked against the C++ core).

## Build and test the core
```
sh tests/run.sh
```
Needs only a C++17 compiler. To compare the JavaScript copy with the C++ core:
```
g++ -std=c++17 -O2 -o /tmp/hb_parity tests/parity_dump.cpp && /tmp/hb_parity > /tmp/parity_cpp.txt && node tools/parity.js /tmp/parity_cpp.txt
```
After changing `src/core/HairballParams.h`, run `python3 tools/gen_params_json.py` then `python3 tools/build_mockup.py`.

## License
AGPL-3.0-or-later, see [LICENSE](LICENSE). Not affiliated with any other product named in documentation. All trademarks belong to their owners.

## Support

<p align="center">
  <a href="https://ko-fi.com/studio87"><img src="https://img.shields.io/badge/Support_Studio_87-on_Ko--fi-FF5E5B?style=for-the-badge&logo=kofi&logoColor=white" alt="Support Studio 87 on Ko-fi"></a>
</p>

**☕ Free and open source, made by one person. If it helps your music, a coffee on [Ko-fi](https://ko-fi.com/studio87) keeps it coming. 💜**
