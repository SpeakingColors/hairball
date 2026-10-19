// Compares the JS engine (docs/mockups/engine.js) with the C++ core: node tools/parity.js /path/to/cpp_dump.txt
const fs = require('fs'); const E = require('../docs/mockups/engine.js'); const table = require('../docs/mockups/params.json');
const defaults = () => { const o = {}; for (const p of table) o[p.id] = p.def_; return o; };
const mk = ov => { const o = defaults(); for (const k in ov) { const p = table.find(t => t.id === k); o[k] = Math.min(p.hi, Math.max(p.lo, ov[k])); } return o; };
function sig(n) { const l = new Float32Array(n), r = new Float32Array(n);
  for (let i = 0; i < n; ++i) { const t = i / 48000, c = (i % 12000) < 300 ? 0.6 * (1 - (i % 12000) / 300) : 0;
    l[i] = 0.3 * Math.sin(2 * Math.PI * 220 * t) + 0.15 * Math.sin(2 * Math.PI * 1330 * t) + c; r[i] = 0.3 * Math.sin(2 * Math.PI * 227 * t) + 0.15 * Math.sin(2 * Math.PI * 910 * t) + c; }
  return [l, r]; }
function run(ov, fx) {
  const [l, r] = sig(48000), p = mk(ov); p.mode = fx ? 1 : 0; const e = new E(48000, p); const ol = new Float32Array(48000), orr = new Float32Array(48000);
  if (!fx) { e.loadSample(l, r, 48000); e.noteOn(60, .9); e.noteOn(67, .7); }
  const blocks = [64, 100, 480, 33, 1024]; let pos = 0, bi = 0;
  while (pos < 48000) { const n = Math.min(blocks[bi++ % 5], 48000 - pos); if (!fx && pos > 24000 && pos < 24600) e.noteOff(60);
    e.process(l.subarray(pos), r.subarray(pos), ol.subarray(pos), orr.subarray(pos), n); pos += n; }
  const out = []; for (let b = 0; b < 46; ++b) { let a = 0; for (let i = 0; i < 1024; ++i) a += ol[b * 1024 + i] ** 2; out.push(Math.sqrt(a / 1024)); } return out;
}
const sc = { inst_default: [{}, 0], inst_riser_chain: [{ riseOn: 1, riseTime: .5, drive: 40, transistor: 30, dance: 50, filtType: 1, cutoff: 3000, lowG: 6, highG: -4, width: 150 }, 0],
  inst_rev_penv_trans: [{ waveSpacing: -100, pitchEnv: 12, pitchEnvT: 400, spacing: 60, pan: 60, transMode: 2 }, 0],
  fx_riser: [{ freeze: 0, riseOn: 1, riseTime: .4, fuzz: 30 }, 1], fx_hold_mix: [{ hold: 1, mix: 70 }, 1] };
const ref = {}; for (const line of fs.readFileSync(process.argv[2], 'utf8').trim().split('\n')) { const a = line.split(' '); ref[a[0]] = a.slice(1).map(Number); }
let bad = 0;
for (const k in sc) { const js = run(...sc[k]), cp = ref[k]; let maxd = 0, maxr = 0; for (let i = 0; i < 46; ++i) { const d = Math.abs(js[i] - cp[i]); maxd = Math.max(maxd, d); maxr = Math.max(maxr, cp[i]); }
  const ok = maxd < 0.03 * Math.max(maxr, 1e-3); if (!ok) bad++; console.log((ok ? 'ok   ' : 'FAIL ') + k + '  max diff ' + maxd.toFixed(5) + '  peak rms ' + maxr.toFixed(4)); }
if (process.env.DUMP) for (const k in sc) { const js = run(...sc[k]); console.log(k, js.map((x,i)=>(x-ref[k][i]).toFixed(4)).join(" ")); }
process.exit(bad ? 1 : 0);
