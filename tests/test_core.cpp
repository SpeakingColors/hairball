// Hairball core tests. No JUCE needed: sh tests/run.sh
#include "../src/core/HairballCore.h"
#include <chrono>
#include <cstdio>
#include <set>
#include <string>

static int fails = 0, checks = 0;
#define CHECK(cond, ...) do { ++checks; if (!(cond)) { ++fails; std::printf("FAIL line %d: %s  ", __LINE__, #cond); std::printf(__VA_ARGS__); std::printf("\n"); } } while (0)

static const double SR = 48000.0;
using Vec = std::vector<float>;

static Vec sine (double hz, double amp, size_t n) { Vec v (n); for (size_t i = 0; i < n; ++i) v[i] = (float) (amp * std::sin (2.0 * 3.14159265358979 * hz * (double) i / SR)); return v; }
static Vec noise (size_t n, float amp, uint32_t seed = 7) { hb::Rng r; r.s = seed; Vec v (n); for (auto& x : v) x = r.bi() * amp; return v; }
static double rms (const Vec& v, size_t a, size_t b) { double s = 0; for (size_t i = a; i < b; ++i) s += (double) v[i] * v[i]; return std::sqrt (s / (double) (b - a)); }
static double dbOf (double x) { return 20.0 * std::log10 (std::max (1e-9, x)); }
static double freqOf (const Vec& v, size_t a, size_t b) { int z = 0; for (size_t i = a + 1; i < b; ++i) if (v[i - 1] <= 0 && v[i] > 0) ++z; return z * SR / (double) (b - a); }
static bool finite (const Vec& v) { for (float x : v) if (!std::isfinite (x)) return false; return true; }
static double autoc (const Vec& v, size_t a, size_t b, size_t lag) { double s = 0, e0 = 0, e1 = 0; for (size_t i = a; i + lag < b; ++i) { s += (double) v[i] * v[i + lag]; e0 += (double) v[i] * v[i]; e1 += (double) v[i + lag] * v[i + lag]; } return s / std::sqrt (std::max (1e-12, e0 * e1)); }

struct Rig
{
    hb::Engine e; hb::Params p;
    Rig() { e.prepare (SR); }
    void apply() { e.setParams (p); }
    // run n samples in blocks of 128, optional input
    void run (Vec& l, Vec& r, size_t n, const Vec* inL = nullptr, size_t offset = 0)
    {
        l.assign (n, 0.f); r.assign (n, 0.f);
        for (size_t pos = 0; pos < n; pos += 128)
        {
            const size_t c = std::min<size_t> (128, n - pos);
            e.process (inL ? inL->data() + offset + pos : nullptr, inL ? inL->data() + offset + pos : nullptr, l.data() + pos, r.data() + pos, (int) c);
        }
    }
};

static void testParams()
{
    std::set<std::string> ids;
    for (int i = 0; i < hb::pCount; ++i)
    {
        const auto& k = hb::kParams[i];
        CHECK (ids.insert (k.id).second, "duplicate id %s", k.id);
        CHECK (k.def >= k.lo && k.def <= k.hi, "%s default out of range", k.id);
        CHECK (k.tip && k.tip[0], "%s has no tip", k.id);
        CHECK (k.lo < k.hi, "%s range", k.id);
    }
    CHECK (hb::indexOf ("drive") == hb::pDrive && hb::indexOf ("nope") < 0, "indexOf");
    std::printf ("params: %d automatable parameters\n", (int) hb::pCount);
}

static void testIdentity()
{
    Rig g; const Vec s = sine (440, 0.5, 96000); g.e.loadSample (s.data(), nullptr, s.size(), SR);
    g.p[hb::pMix] = 100; g.apply(); g.e.noteOn (60, 1.0f);
    Vec l, r; g.run (l, r, 48000);
    const double in = rms (s, 0, 48000), out = rms (l, 4800, 48000);
    CHECK (std::fabs (dbOf (out) - dbOf (in)) < 1.0, "identity level %.2f dB vs %.2f dB", dbOf (out), dbOf (in));
    const double f = freqOf (l, 4800, 48000);
    CHECK (std::fabs (f - 440.0) < 8.0, "identity pitch %.1f Hz", f);
    CHECK (finite (l), "finite");
}

