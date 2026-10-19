// Hairball engine, JavaScript port of src/core/HairballCore.h (same maths, same parameter ids).
// Used by the design mockup so the page sounds like the plugin core. Not part of the plugin.
(function (root) {
'use strict';
const PI = Math.PI;
const clamp = (v, lo, hi) => v < lo ? lo : (v > hi ? hi : v);
const dbToLin = db => Math.pow(10, db / 20);

class Rng { constructor(s) { this.s = (s || 2463534242) >>> 0; }
  next01() { let s = this.s; s ^= s << 13; s >>>= 0; s ^= s >>> 17; s ^= s << 5; s >>>= 0; this.s = s; return (s >>> 8) / 16777216; }
  bi() { return this.next01() * 2 - 1; } }

class Biquad { constructor() { this.b0 = 1; this.b1 = 0; this.b2 = 0; this.a1 = 0; this.a2 = 0; this.z1 = 0; this.z2 = 0; }
  norm(B0, B1, B2, A0, A1, A2) { this.b0 = B0 / A0; this.b1 = B1 / A0; this.b2 = B2 / A0; this.a1 = A1 / A0; this.a2 = A2 / A0; }
  peak(sr, f, g, q) { const A = Math.pow(10, g / 40), w = 2 * PI * f / sr, al = Math.sin(w) / (2 * q), c = Math.cos(w); this.norm(1 + al * A, -2 * c, 1 - al * A, 1 + al / A, -2 * c, 1 - al / A); }
  lowShelf(sr, f, g) { const A = Math.pow(10, g / 40), w = 2 * PI * f / sr, c = Math.cos(w), s = Math.sin(w), al = s / 2 * Math.sqrt(2), t = 2 * Math.sqrt(A) * al;
    this.norm(A * ((A + 1) - (A - 1) * c + t), 2 * A * ((A - 1) - (A + 1) * c), A * ((A + 1) - (A - 1) * c - t), (A + 1) + (A - 1) * c + t, -2 * ((A - 1) + (A + 1) * c), (A + 1) + (A - 1) * c - t); }
  highShelf(sr, f, g) { const A = Math.pow(10, g / 40), w = 2 * PI * f / sr, c = Math.cos(w), s = Math.sin(w), al = s / 2 * Math.sqrt(2), t = 2 * Math.sqrt(A) * al;
    this.norm(A * ((A + 1) + (A - 1) * c + t), -2 * A * ((A - 1) + (A + 1) * c), A * ((A + 1) + (A - 1) * c - t), (A + 1) - (A - 1) * c + t, 2 * ((A - 1) - (A + 1) * c), (A + 1) - (A - 1) * c - t); }
  tick(x) { const y = this.b0 * x + this.z1; this.z1 = this.b1 * x - this.a1 * y + this.z2; this.z2 = this.b2 * x - this.a2 * y; return y; } }

class Svf { constructor() { this.g = .1; this.k = 1; this.a1 = 0; this.a2 = 0; this.a3 = 0; this.ic1 = 0; this.ic2 = 0; this.lp = 0; this.bp = 0; this.hp = 0; }
  set(sr, f, q) { f = Math.min(f, sr * .45); this.g = Math.tan(PI * f / sr); this.k = 1 / q; this.a1 = 1 / (1 + this.g * (this.g + this.k)); this.a2 = this.g * this.a1; this.a3 = this.g * this.a2; }
  tick(x) { const v3 = x - this.ic2, v1 = this.a1 * this.ic1 + this.a2 * v3, v2 = this.ic2 + this.a2 * this.ic1 + this.a3 * v3; this.ic1 = 2 * v1 - this.ic1; this.ic2 = 2 * v2 - this.ic2; this.bp = v1; this.lp = v2; this.hp = x - this.k * v1 - v2; } }

const ss01 = t => { t = t < 0 ? 0 : (t > 1 ? 1 : t); return t * t * (3 - 2 * t); };
const asymTanh = (u, nl) => u >= 0 ? Math.tanh(u) : nl * Math.tanh(u / nl);
function shaper(x, drive, xs, fuzz) {
  const dn = ss01((drive - .8) * 5), xn = ss01((xs - .8) * 5), fn = ss01((fuzz - .8) * 5); let y = x;
  if (drive > .001) { y = Math.tanh(x * (1 + drive * 5 + dn * 30)); if (dn > 0) y += dn * (Math.tanh(y * (2 + 10 * dn)) - y); }
  if (xs > .001) { const nl = Math.max(.2, 1 - .3 * xs - .25 * xn), u = y * (1 + xs * 5 + xn * 30); y = asymTanh(u, nl); y += (.3 * xs + .25 * xn) * y * y;
    if (xn > 0) { const y2 = asymTanh(y * (2 + 10 * xn) + .15 * xn, nl); y += xn * (y2 - y); } }
  if (fuzz > .001) { const fz = 1 + fuzz * 14 + fn * 70, bf = .5 * fuzz; y = Math.tanh(y * fz + bf) - Math.tanh(bf);
    if (fn > 0) { const r = Math.sqrt(y * y + .02) - .1414; y += fn * .7 * (r - .3); y = Math.tanh(y * (1 + .8 * fn)); } }
  return y;
}
function makeup(d, x, f) { let a = 0, b = 0; for (let i = 0; i < 64; ++i) { const s = .35 * Math.sin(6.2831853 * i / 64), y = shaper(s, d, x, f); a += s * s; b += y * y; } return b > 1e-9 ? clamp(Math.sqrt(a / b), .25, 4) : 1; }
const softLim = x => { const ax = Math.abs(x); if (ax <= .9) return x; const y = .9 + .1 * Math.tanh((ax - .9) * 10); return x < 0 ? -y : y; };

const KCHUNK = 32, KGRAINS = 24, KVOICES = 8, KRING = 8;

function newVoice() { const g = []; for (let i = 0; i < KGRAINS; ++i) g.push({ on: false, idx: 0, rate: 1, age: 0, len: 1, att: 1, gl: 1, gr: 1, norm: 1 });
  return { on: false, note: 60, stage: 0, vel: 1, env: 0, age: 0, riseU: 0, riseT: 0, penvT: 0, head: 0, lag: 1, semis: 0, lastHead: 0, acc: 0, lfo: 0, cnt: 0, odd: false, rnd: new Rng(), g }; }

class HairballEngine {
  constructor(sr, params) {
    this.sr = sr; this.P = Object.assign({}, params);
    this.ringCap = Math.floor(KRING * sr) + 8; this.live = { l: new Float32Array(this.ringCap), r: new Float32Array(this.ringCap), n: this.ringCap, ratio: 1 };
    this.smp = { l: new Float32Array(0), r: new Float32Array(0), n: 0, ratio: 1 };
    this.win = new Float32Array(1025); for (let i = 0; i <= 1024; ++i) this.win[i] = .5 - .5 * Math.cos(PI * i / 1024);
    this.voices = []; for (let i = 0; i < KVOICES; ++i) this.voices.push(newVoice());
    this.liveW = 0; this.frozen = false; this.ringN = Math.floor(4 * sr); this.detected = []; this.regions = [];
    this.lowSh = [new Biquad(), new Biquad()]; this.midBq = [new Biquad(), new Biquad()]; this.highSh = [new Biquad(), new Biquad()]; this.filt = [new Svf(), new Svf()];
    this.lowState = [0, 0]; this.sideLp = 0; this.dcX = [0, 0]; this.dcY = [0, 0];
    this.sm = {}; this.smInit = false; this.c = { low: 1e9, mid: 1e9, midF: 1e9, midQ: 1e9, high: 1e9, cut: 1e9, res: 1e9 }; this.mk = 1; this.mkD = -1; this.mkX = -1; this.mkF = -1;
    this.danceEnv = 0; this.gl = new Float32Array(KCHUNK); this.grr = new Float32Array(KCHUNK);
    this.stats = { grains: 0, rise: 0, head: 0 }; this.lastGr = 0;
  }
  setParams(p) { Object.assign(this.P, p); }
  loadSample(l, r, fileSr) {
    this.smp.l = Float32Array.from(l); this.smp.r = Float32Array.from(r || l); this.smp.n = l.length; this.smp.ratio = fileSr / this.sr; this.detect();
    for (const v of this.voices) { v.head = 0; for (const g of v.g) g.on = false; }
  }
  detect() {
    const d = this.detected = []; const s = this.smp; if (s.n < 2048) return; const hop = 256, frames = Math.floor(s.n / hop), e = new Float32Array(frames);
    for (let f = 0; f < frames; ++f) { let a = 0; for (let i = 0; i < hop; ++i) { const x = .5 * (s.l[f * hop + i] + s.r[f * hop + i]); a += x * x; } e[f] = Math.sqrt(a / hop); }
    const minGap = Math.floor(.08 * s.ratio * this.sr / hop) + 1; let last = 0, any = false;
    for (let f = 2; f + 1 < frames; ++f) { let avg = 0; const a0 = f > 24 ? f - 24 : 0; for (let k = a0; k < f; ++k) avg += e[k]; avg /= (f - a0);
      const onset = e[f] > 1.8 * avg + .01 && e[f] > e[f - 1] && e[f - 1] <= e[f - 2] * 1.5 + 1e-4; if (onset && (!any || f - last >= minGap)) { d.push(f * hop); last = f; any = true; } }
  }
  captureToSample() {
    const N = this.ringN, wr = this.liveW % N, l = new Float32Array(N), r = new Float32Array(N);
    for (let i = 0; i < N; ++i) { const k = (wr + i) % N; l[i] = this.live.l[k]; r[i] = this.live.r[k]; }
    this.smp = { l, r, n: N, ratio: 1 }; this.detect();
  }
  transList() { const t = this.P.transMode; return (t > .5 && t < 1.5 && this.regions.length) ? this.regions : this.detected; }
  placeHead(v, note) {
    const P = this.P, n = this.smp.n; let h = P.start * .01 * n; const k = Math.round(P.keyTo);
    if (k === 1 && note >= 72) h = clamp((note - 72) / 24, 0, 1) * n;
    else if (k === 2 && note >= 72) h = h + (note - 72) * (P.grainLen + P.attack) * .001 * this.sr * this.smp.ratio;
    else if (k === 3 && note >= 72) { const t = this.transList(), i = note - 72; if (i < t.length) h = t[i]; }
    v.head = h % Math.max(1, n); v.lastHead = v.head;
  }
  noteOn(note, vel) {
    const P = this.P;
    if (P.mode >= .5) { const f = this.voices[0]; f.note = note; f.penvT = 0; f.riseT = 0; return; }
    let idx = -1; const maxV = Math.floor(P.voices);
    for (let i = 0; i < maxV && i < KVOICES; ++i) if (!this.voices[i].on) { idx = i; break; }
    if (idx < 0) { let best = -1; for (let i = 0; i < maxV && i < KVOICES; ++i) if (this.voices[i].age > best || idx < 0) { best = this.voices[i].age; idx = i; } }
    if (idx < 0) idx = 0;
    const old = this.voices[idx], prevSemis = old.semis, was = old.on && old.env > .01, prevEnv = old.env;
    const v = this.voices[idx] = newVoice(); v.on = true; v.note = note; v.vel = vel; v.stage = 1; v.env = was ? prevEnv : 0; v.semis = was ? prevSemis : 0; v.rnd = new Rng(99991 + note * 7919 + idx * 31);
    this.placeHead(v, note);
  }
  noteOff(note) { for (const v of this.voices) if (v.on && v.note === note && v.stage < 3) v.stage = 3; }
  allNotesOff() { for (const v of this.voices) if (v.on) v.stage = 3; }
  playhead() { const v = this.voices[0]; if (this.P.mode >= .5) return clamp(1 - v.lag / this.ringN, 0, 1); return this.smp.n ? clamp(v.head / this.smp.n, 0, 1) : 0; }
  activeVoices() { return this.voices.filter(v => v.on).length; }
  activeGrains() { let n = 0; for (const v of this.voices) for (const g of v.g) if (g.on) ++n; return n; }
  riserProgress() { let m = 0; for (const v of this.voices) if (v.on) m = Math.max(m, v.riseU); return m; }

  process(inL, inR, outL, outR, n) { for (let pos = 0; pos < n; pos += KCHUNK) { const c = Math.min(KCHUNK, n - pos); this.chunk(inL, inR, outL, outR, pos, c); } }

  sampleAt(b, idx, ch, wrap) {
    const d = ch ? b.r : b.l, N = b.n; if (N < 4) return 0; const i1 = Math.floor(idx), f = idx - i1; let y0, y1, y2, y3;
    const at = k => { let i = i1 - 1 + k; if (wrap) { i %= N; if (i < 0) i += N; } else i = i < 0 ? 0 : (i >= N ? N - 1 : i); return d[i]; };
    y0 = at(0); y1 = at(1); y2 = at(2); y3 = at(3);
    const c0 = y1, c1 = .5 * (y2 - y0), c2 = y0 - 2.5 * y1 + 2 * y2 - .5 * y3, c3 = .5 * (y3 - y0) + 1.5 * (y1 - y2);
    return ((c3 * f + c2) * f + c1) * f + c0;
  }
  readWin(x) { x = clamp(x, 0, 1) * 1024; const i = Math.floor(x), f = x - i; return this.win[i] + (this.win[Math.min(i + 1, 1024)] - this.win[i]) * f; }

  chunk(inL, inR, outL, outR, off, c) {
    const P = this.P, sr = this.sr, fx = P.mode >= .5, mixT = P.mix * .01;
    const N = this.ringN = Math.min(this.ringCap - 8, Math.floor(P.liveLen * sr));
    const effFreeze = fx && (P.freeze >= .5 || P.hold >= .5);
    if (effFreeze && !this.frozen) { this.frozen = true; const v = this.voices[0]; v.lag = Math.max(P.capOff * .001 * sr + 16, Math.min(N, v.lag)); v.penvT = 0; }
    if (!effFreeze && this.frozen) this.frozen = false;
    if (fx && inL) { if (!this.frozen) for (let i = 0; i < c; ++i) { const k = this.liveW % N; this.live.l[k] = inL[off + i]; this.live.r[k] = inR ? inR[off + i] : inL[off + i]; ++this.liveW; } }
    else if (fx && !inL && !this.frozen) this.liveW += c;
    const a = 1 - Math.exp(-c / (.010 * sr)), S = this.sm, init = this.smInit;
    const sm = (k, t) => { if (!init || S[k] === undefined) S[k] = t; else S[k] += (t - S[k]) * a; };
    const nasty = P.nasty >= .5, cl = nasty ? 2 : 1;
    sm('mix', mixT); sm('out', dbToLin(P.out)); sm('width', P.width * .01); sm('bassMono', P.bassMono); sm('low', P.lowG); sm('mid', P.midG); sm('midF', P.midF); sm('high', P.highG);
    sm('reso', P.reso); sm('drive', Math.min(P.drive * .01, cl)); sm('x', Math.min(P.transistor * .01, cl)); sm('fuzz', Math.min(P.fuzz * .01, cl)); sm('dance', P.dance * .01);
    const gl = this.gl, gr = this.grr; gl.fill(0); gr.fill(0); let riseMax = 0;
    if (S.mix > 1e-5 || !init) {
      if (fx) { const v = this.voices[0]; v.on = true; v.stage = 2; v.env = 1; this.renderVoice(v, c, gl, gr, true, N); riseMax = v.riseU; }
      else for (let vi = 0; vi < KVOICES; ++vi) { const v = this.voices[vi]; if (v.on) { this.renderVoice(v, c, gl, gr, false, N); riseMax = Math.max(riseMax, v.riseU); } }
    } else for (const v of this.voices) for (const g of v.g) g.on = false;
    this.wetChain(gl, gr, c, riseMax);
    const lim = P.safeLim >= .5;
    for (let i = 0; i < c; ++i) {
      let wl = gl[i], wr = gr[i]; if (lim) { wl = softLim(wl); wr = softLim(wr); }
      const dl = inL ? inL[off + i] : 0, dr = inR ? inR[off + i] : dl, m = S.mix;
      outL[off + i] = m >= .99999 ? wl : dl * (1 - m) + wl * m; outR[off + i] = m >= .99999 ? wr : dr * (1 - m) + wr * m;
    }
    this.smInit = true;
  }

  renderVoice(v, c, gl, gr, fx, N) {
    const P = this.P, sr = this.sr, smp = this.smp, live = this.live, haveSrc = fx ? this.liveW > 64 : smp.n > 64, dt = 1 / sr;
    const attS = Math.max(.0008, P.attack * .001) * sr, loop = P.loop >= .5, envA = P.ampA, envD = P.ampD, envS = P.ampS * .01, envR = P.ampR;
    const glideC = P.glide > .5 ? 1 - Math.exp(-1 / (.001 * P.glide * sr)) : 1, keyPitch = Math.round(P.keyTo) === 0;
    const targetSemis = (!fx && keyPitch) ? (v.note - Math.round(P.root)) : 0, lfoInc = P.lfoSpeed * dt;
    for (let i = 0; i < c; ++i) {
      if (!fx) {
        if (v.stage === 1) { v.env += envA <= .5 ? 1 : 1000 / (envA * sr); if (v.env >= 1) { v.env = 1; v.stage = 2; } }
        else if (v.stage === 2) { const t = envS; if (v.env > t) { v.env -= envD <= .5 ? 1 : (1 - t) * 1000 / (envD * sr); if (v.env < t) v.env = t; } }
        else if (v.stage === 3) { v.env -= envR <= .5 ? 1 : Math.max(.0001, v.env) * 6.9 * 1000 / (envR * sr) + 1e-7; if (v.env <= .0005) { v.env = 0; v.on = false; for (const g of v.g) g.on = false; return; } }
      }
      v.age += dt; v.riseT += dt; v.penvT += dt;
      const riseOn = P.riseOn >= .5;
      if (riseOn) { const u = clamp(v.riseT / Math.max(.05, P.riseTime), 0, 1); v.riseU = Math.pow(u, P.riseCurve); } else { v.riseT = 0; v.riseU = Math.max(0, v.riseU - dt * 4); }
      v.semis += (targetSemis - v.semis) * glideC;
      const pe = P.pitchEnv * Math.pow(Math.max(0, 1 - clamp(v.penvT * 1000 / Math.max(5, P.pitchEnvT), 0, 1)), P.pitchEnvC);
      const semis = v.semis + P.pitch + P.fine * .01 + pe + v.riseU * P.risePitch;
      v.lfo += lfoInc; if (v.lfo >= 1) v.lfo -= 1;
      let ws = P.waveSpacing * .01 + P.lfoDepth * .01 * Math.sin(2 * PI * v.lfo); if (P.hold >= .5) ws = 0;
      if (haveSrc) {
        if (fx) {
          if (!this.frozen) { v.lag += 1 - ws; v.lag = Math.fround(clamp(Math.fround(v.lag), Math.fround(Math.fround(P.capOff * .001 * sr) + 16), Math.fround(N - 4096))); }
          else { v.lag -= ws; while (v.lag < 1) v.lag += N; while (v.lag > N) v.lag -= N; }
        } else {
          v.head += ws * smp.ratio; const n = smp.n, loopStart = clamp(P.start * .01, 0, .99) * n;
          if (loop) { if (v.head >= n) v.head = loopStart + ((v.head - n) % Math.max(1, n - loopStart)); else if (v.head < 0) v.head = n + (v.head % Math.max(1, n)); }
          else if (v.head >= n || v.head < 0) { if (v.stage < 3) v.stage = 3; v.head = clamp(v.head, 0, n - 1); }
        }
      }
      if (!haveSrc) v.cnt = 0;
      if (--v.cnt <= 0 && haveSrc) {
        const u = v.riseU, holdMs0 = P.grainLen; const holdMs = holdMs0 * Math.pow(Math.min(1, 3 / holdMs0), u * P.riseGrain * .01);
        let holdS = holdMs * .001 * sr, att = attS; const spc = P.spacing * .01, nominal = holdS + att, w = u * P.riseTone * .01;
        let hop = nominal * (spc + (1 - spc) * w), excess = Math.max(1, nominal / Math.max(1, hop));
        if (w > 0) {
          const hz = (!fx && P.riseKey >= .5) ? 440 * Math.pow(2, (v.note - 69) / 12) : P.riseHz, hopTone = sr / Math.max(20, hz);
          const a0 = Math.log(Math.max(8, hop)), b0 = Math.log(hopTone); hop = Math.exp(a0 + (b0 - a0) * w); holdS = holdS + (1 - holdS) * w; att = att + (hop - att) * w; excess = excess + (1 - excess) * w;
        }
        hop = Math.max(8, hop); let len = Math.max(16, Math.floor(holdS + 2 * att));
        let start = 0, lenUse = len; const rate = Math.pow(2, semis / 12) * (fx ? 1 : smp.ratio), rnd = P.rand * .01, jit = rnd > 0 ? v.rnd.bi() * rnd * .25 * sr : 0;
        if (fx) {
          let lag = v.lag + jit; const need = len * Math.max(0, rate - 1) + 64; lag = this.frozen ? lag : Math.max(lag, need);
          while (lag < 1) lag += N; while (lag > N) lag -= N; start = (this.liveW % N) - lag; while (start < 0) start += N;
        } else {
          start = v.head + jit * smp.ratio; const tm = Math.round(P.transMode);
          if (tm > 0) { const t = this.transList(), lo = Math.min(v.lastHead, v.head), hi = Math.max(v.lastHead, v.head); let k = 0, a = 0, b = t.length; const key = Math.max(0, Math.floor(lo));
            while (a < b) { const m = (a + b) >> 1; if (t[m] < key) a = m + 1; else b = m; } k = a; if (k < t.length && t[k] <= hi + 1) { start = t[k]; lenUse = Math.max(16, P.transHold * .001 * sr + 2 * attS); } }
          v.lastHead = v.head; const nn = smp.n; if (start < 0) start += nn; if (start >= nn) start = start % nn;
        }
        let gp = null, oldest = -1; for (const g of v.g) { if (!g.on) { gp = g; break; } if (g.age > oldest) { oldest = g.age; gp = g; } }
        const g = gp; g.on = true; g.idx = start; g.rate = rate; g.age = 0; g.len = Math.floor(lenUse); g.att = Math.max(1, Math.floor(Math.min(att, lenUse * .5)));
        const pan = P.pan * .01; v.odd = !v.odd; g.gl = v.odd ? 1 : 1 - pan; g.gr = v.odd ? 1 - pan : 1;
        const coh = (1 - rnd) / (1 + 8 * Math.abs(rate / (fx ? 1 : smp.ratio) - 1)); g.norm = Math.pow(excess, -(.5 + .5 * coh));
        v.acc += hop; const whole = Math.floor(v.acc); v.acc -= whole; v.cnt += Math.max(1, whole);
      }
      let sl = 0, sr_ = 0;
      for (const g of v.g) {
        if (!g.on) continue;
        const fi = g.age < g.att ? this.readWin(g.age / g.att) : 1, rem = g.len - g.age, fo = rem < g.att ? this.readWin(rem / g.att) : 1, gain = fi * fo * g.norm; let xl, xr;
        if (fx) { xl = this.sampleAt(live, g.idx, 0, true); xr = this.sampleAt(live, g.idx, 1, true); }
        else { if (!loop && (g.idx >= smp.n - 1 || g.idx < 0)) { g.on = false; continue; } xl = this.sampleAt(smp, g.idx, 0, loop); xr = this.sampleAt(smp, g.idx, 1, loop); }
        sl += xl * gain * g.gl; sr_ += xr * gain * g.gr; g.idx += g.rate;
        if (fx) { if (g.idx >= N) g.idx -= N; else if (g.idx < 0) g.idx += N; } else if (loop) { const n = smp.n; if (g.idx >= n) g.idx -= n; else if (g.idx < 0) g.idx += n; }
        if (++g.age >= g.len) g.on = false;
      }
      const e = v.env * (fx ? 1 : .4 + .6 * v.vel); gl[i] += sl * e; gr[i] += sr_ * e;
    }
  }

  wetChain(gl, gr, c, riseU) {
    const P = this.P, S = this.sm, sr = this.sr, C = this.c;
    if (Math.abs(S.width - 1) > 1e-3 || S.bassMono > 20) {
      const lpA = S.bassMono > 20 ? 1 - Math.exp(-2 * PI * S.bassMono / sr) : 0;
      for (let i = 0; i < c; ++i) { const m = .5 * (gl[i] + gr[i]); let s = .5 * (gl[i] - gr[i]); if (lpA > 0) { this.sideLp += (s - this.sideLp) * lpA; s -= this.sideLp; } s *= S.width; gl[i] = m + s; gr[i] = m - s; }
    }
    const cutT = P.filtType > .5 ? clamp(P.cutoff * Math.pow(2, riseU * P.riseFilter), 20, 20000) : 20000;
    if (Math.abs(S.low - C.low) > .01) { C.low = S.low; for (const b of this.lowSh) b.lowShelf(sr, 120, C.low); }
    if (Math.abs(S.mid - C.mid) > .01 || Math.abs(S.midF - C.midF) > .5 || Math.abs(P.midQ - C.midQ) > .01) { C.mid = S.mid; C.midF = S.midF; C.midQ = P.midQ; for (const b of this.midBq) b.peak(sr, C.midF, C.mid, C.midQ); }
    if (Math.abs(S.high - C.high) > .01) { C.high = S.high; for (const b of this.highSh) b.highShelf(sr, 8000, C.high); }
    const eqOn = [S.low, S.mid, S.high, C.low, C.mid, C.high].some(x => Math.abs(x) > .01 && Math.abs(x) < 100);
    if (eqOn) for (let i = 0; i < c; ++i) { gl[i] = this.highSh[0].tick(this.midBq[0].tick(this.lowSh[0].tick(gl[i]))); gr[i] = this.highSh[1].tick(this.midBq[1].tick(this.lowSh[1].tick(gr[i]))); }
    const ft = Math.round(P.filtType);
    if (ft > 0) {
      if (Math.abs(cutT - C.cut) > .5 || Math.abs(S.reso - C.res) > .005) { C.cut = cutT; C.res = S.reso; for (const f of this.filt) f.set(sr, C.cut, C.res); }
      const f0 = this.filt[0], f1 = this.filt[1];
      for (let i = 0; i < c; ++i) { f0.tick(gl[i]); f1.tick(gr[i]); gl[i] = ft === 1 ? f0.lp : (ft === 2 ? f0.hp : f0.bp * 2); gr[i] = ft === 1 ? f1.lp : (ft === 2 ? f1.hp : f1.bp * 2); }
    }
    if (S.drive > .001 || S.x > .001 || S.fuzz > .001) {
      if (Math.abs(S.drive - this.mkD) > .01 || Math.abs(S.x - this.mkX) > .01 || Math.abs(S.fuzz - this.mkF) > .01) { this.mkD = S.drive; this.mkX = S.x; this.mkF = S.fuzz; this.mk = makeup(this.mkD, this.mkX, this.mkF); }
      const arr = [gl, gr];
      for (let i = 0; i < c; ++i) for (let k = 0; k < 2; ++k) { const y = shaper(arr[k][i], S.drive, S.x, S.fuzz) * this.mk, o = y - this.dcX[k] + .9995 * this.dcY[k]; this.dcX[k] = y; this.dcY[k] = o; arr[k][i] = o; }
    }
    if (S.dance > .002) {
      const lpA = 1 - Math.exp(-2 * PI * 150 / sr), thr = -6 - 24 * S.dance, ratio = 2 + 4 * S.dance, mkDb = -thr * (1 - 1 / ratio) * .35;
      const atk = 1 - Math.exp(-1 / (.002 * sr)), rel = 1 - Math.exp(-1 / (.08 * sr)), ls = this.lowState;
      for (let i = 0; i < c; ++i) {
        ls[0] += (gl[i] - ls[0]) * lpA; ls[1] += (gr[i] - ls[1]) * lpA; const hl = gl[i] - ls[0], hr = gr[i] - ls[1], pk = Math.max(Math.abs(hl), Math.abs(hr));
        this.danceEnv += (pk - this.danceEnv) * (pk > this.danceEnv ? atk : rel); const db = 20 * Math.log10(Math.max(1e-6, this.danceEnv));
        const grDb = db > thr ? -(db - thr) * (1 - 1 / ratio) : 0, g = dbToLin(grDb + mkDb * S.dance); this.lastGr = grDb; gl[i] = ls[0] + hl * g; gr[i] = ls[1] + hr * g;
      }
    }
    else this.lastGr = 0;
    for (let i = 0; i < c; ++i) { gl[i] *= S.out; gr[i] *= S.out; }
  }
}
HairballEngine.shaper = shaper; HairballEngine.makeup = makeup; HairballEngine.Biquad = Biquad;
root.HairballEngine = HairballEngine;
if (typeof module !== 'undefined') module.exports = HairballEngine;
})(typeof window !== 'undefined' ? window : globalThis);
