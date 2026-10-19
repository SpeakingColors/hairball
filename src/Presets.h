// Factory presets. Values use the parameter ids in HairballParams.h; anything not listed keeps its default.
// Starting points made from the mockup, not tuned by ear yet. Original sounds.
#pragma once
#include <vector>
#include <utility>

namespace hb
{
struct FactoryPreset { const char* name; const char* category; std::vector<std::pair<const char*, float>> values; };

inline const std::vector<FactoryPreset>& factoryPresets()
{
    static const std::vector<FactoryPreset> list = {
        { "Init", "Init", {  } },
        { "Freeze Pad", "FX", { {"mode", 1.0f}, {"freeze", 1.0f}, {"grainLen", 220.0f}, {"attack", 90.0f}, {"spacing", 60.0f}, {"pan", 70.0f}, {"width", 160.0f}, {"highG", -3.0f}, {"mix", 100.0f} } },
        { "Cloud Wash", "Grains", { {"grainLen", 40.0f}, {"attack", 25.0f}, {"spacing", 45.0f}, {"rand", 55.0f}, {"pan", 80.0f}, {"waveSpacing", 30.0f}, {"width", 170.0f}, {"lowG", 2.0f} } },
        { "Reverse Wash", "Grains", { {"waveSpacing", -100.0f}, {"grainLen", 120.0f}, {"attack", 60.0f}, {"spacing", 70.0f}, {"pan", 40.0f}, {"midG", 3.0f}, {"midF", 2200.0f} } },
        { "Riser Tone Sweep", "Riser", { {"riseOn", 1.0f}, {"riseTime", 6.0f}, {"riseCurve", 2.2f}, {"risePitch", 24.0f}, {"riseTone", 80.0f}, {"riseGrain", 85.0f}, {"riseFilter", 3.0f}, {"filtType", 1.0f}, {"cutoff", 900.0f}, {"dance", 25.0f} } },
        { "Riser Up On The Key", "Riser", { {"riseOn", 1.0f}, {"riseTime", 3.0f}, {"riseCurve", 1.6f}, {"risePitch", 12.0f}, {"riseTone", 90.0f}, {"riseKey", 1.0f}, {"grainLen", 60.0f}, {"attack", 12.0f}, {"drive", 25.0f} } },
        { "Hold Stutter", "FX", { {"mode", 1.0f}, {"hold", 1.0f}, {"grainLen", 70.0f}, {"attack", 5.0f}, {"spacing", 100.0f}, {"lfoDepth", 80.0f}, {"lfoSpeed", 8.0f}, {"dance", 35.0f} } },
        { "Pitch Drop", "Pitch", { {"pitchEnv", -24.0f}, {"pitchEnvT", 600.0f}, {"pitchEnvC", 0.5f}, {"grainLen", 150.0f}, {"attack", 40.0f}, {"transistor", 30.0f} } },
        { "Thick Low Grains", "Thicken", { {"pitch", -12.0f}, {"grainLen", 180.0f}, {"attack", 60.0f}, {"spacing", 55.0f}, {"bassMono", 160.0f}, {"lowG", 5.0f}, {"transistor", 45.0f}, {"drive", 20.0f}, {"dance", 30.0f}, {"filtType", 1.0f}, {"cutoff", 5000.0f} } },
        { "Telephone Wobble", "FX", { {"mode", 1.0f}, {"lfoDepth", 60.0f}, {"lfoSpeed", 5.0f}, {"filtType", 3.0f}, {"cutoff", 1400.0f}, {"reso", 2.5f}, {"fuzz", 25.0f}, {"grainLen", 50.0f}, {"attack", 10.0f} } },
        { "Hit Slicer", "Rhythm", { {"transMode", 2.0f}, {"transHold", 90.0f}, {"grainLen", 90.0f}, {"attack", 6.0f}, {"rand", 25.0f}, {"waveSpacing", 60.0f}, {"dance", 40.0f} } },
    };
    return list;
}
} // namespace hb