static void testPitchAndSpeed()
{
    Rig g; const Vec s = sine (300, 0.5, 192000); g.e.loadSample (s.data(), nullptr, s.size(), SR);
    g.p[hb::pPitch] = 12; g.apply(); g.e.noteOn (60, 1.0f);
    Vec l, r; g.run (l, r, 48000);
    const double f = freqOf (l, 9600, 48000);
    CHECK (std::fabs (f - 600.0) < 12.0, "+12 st pitch %.1f Hz (want 600)", f);
    CHECK (std::fabs (g.e.playhead() - 48000.0 / 192000.0) < 0.01, "position still moves at normal speed: %.4f", g.e.playhead());
    // key to pitch: note 72 is one octave above root 60
    Rig k; k.e.loadSample (s.data(), nullptr, s.size(), SR); k.apply(); k.e.noteOn (72, 1.0f); k.run (l, r, 48000);
    const double fk = freqOf (l, 9600, 48000);
    CHECK (std::fabs (fk - 600.0) < 12.0, "key pitch %.1f Hz", fk);
}

static void testFreezeReverseSpacing()
{
    const Vec s = noise (192000, 0.4f);
    { // Wave spacing 0 holds the position
        Rig g; g.e.loadSample (s.data(), nullptr, s.size(), SR); g.p[hb::pWaveSpacing] = 0; g.p[hb::pStart] = 50; g.apply(); g.e.noteOn (60, 1.0f);
        Vec l, r; g.run (l, r, 24000); const float a = g.e.playhead(); g.run (l, r, 24000); const float b = g.e.playhead();
        CHECK (std::fabs (a - 0.5f) < 0.002f && std::fabs (b - a) < 1e-5f, "frozen position %.4f %.4f", a, b);
        CHECK (rms (l, 0, 24000) > 0.05, "frozen output is not silent");
    }
    { // negative wave spacing moves backwards
        Rig g; g.e.loadSample (s.data(), nullptr, s.size(), SR); g.p[hb::pWaveSpacing] = -100; g.p[hb::pStart] = 90; g.apply(); g.e.noteOn (60, 1.0f);
        Vec l, r; g.run (l, r, 24000); const float a = g.e.playhead();
        CHECK (a < 0.9f - 0.1f, "reverse position %.3f", a);
    }
    { // hold button stops the position while on
        Rig g; g.e.loadSample (s.data(), nullptr, s.size(), SR); g.apply(); g.e.noteOn (60, 1.0f);
        Vec l, r; g.run (l, r, 12000); const float a = g.e.playhead();
        g.p[hb::pHold] = 1; g.apply(); g.run (l, r, 12000); const float b = g.e.playhead();
        CHECK (std::fabs (b - a) < 0.003f, "hold stops the position %.4f -> %.4f", a, b);
        g.p[hb::pHold] = 0; g.apply(); g.run (l, r, 12000); CHECK (g.e.playhead() > b + 0.03f, "released hold moves again");
    }
    { // spacing 50 percent: thicker but level stays within 2 dB of 100 percent
        auto lev = [&] (float sp) { Rig g; g.e.loadSample (s.data(), nullptr, s.size(), SR); g.p[hb::pSpacing] = sp; g.apply(); g.e.noteOn (60, 1.0f); Vec l, r; g.run (l, r, 48000); return rms (l, 4800, 48000); };
        const double a = lev (100), b = lev (50), c = lev (25), d = lev (200);
        CHECK (std::fabs (dbOf (b) - dbOf (a)) < 2.0 && std::fabs (dbOf (c) - dbOf (a)) < 3.0, "spacing level %.2f %.2f %.2f dB", dbOf (a), dbOf (b), dbOf (c));
        CHECK (d < a * 0.95, "wider spacing leaves gaps (%.2f dB)", dbOf (d));
    }
}

