// Reads the approved mockup in a headless browser and writes src/ui/Layout.h: where every control, graph and panel sits
// in the fixed design space (the editor scales that space to the window). Run: node tools/gen_layout.js
// Needs Playwright with Chromium (PLAYWRIGHT_MODULE and CHROMIUM env vars override the defaults).
const path = require('path'), fs = require('fs');
const pw = require(process.env.PLAYWRIGHT_MODULE || '/home/claude/.npm-global/lib/node_modules/playwright');
const root = path.resolve(__dirname, '..');

(async () => {
  const browser = await pw.chromium.launch({ executablePath: process.env.CHROMIUM || '/opt/pw-browsers/chromium', args: ['--no-sandbox'] });
  const page = await browser.newPage({ viewport: { width: 1400, height: 1000 } });
  await page.addInitScript(() => { try { localStorage.setItem('hb.zoom', '1'); } catch (e) {} });
  await page.goto('file://' + path.join(root, 'docs/mockups/Hairball.html'));
  await page.waitForTimeout(600);
  const pages = ['grains', 'riser', 'thicken'];
  const out = { pages: {} };
  for (const pg of pages) {
    await page.click(`.tabs [data-page=${pg}]`); await page.waitForTimeout(150);
    out.pages[pg] = await page.evaluate((pgName) => {
      const u = document.querySelector('#unit'), ur = u.getBoundingClientRect(), sc = ur.width / 1240;
      const R = el => { const r = el.getBoundingClientRect(); return [Math.round((r.left - ur.left) / sc), Math.round((r.top - ur.top) / sc), Math.round(r.width / sc), Math.round(r.height / sc)]; };
      const o = { H: Math.round(ur.height / sc), secs: [], knobs: [], segs: [], gfx: [], notes: [] };
      document.querySelectorAll(`#pg-${pgName} .sec`).forEach(s => {
        o.secs.push({ title: s.dataset.sec, r: R(s) });
        s.querySelectorAll('.knobwrap').forEach(k => { const id = k.querySelector('.knob').getAttribute('data-tip'); o.knobs.push({ id, r: R(k), knob: R(k.querySelector('.knob')), grp: s.dataset.sec }); });
        s.querySelectorAll('.choice').forEach(c => { const seg = c.querySelector('.seg'); const id = seg.getAttribute('data-tip'); o.segs.push({ id, label: R(c.querySelector('label')), r: R(seg), n: seg.querySelectorAll('button').length, grp: s.dataset.sec }); });
        s.querySelectorAll('canvas.gfx').forEach(c => o.gfx.push({ id: c.id.replace('g-', ''), r: R(c) }));
        s.querySelectorAll('.note').forEach(n => o.notes.push({ text: n.textContent, r: R(n) }));
      });
      return o;
    }, pg);
  }
  // fixed parts, measured on the grains page
  await page.click('.tabs [data-page=grains]'); await page.waitForTimeout(100);
  out.fixed = await page.evaluate(() => {
    const u = document.querySelector('#unit'), ur = u.getBoundingClientRect(), sc = ur.width / 1240;
    const R = el => { const r = el.getBoundingClientRect(); return [Math.round((r.left - ur.left) / sc), Math.round((r.top - ur.top) / sc), Math.round(r.width / sc), Math.round(r.height / sc)]; };
    const q = s => document.querySelector(s);
    const f = { brand: R(q('.brand')), prev: R(q('#prev')), pname: R(q('#pname')), next: R(q('#next')), save: R(q('#psave')), rename: R(q('#prename')), del: R(q('#pdel')), exp: R(q('#pexp')), imp: R(q('#pimp')),
      guide: R(q('#bGuide')), set: R(q('#bSet')), about: R(q('#bAbout')), zoom: R(q('#bZoom')),
      srcBox: R(q('.strip .box.transport')), modeInst: R(q('#modeSeg button[data-v="0"]')), modeFx: R(q('#modeSeg button[data-v="1"]')), srcSel: R(q('#srcSel')),
      freeze: R(q('#bFreeze')), hold: R(q('#bHold')), play: R(q('#bPlay')), rise: R(q('#bRise')), cap: R(q('#bCap')), hint: R(q('#srcHint')),
      disp: R(q('#main')), bufBox: R(q('#srcKnobs')), tabs: [...document.querySelectorAll('.tabs [role=tab]')].map(R), stat: R(q('#stat')),
      outBox: R(q('.outcol .box')), meter: R(q('#meter')), outKnobsBox: R(q('#outK')), kbd: R(q('#kbd')), octBox: R(q('.kbdctl')), octDn: R(q('#octDn')), octUp: R(q('#octUp')), octLbl: R(q('#octLbl')),
      footer: R(q('footer')), cheekL: R(q('#cheekL')), cheekR: R(q('#cheekR')), panel: R(q('.panel')) };
    f.srcKnobs = []; document.querySelectorAll('#srcK .knobwrap').forEach(k => f.srcKnobs.push({ id: k.querySelector('.knob').getAttribute('data-tip'), r: R(k) }));
    f.outKnobs = []; document.querySelectorAll('#outK .knobwrap').forEach(k => f.outKnobs.push({ id: k.querySelector('.knob').getAttribute('data-tip'), r: R(k) }));
    f.outSegs = []; document.querySelectorAll('#outK .choice').forEach(c => { const seg = c.querySelector('.seg'); f.outSegs.push({ id: seg.getAttribute('data-tip'), label: R(c.querySelector('label')), r: R(seg), n: seg.querySelectorAll('button').length }); });
    return f;
  });
  await browser.close();

  // one window size for all three tabs: the tallest tab decides, shorter tabs keep their panels at the top
  const H = Math.max(...pages.map(p => out.pages[p].H));
  const W = 1240;
  const rr = a => `{ ${a[0]}, ${a[1]}, ${a[2]}, ${a[3]} }`;
  const esc = s => s.replace(/\\/g, '\\\\').replace(/"/g, '\\"');
  let h = `// GENERATED by tools/gen_layout.js from docs/mockups/Hairball.html (the approved mockup). Do not edit by hand.\n// Fixed design space ${W} x ${H}; the editor scales it to the window.\n#pragma once\n#include <cstddef>\nnamespace hb::layout\n{\nstruct R { int x, y, w, h; };\nconstexpr int W = ${W}, H = ${H}, cheek = 34;\n`;
  h += `struct Sec { const char* title; R r; };\nstruct KnobPos { const char* id; R r; const char* group; };\nstruct SegPos { const char* id; R label; R r; int n; };\nstruct GfxPos { const char* id; R r; };\nstruct NotePos { const char* text; R r; };\n`;
  h += `struct Page { int secN; const Sec* secs; int knobN; const KnobPos* knobs; int segN; const SegPos* segs; int gfxN; const GfxPos* gfx; int noteN; const NotePos* notes; };\n`;
  for (const pg of pages) {
    const p = out.pages[pg];
    h += `inline constexpr Sec ${pg}Secs[] = {\n` + p.secs.map(s => `  { "${esc(s.title)}", ${rr(s.r)} },`).join('\n') + `\n};\n`;
    h += `inline constexpr KnobPos ${pg}Knobs[] = {\n` + p.knobs.map(k => `  { "${k.id}", ${rr(k.r)}, "${esc(k.grp)}" },`).join('\n') + `\n};\n`;
    h += `inline constexpr SegPos ${pg}Segs[] = {\n` + (p.segs.map(k => `  { "${k.id}", ${rr(k.label)}, ${rr(k.r)}, ${k.n} },`).join('\n') || '  { "", { 0,0,0,0 }, { 0,0,0,0 }, 0 },') + `\n};\n`;
    h += `inline constexpr GfxPos ${pg}Gfx[] = {\n` + (p.gfx.map(k => `  { "${k.id}", ${rr(k.r)} },`).join('\n') || '  { "", { 0,0,0,0 } },') + `\n};\n`;
    h += `inline constexpr NotePos ${pg}Notes[] = {\n` + (p.notes.map(k => `  { "${esc(k.text)}", ${rr(k.r)} },`).join('\n') || '  { "", { 0,0,0,0 } },') + `\n};\n`;
  }
  const cnt = (pg, k, ph) => (out.pages[pg][k].length || 0);
  h += `inline constexpr Page pages[3] = {\n` + pages.map(pg => { const p = out.pages[pg]; return `  { ${p.secs.length}, ${pg}Secs, ${p.knobs.length}, ${pg}Knobs, ${p.segs.length}, ${pg}Segs, ${p.gfx.length}, ${pg}Gfx, ${p.notes.length}, ${pg}Notes },`; }).join('\n') + `\n};\n`;
  const f = out.fixed;
  h += `struct Fixed { R brand, prev, pname, next, save, rename, del, exp, imp, guide, set, about, zoom, srcBox, modeInst, modeFx, srcSel, freeze, hold, play, rise, cap, hint, disp, bufBox, tabs[3], stat, outBox, meter, outKnobsBox, kbd, octBox, octDn, octUp, octLbl, footer; };\n`;
  h += `inline constexpr Fixed fixed = { ${['brand','prev','pname','next','save','rename','del','exp','imp','guide','set','about','zoom','srcBox','modeInst','modeFx','srcSel','freeze','hold','play','rise','cap','hint','disp','bufBox'].map(k => rr(f[k])).join(', ')}, { ${f.tabs.map(rr).join(', ')} }, ${['stat','outBox','meter','outKnobsBox','kbd','octBox','octDn','octUp','octLbl','footer'].map(k => rr(f[k])).join(', ')} };\n`;
  h += `inline constexpr KnobPos srcKnobs[] = {\n` + f.srcKnobs.map(k => `  { "${k.id}", ${rr(k.r)}, "Source" },`).join('\n') + `\n};\n`;
  h += `inline constexpr KnobPos outKnobs[] = {\n` + f.outKnobs.map(k => `  { "${k.id}", ${rr(k.r)}, "Output" },`).join('\n') + `\n};\n`;
  h += `inline constexpr SegPos outSegs[] = {\n` + f.outSegs.map(k => `  { "${k.id}", ${rr(k.label)}, ${rr(k.r)}, ${k.n} },`).join('\n') + `\n};\n`;
  h += `}\n`;
  fs.mkdirSync(path.join(root, 'src/ui'), { recursive: true });
  fs.writeFileSync(path.join(root, 'src/ui/Layout.h'), h);
  console.log('wrote src/ui/Layout.h', 'H per page', pages.map(p => out.pages[p].H).join(' '), 'chosen', H);
})();
