// Dumps block RMS of fixed scenarios so tools/parity.js can compare the JS engine with this core.
#include "../src/core/HairballCore.h"
#include <cstdio>
#include <cmath>
#include <vector>
using namespace hb;
static void sig(std::vector<float>& l, std::vector<float>& r, int n, double sr)
{
    l.resize(n); r.resize(n);
    for (int i = 0; i < n; ++i) {
        double t = i / sr; float c = (i % 12000) < 300 ? 0.6f * (1.0f - (i % 12000) / 300.0f) : 0.0f;
        l[i] = (float) (0.3 * std::sin(2 * M_PI * 220 * t) + 0.15 * std::sin(2 * M_PI * 1330 * t)) + c;
        r[i] = (float) (0.3 * std::sin(2 * M_PI * 227 * t) + 0.15 * std::sin(2 * M_PI * 910 * t)) + c;
    }
}
static void run(const char* name, Params p, bool fx, double sr)
{
    Engine e; e.prepare(sr); std::vector<float> l, r; sig(l, r, 48000, 48000);
    p.v[pMode] = fx ? 1 : 0; e.setParams(p);
    std::vector<float> ol(48000), orr(48000);
    if (!fx) { e.loadSample(l.data(), r.data(), l.size(), 48000); e.noteOn(60, 0.9f); e.noteOn(67, 0.7f); }
    // process in uneven blocks
    int pos = 0; int blocks[] = {64, 100, 480, 33, 1024};
    int bi = 0; std::vector<float> dl(l), dr(r);
    while (pos < 48000) {
        int n = std::min(blocks[bi++ % 5], 48000 - pos);
        if (!fx && pos > 24000 && pos < 24600) e.noteOff(60);
        e.process(l.data() + pos, r.data() + pos, ol.data() + pos, orr.data() + pos, n); pos += n;
    }
    printf("%s", name);
    for (int b = 0; b < 46; ++b) { double a = 0; for (int i = 0; i < 1024; ++i) { double x = ol[b*1024+i]; a += x*x; } printf(" %.6f", std::sqrt(a/1024)); }
    printf("\n");
}
int main()
{
    Params p;
    run("inst_default", p, false, 48000);
    Params q; q.set("riseOn", 1); q.set("riseTime", 0.5f); q.set("drive", 40); q.set("transistor", 30); q.set("dance", 50); q.set("filtType", 1); q.set("cutoff", 3000); q.set("lowG", 6); q.set("highG", -4); q.set("width", 150);
    run("inst_riser_chain", q, false, 48000);
    Params s; s.set("waveSpacing", -100); s.set("pitchEnv", 12); s.set("pitchEnvT", 400); s.set("spacing", 60); s.set("pan", 60); s.set("transMode", 2);
    run("inst_rev_penv_trans", s, false, 48000);
    Params f; f.set("freeze", 0); f.set("riseOn", 1); f.set("riseTime", 0.4f); f.set("fuzz", 30);
    run("fx_riser", f, true, 48000);
    Params g; g.set("hold", 1); g.set("mix", 70);
    run("fx_hold_mix", g, true, 48000);
    return 0;
}