static void testRiserTone()
{
    const Vec s = noise (192000, 0.4f);
    Rig g; g.e.loadSample (s.data(), nullptr, s.size(), SR);
    g.p[hb::pRiseOn] = 1; g.p[hb::pRiseTime] = 4; g.p[hb::pRiseCurve] = 1; g.p[hb::pRisePitch] = 0; g.p[hb::pRiseGrain] = 100; g.p[hb::pRiseTone] = 100;
    g.p[hb::pRiseKey] = 0; g.p[hb::pRiseHz] = 440; g.p[hb::pWaveSpacing] = 0; g.p[hb::pStart] = 30; g.p[hb::pGrainLen] = 200; g.p[hb::pSpacing] = 150; g.apply();
    g.e.noteOn (60, 1.0f);
    Vec l, r; g.run (l, r, (size_t) (5.0 * SR));
    const size_t lag = (size_t) std::lround (SR / 440.0);
    const double early = autoc (l, (size_t) (0.2 * SR), (size_t) (0.5 * SR), lag), late = std::max (autoc (l, (size_t) (4.4 * SR), (size_t) (4.9 * SR), lag), autoc (l, (size_t) (4.4 * SR), (size_t) (4.9 * SR), lag + 1));
    CHECK (late > 0.5 && early < 0.35, "riser turns into a 440 Hz tone: autocorrelation early %.2f late %.2f", early, late);
    CHECK (std::fabs (g.e.riserProgress() - 1.0f) < 0.02f, "riser progress %.2f", g.e.riserProgress());
    CHECK (finite (l) && rms (l, (size_t) (4.4 * SR), (size_t) (4.9 * SR)) > 0.03, "riser output audible and finite");
    // key follow: playing note 81 (A5, 880 Hz) gives a tone at 880
    Rig k; k.e.loadSample (s.data(), nullptr, s.size(), SR); k.p = g.p; k.p[hb::pRiseKey] = 1; k.p[hb::pKeyTo] = 1; k.p[hb::pRiseTime] = 1; k.apply(); k.e.noteOn (81, 1.0f); // keyTo percent keeps pitch fixed
    // note 81 < 72? it is above: percent mode moves the start, tone follows the key
    k.run (l, r, (size_t) (1.6 * SR));
    const size_t l2 = (size_t) std::lround (SR / 880.0);
    CHECK (std::max (autoc (l, (size_t) (1.2 * SR), (size_t) (1.6 * SR), l2), autoc (l, (size_t) (1.2 * SR), (size_t) (1.6 * SR), l2 + 1)) > 0.45, "riser follows the played key");
}

static void testLiveFreeze()
{
    Rig g; g.p[hb::pMode] = 1; g.p[hb::pLiveLen] = 2; g.p[hb::pMix] = 100; g.p[hb::pGrainLen] = 120; g.p[hb::pWaveSpacing] = 100; g.apply();
    const Vec in = sine (330, 0.5, 96000);
    Vec l, r; g.run (l, r, 96000, &in, 0);   // fill the whole 2 s capture ring first
    // live, not frozen: lag stays constant, the output is the live sound a little behind
    CHECK (std::fabs (freqOf (l, 24000, 96000) - 330.0) < 8.0, "live follow keeps pitch %.1f", freqOf (l, 24000, 96000));
    g.p[hb::pFreeze] = 1; g.apply();
    Vec silence (96000, 0.f); Vec l2, r2; g.run (l2, r2, 48000, &silence, 0);
    CHECK (g.e.isFrozen(), "frozen");
    CHECK (rms (l2, 4800, 48000) > 0.15 && std::fabs (freqOf (l2, 4800, 48000) - 330.0) < 10.0, "freeze keeps playing the captured tone with silence at the input: rms %.3f freq %.1f", rms (l2, 4800, 48000), freqOf (l2, 4800, 48000));
    // mix 0 passes the input bit for bit, even while frozen
    g.p[hb::pMix] = 0; g.apply(); Vec in2 = noise (4096, 0.5f, 3); Vec o2, o3; g.run (o2, o3, 4096, &in2, 0);
    // smoothing: allow the 10 ms glide to settle, then compare
    Vec in3 = noise (9600, 0.5f, 5); g.run (o2, o3, 9600, &in3, 0); bool exact = true; for (size_t i = 9000; i < 9600; ++i) if (o2[i] != in3[i]) exact = false;
    CHECK (exact, "mix 0 is bit exact");
    // capture to sample then play with MIDI
    g.p[hb::pMode] = 0; g.p[hb::pMix] = 100; g.p[hb::pFreeze] = 0; g.apply(); g.e.captureToSample();
    CHECK (g.e.sampleLength() > 1000, "captured %zu samples", g.e.sampleLength());
    g.e.noteOn (60, 1.0f); Vec l3, r3; g.run (l3, r3, 24000);
    CHECK (rms (l3, 4800, 24000) > 0.05 && finite (l3), "captured sample plays");
}

