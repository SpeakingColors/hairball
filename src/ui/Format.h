// Text for a parameter value, same wording as the mockup (fmt in hairball.src.html). Used by the host parameter strings,
// the knob value text and the manual.
#pragma once
#include <juce_core/juce_core.h>
#include "../core/HairballParams.h"

namespace hb::fmt
{
inline const char* const kNoteNames[12] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
inline juce::String noteName (int n) { return juce::String (kNoteNames[((n % 12) + 12) % 12]) + juce::String (n / 12 - 1); }

inline juce::String value (int index, float v)
{
    const auto& d = kParams[index];
    const juce::String u (d.unit);
    if (d.choice)
    {
        const auto labs = juce::StringArray::fromTokens (d.choices, "|", "");
        return labs[juce::jlimit (0, labs.size() - 1, juce::roundToInt (v))];
    }
    const float a = std::abs (v);
    if (u == "ms") return a >= 1000.0f ? juce::String (v / 1000.0f, 2) + " s" : juce::String (v, a >= 100.0f ? 0 : 1) + " ms";
    if (u == "s")  return juce::String (v, 2) + " s";
    if (u == "Hz") return a >= 1000.0f ? juce::String (v / 1000.0f, 2) + " kHz" : juce::String (v, a >= 100.0f ? 0 : 1) + " Hz";
    if (u == "st") return (v > 0 ? "+" : "") + juce::String (v, 1) + " st";
    if (u == "ct") return (v > 0 ? "+" : "") + juce::String (v, 0) + " ct";
    if (u == "dB") return (v > 0 ? "+" : "") + juce::String (v, 1) + " dB";
    if (u == "%")  return juce::String (v, 0) + "%";
    if (u == "x")  return juce::String (v, 2) + "x";
    if (u == "oct") return juce::String (v, 1) + " oct";
    if (index == pRoot) return noteName (juce::roundToInt (v));
    if (index == pVoices) return juce::String (juce::roundToInt (v));
    return juce::String (v, 2);
}

// Range text for the info bubble: "20 Hz to 20.00 kHz, default 12.00 kHz" or the list of choices.
inline juce::String rangeText (int index)
{
    const auto& d = kParams[index];
    if (d.choice) return juce::String (d.choices).replace ("|", " / ");
    return value (index, d.lo) + " to " + value (index, d.hi) + ", default " + value (index, d.def);
}

// Parameters whose normalised position is a power curve or logarithmic, exactly as in the mockup.
inline bool isLog (int i) { const auto& d = kParams[i]; return !d.choice && d.lo > 0 && d.hi / d.lo >= 40; }
inline bool isSkew (int i) { const char* s[] = { "attack", "capOff", "ampA", "ampD", "ampR", "bassMono" }; for (auto* x : s) if (juce::String (kParams[i].id) == x) return true; return false; }
inline bool isInt (int i) { return i == pRoot || i == pVoices; }
}