static void testTransients()
{
    Vec s (96000, 0.f); for (int k = 0; k < 8; ++k) for (int i = 0; i < 400; ++i) s[(size_t) (6000 + k * 12000 + i)] = 0.9f * std::exp (-(float) i / 80.0f) * std::sin ((float) i * 0.4f);
    for (auto& x : s) x += 0.002f * ((&x - &s[0]) % 7 - 3);
    Rig g; g.e.loadSample (s.data(), nullptr, s.size(), SR);
    const auto& d = g.e.detectedTransients();
    int hit = 0; for (int k = 0; k < 8; ++k) for (size_t t : d) if (std::llabs ((long long) t - (6000 + k * 12000)) < 768) { ++hit; break; }
    CHECK (hit >= 7 && d.size() <= 11, "transients found %d of 8, total %zu", hit, d.size());
    // key to transient: note 74 picks slice 2
    g.p[hb::pKeyTo] = 3; g.apply(); g.e.noteOn (74, 1.0f);
    CHECK (g.e.playhead() > 0.2f, "key to transient picks a later slice: %.3f", g.e.playhead());
    // key to percent: C6 (84) is 50 percent
    Rig h; h.e.loadSample (s.data(), nullptr, s.size(), SR); h.p[hb::pKeyTo] = 1; h.apply(); h.e.noteOn (84, 1.0f);
    CHECK (std::fabs (h.e.playhead() - 0.5f) < 0.01f, "key to percent %.3f", h.e.playhead());
}

static void testChain()
{
    const Vec s = sine (220, 0.35, 96000);
    auto run = [&] (hb::Params p, Vec& l, Vec& r) { Rig g; g.e.loadSample (s.data(), nullptr, s.size(), SR); g.p = p; g.apply(); g.e.noteOn (60, 1.0f); g.run (l, r, 48000); };
    hb::Params base; Vec l0, r0; run (base, l0, r0); const double ref = rms (l0, 9600, 48000);
    { hb::Params p = base; p[hb::pDrive] = 100; Vec l, r; run (p, l, r); CHECK (std::fabs (dbOf (rms (l, 9600, 48000)) - dbOf (ref)) < 2.5, "drive keeps level: %.2f vs %.2f dB", dbOf (rms (l, 9600, 48000)), dbOf (ref)); CHECK (finite (l), "drive finite"); }
    { hb::Params p = base; p[hb::pTransistor] = 100; Vec l, r; run (p, l, r); CHECK (std::fabs (dbOf (rms (l, 9600, 48000)) - dbOf (ref)) < 2.5, "transistor keeps level: %.2f dB", dbOf (rms (l, 9600, 48000))); }
    { hb::Params p = base; p[hb::pFuzz] = 100; Vec l, r; run (p, l, r); CHECK (std::fabs (dbOf (rms (l, 9600, 48000)) - dbOf (ref)) < 3.0, "fuzz keeps level: %.2f dB", dbOf (rms (l, 9600, 48000))); }
    { hb::Params p = base; p[hb::pWidth] = 0; Vec l, r; Rig g; g.e.loadSample (s.data(), nullptr, s.size(), SR); g.p = p; g.p[hb::pPan] = 100; g.apply(); g.e.noteOn (60, 1.0f); g.run (l, r, 48000); double d = 0; for (size_t i = 9600; i < 48000; ++i) d = std::max (d, (double) std::fabs (l[i] - r[i])); CHECK (d < 1e-4, "width 0 is mono (%.6f)", d); }
    { hb::Params p = base; p[hb::pFiltType] = 1; p[hb::pCutoff] = 100; Vec l, r; run (p, l, r); CHECK (rms (l, 9600, 48000) < ref * 0.3, "low pass at 100 Hz cuts a 220 Hz tone"); }
    { const Vec b = sine (60, 0.35, 96000); auto lv = [&] (float g12) { Rig g; g.e.loadSample (b.data(), nullptr, b.size(), SR); g.p[hb::pLowG] = g12; g.apply(); g.e.noteOn (60, 1.0f); Vec l, r; g.run (l, r, 48000); return rms (l, 9600, 48000); };
      const double up = dbOf (lv (12)) - dbOf (lv (0)); CHECK (up > 8.0 && up < 13.0, "low shelf +12 dB lifts a 60 Hz tone by %.2f dB", up); }
    { // Dance glues the highs and leaves the bass alone
        auto lvl = [&] (double hz, float dance) { Vec t = sine (hz, 0.8, 96000); Rig g; g.e.loadSample (t.data(), nullptr, t.size(), SR); g.p[hb::pDance] = dance; g.apply(); g.e.noteOn (60, 1.0f); Vec l, r; g.run (l, r, 48000); return rms (l, 9600, 48000); };
        const double h0 = lvl (1500, 0), h1 = lvl (1500, 100), b0 = lvl (60, 0), b1 = lvl (60, 100);
        CHECK (dbOf (h1) < dbOf (h0) - 1.0 || dbOf (h1) > dbOf (h0) + 0.0, "dance acts on highs (%.2f -> %.2f dB)", dbOf (h0), dbOf (h1));
        CHECK (std::fabs (dbOf (b1) - dbOf (b0)) < 1.0, "dance leaves the bass (%.2f -> %.2f dB)", dbOf (b0), dbOf (b1));
    }
}

static void testSafety()
{
    const Vec s = noise (96000, 1.0f, 11);
    Rig g; g.e.loadSample (s.data(), nullptr, s.size(), SR);
    for (int i = 0; i < hb::pCount; ++i) g.p.v[i] = hb::kParams[i].hi;   // everything at the maximum
    g.p[hb::pMode] = 0; g.p[hb::pNasty] = 1; g.p[hb::pMix] = 100; g.p[hb::pVoices] = 8; g.p[hb::pLiveLen] = 8; g.p[hb::pFreeze] = 0; g.apply();
    for (int n = 48; n < 60; ++n) g.e.noteOn (n, 1.0f);
    Vec l, r; g.run (l, r, 96000);
    double pk = 0; for (float x : l) pk = std::max (pk, (double) std::fabs (x)); for (float x : r) pk = std::max (pk, (double) std::fabs (x));
    CHECK (finite (l) && finite (r), "everything at maximum stays finite");
    CHECK (pk <= 1.0001, "safety limiter holds the ceiling: peak %.4f", pk);
    // minimum values
    Rig m; m.e.loadSample (s.data(), nullptr, s.size(), SR); for (int i = 0; i < hb::pCount; ++i) m.p.v[i] = hb::kParams[i].lo; m.p[hb::pMix] = 100; m.p[hb::pVoices] = 1; m.p[hb::pLiveLen] = 0.5f; m.apply(); m.e.noteOn (60, 1.0f);
    m.run (l, r, 48000); CHECK (finite (l) && finite (r), "everything at the minimum stays finite");
    // FX mode with extreme settings on noise input
    Rig f; for (int i = 0; i < hb::pCount; ++i) f.p.v[i] = hb::kParams[i].hi; f.p[hb::pMode] = 1; f.p[hb::pLiveLen] = 1; f.apply(); Vec in = noise (96000, 1.0f, 13); f.run (l, r, 96000, &in, 0);
    CHECK (finite (l) && finite (r), "FX mode at maximum stays finite");
    // no input in FX mode
    Rig z; z.p[hb::pMode] = 1; z.apply(); z.run (l, r, 48000); CHECK (finite (l), "FX mode without input");
}

static void testResponsiveness()
{
    // a parameter step must show in the output within one chunk of the next block (no hidden lag)
    const Vec s = sine (440, 0.5, 192000);
    Rig g; g.e.loadSample (s.data(), nullptr, s.size(), SR); g.apply(); g.e.noteOn (60, 1.0f);
    Vec l, r; g.run (l, r, 24000);
    g.p[hb::pOut] = -12; g.apply();
    Vec l2, r2; g.run (l2, r2, 4800);
    // compare envelope: find first 64 sample window whose peak dropped by more than 1 dB vs the earlier peak
    auto pk = [&] (size_t a) { double m = 0; for (size_t i = a; i < a + 64; ++i) m = std::max (m, (double) std::fabs (l2[i])); return m; };
    size_t first = 0; for (size_t i = 0; i + 64 < l2.size(); i += 8) if (pk (i) < 0.5 * 0.85) { first = i; break; }
    CHECK (first > 0 && first < 700, "output step becomes audible after %zu samples (%.1f ms)", first, first * 1000.0 / SR);
    std::printf ("knob to sound: output level step audible after %zu samples (%.1f ms, includes the 10 ms smoothing)\n", first, first * 1000.0 / SR);
    CHECK (g.e.latencySamples() == 0, "reported latency");
}

static void testSpeed()
{
    const Vec s = noise (192000, 0.5f, 21);
    Rig g; g.e.loadSample (s.data(), nullptr, s.size(), SR);
    g.p[hb::pVoices] = 8; g.p[hb::pSpacing] = 25; g.p[hb::pGrainLen] = 400; g.p[hb::pDrive] = 60; g.p[hb::pDance] = 60; g.p[hb::pFiltType] = 1; g.p[hb::pLowG] = 3; g.p[hb::pWidth] = 150; g.apply();
    for (int n = 48; n < 56; ++n) g.e.noteOn (n, 1.0f);
    Vec l, r; const auto t0 = std::chrono::steady_clock::now(); g.run (l, r, 480000); const double sec = std::chrono::duration<double> (std::chrono::steady_clock::now() - t0).count();
    const double x = 10.0 / sec;
    std::printf ("speed: 8 voices, dense grains, drive, dance, filter, EQ: %.0fx realtime (%.1f percent of one core at 48 kHz), grains alive %d\n", x, 100.0 / x, g.e.activeGrains());
    CHECK (x > 8.0, "worst case should run at least 8x realtime, got %.1fx", x);
    Rig f; f.p[hb::pMode] = 1; f.p[hb::pSpacing] = 50; f.apply(); Vec in = noise (480000, 0.4f, 9); const auto t1 = std::chrono::steady_clock::now(); f.run (l, r, 480000, &in, 0);
    const double sec2 = std::chrono::duration<double> (std::chrono::steady_clock::now() - t1).count();
    std::printf ("speed: FX mode default chain: %.0fx realtime (%.1f percent of one core)\n", 10.0 / sec2, 10.0 / sec2 > 0 ? 100.0 / (10.0 / sec2) : 0.0);
}

static void testPluginHooks()
{
    // sample prepared off the audio thread, swapped in without allocation; same result as loadSample
    const Vec s = noise (96000, 0.5f, 33);
    Rig a; a.e.loadSample (s.data(), nullptr, s.size(), SR);
    Rig b; hb::Engine::Sample smp; hb::Engine::makeSample (smp, s.data(), nullptr, s.size(), SR, SR); b.e.swapSample (smp);
    CHECK (smp.n == 0, "old sample handed back");
    CHECK (b.e.sampleLength() == a.e.sampleLength() && b.e.detectedTransients() == a.e.detectedTransients(), "swapped sample equals loaded sample");
    b.e.noteOn (60, 1.0f); a.e.noteOn (60, 1.0f);
    Vec l1, r1, l2, r2; a.run (l1, r1, 4800); b.run (l2, r2, 4800);
    double d = 0; for (size_t i = 0; i < l1.size(); ++i) d = std::max (d, (double) std::fabs (l1[i] - l2[i]));
    CHECK (d == 0.0, "same audio from swapped sample, max diff %g", d);
    // capture without allocation: needs reserved capacity, copies the ring oldest first
    Rig f; f.p[hb::pMode] = 1; f.p[hb::pLiveLen] = 1; f.apply(); Vec in = noise (48000, 0.4f, 5); Vec l, r; f.run (l, r, 48000, &in, 0);
    hb::Engine::Buf cap; CHECK (! f.e.captureInto (cap), "refuses without reserved capacity");
    cap.l.reserve (f.e.ringLength() + 8); cap.r.reserve (f.e.ringLength() + 8);
    CHECK (f.e.captureInto (cap) && cap.n == f.e.ringLength(), "capture length");
    double dd = 0; for (size_t i = 0; i < cap.n; ++i) dd = std::max (dd, (double) std::fabs (cap.l[i] - in[in.size() - cap.n + i]));
    CHECK (dd == 0.0, "captured audio equals the last second played, max diff %g", dd);
    float marks[256]; CHECK (f.e.grainMarks (marks, 256) >= 0, "grain marks");
    float heads[8]; CHECK (f.e.voiceHeads (heads, 8) == 1, "FX mode shows one read position");
}

int main()
{
    testParams(); testIdentity(); testPitchAndSpeed(); testFreezeReverseSpacing(); testRiserTone(); testLiveFreeze(); testTransients(); testChain(); testSafety(); testResponsiveness(); testSpeed(); testPluginHooks();
    std::printf ("%d checks, %d failed\n", checks, fails);
    return fails ? 1 : 0;
}
