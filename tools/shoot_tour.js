// Takes the interface-tour pictures of the manual from the approved mockup (docs/mockups/Hairball.html): numbered pink badges on
// every control, lettered teal badges on every display, one crop per panel, and docs/tour/hairball.json with the texts read from the page.
// Run: node tools/shoot_tour.js   (needs Playwright with Chromium; PLAYWRIGHT_MODULE and CHROMIUM override the defaults)
const path = require('path'), fs = require('fs');
const pw = require(process.env.PLAYWRIGHT_MODULE || '/home/claude/.npm-global/lib/node_modules/playwright');
const root = path.resolve(__dirname, '..'), out = path.join(root, 'docs/tour');
const TABS = [['grains', 'Grains page'], ['riser', 'Riser page'], ['thicken', 'Thicken page']];
(async () => {
  fs.mkdirSync(out, { recursive: true });
  const b = await pw.chromium.launch({ executablePath: process.env.CHROMIUM || '/opt/pw-browsers/chromium', args: ['--no-sandbox'] });
  const p = await b.newPage({ viewport: { width: 1400, height: 1100 }, deviceScaleFactor: 2, colorScheme: 'dark' });
  await p.addInitScript(() => { try { localStorage.setItem('hb.zoom', '1'); } catch (e) {} });
  await p.goto('file://' + path.join(root, 'docs/mockups/Hairball.html')); await p.waitForTimeout(700);
  // describe: panels and items for the tab that is showing
  const describe = (tab) => p.evaluate((tab) => {
    const $ = (s, r = document) => r.querySelector(s), $$ = (s, r = document) => [...r.querySelectorAll(s)];
    const rel = e => { const r = e.getBoundingClientRect(); return [r.x + scrollX, r.y + scrollY, r.width, r.height]; };
    const union = els => { const r = els.map(e => e.getBoundingClientRect()), x0 = Math.min(...r.map(q => q.x)), y0 = Math.min(...r.map(q => q.y)), x1 = Math.max(...r.map(q => q.right)), y1 = Math.max(...r.map(q => q.bottom)); return [x0 + scrollX, y0 + scrollY, x1 - x0, y1 - y0]; };
    const NAMES = { prev: 'Previous preset', pname: 'Preset menu', next: 'Next preset', psave: 'Save', prename: 'Rename', pdel: 'Delete', pexp: 'Export', pimp: 'Import', power: 'Audio (mockup page only)', bGuide: 'Guide', bSet: 'Settings', bAbout: 'About & Support', bZoom: 'Window size', srcSel: 'Audio source', bFreeze: 'Freeze', bHold: 'Hold', bPlay: 'Play', bRise: 'Riser', bCap: 'Capture', modeSeg: 'Mode' };
    let seq = 0; $('#modeSeg').setAttribute('data-tip', 'mode'); $('#pname').setAttribute('data-tip', 'Preset list: factory presets first, then your own.');
    const info = el => {
      const sid = tab + '_' + (++seq); el.setAttribute('data-shid', sid); if (el.id === 'modeSeg') el.setAttribute('data-tip', 'mode');
      const kind = el.classList.contains('knob') ? 'knob' : el.classList.contains('seg') ? 'choice' : el.tagName === 'SELECT' ? 'menu' : 'button';
      const val = kind === 'knob' ? ($('.v', el.closest('.knobwrap')) || {}).textContent || '' : '';
      return { sid, name: NAMES[el.id] || el.getAttribute('aria-label') || '', kind, value: val };
    };
    const items = els => els.filter(e => e.tagName !== 'CANVAS' && e.getBoundingClientRect().width > 1 && (e.dataset.tip || ['bGuide','bSet','bAbout'].includes(e.id))).map(e => ({ ...info(e), box: rel(e) }));
    const res = { top: [], panels: [] };
    res.top = items(['#prev', '#pname', '#next', '#psave', '#prename', '#pdel', '#pexp', '#pimp', '#power', '#bGuide', '#bSet', '#bAbout', '#bZoom'].map(s => $(s)).filter(Boolean));
    const strip = $('.strip .box.transport'), srcK = $('#srcK');
    res.panels.push({ title: 'Source, display and keys', wide: true, box: union([strip, $('#main'), srcK, $('#kbd')]),
      disp: [{ letter: 'A', id: 'main', box: rel($('#main')) }, { letter: 'B', id: 'kbd', box: rel($('#kbd')) }],
      els: [$('#modeSeg'), $('#srcSel'), $('#bFreeze'), $('#bHold'), $('#bPlay'), $('#bRise'), $('#bCap'), ...$$('[data-tip]', srcK)].filter(Boolean) });
    const outc = $('.outcol');
    res.panels.push({ title: 'Output', wide: false, box: union([outc]), disp: [{ letter: 'C', id: 'meter', box: rel($('#meter')) }], els: $$('[data-tip]', outc) });
        $$(`#pg-${tab} .sec`).forEach(s => {
      const disp = $$('canvas.gfx', s).map(c => ({ id: c.id.replace('g-', ''), box: rel(c) }));
      res.panels.push({ title: s.dataset.sec, wide: false, box: union([s]), disp, els: $$('[data-tip]', s) });
    });
    res.panels.forEach(P => { P.items = items(P.els); delete P.els; });
    res.box = rel($('#unit'));
    return res;
  }, tab);
  const badge = (list) => p.evaluate(list => {
    list.forEach(([txt, x, y, col]) => { const e = document.createElement('div'); e.className = 'badge87'; e.textContent = txt;
      e.style.cssText = `position:absolute;left:${x - 4}px;top:${y - 4}px;min-width:18px;height:18px;border-radius:9px;background:${col};color:#fff;font:bold 11px sans-serif;display:flex;align-items:center;justify-content:center;z-index:99999;box-shadow:0 0 0 2px #0a0a22;padding:0 3px;pointer-events:none`; document.body.appendChild(e); });
  }, list);
  const unbadge = () => p.evaluate(() => document.querySelectorAll('.badge87').forEach(e => e.remove()));
  const clip = x => ({ x: Math.max(0, x[0] - 4), y: Math.max(0, x[1] - 4), width: x[2] + 8, height: x[3] + 8 });
  let n = 0, panelNo = 0; const result = { key: 'hairball', preset: 'Init', tabs: [], top: null, items: [], fulls: [] };
  const seen = new Map(), letters = new Map([['Source, display and keys|main', 'A'], ['Source, display and keys|kbd', 'B'], ['Output|meter', 'C']]);   // text of element -> number, so shared parts keep one number across tabs
  for (const [tab, label] of TABS) {
    await p.click(`.tabs [data-page=${tab}]`); await p.waitForTimeout(250);
    const d = await describe(tab);
    const STATIC = { bGuide: 'Opens the short guide: how to use Instrument and FX, Freeze, Hold, Capture, Riser and Thicken, the Key to keys, and how to see a hint.', bSet: 'Menu: info bubbles on or off, power saving, show grains on the display, reset the window size to 100%, open the GitHub page.', bAbout: 'Studio 87, version, licence (AGPL-3.0-or-later), where your saved sounds live, and the Support on Ko-fi button. Nothing is sent anywhere; the button only opens a link when you click it.' };
    const ids = { bGuide: 1, bSet: 1, bAbout: 1 };
    const all = [...d.top, ...d.panels.flatMap(P => P.items)];
    for (const it of all) {
      if (it.tip) continue;
      const loc = p.locator(`[data-shid="${it.sid}"]`);
      const idAttr = await loc.getAttribute('id');
      if (idAttr && STATIC[idAttr]) { it.tip = STATIC[idAttr]; continue; }
      await loc.evaluate(e => { e.dispatchEvent(new PointerEvent('pointerout', { bubbles: true })); e.dispatchEvent(new PointerEvent('pointerover', { bubbles: true })); }); await p.waitForTimeout(1150);
      const html = await p.evaluate(() => { const t = document.querySelector('#tip'); return t.hidden ? '' : t.innerHTML; });
      const m = html.match(/^<b>(.*?)<\/b>(.*?)<i>(.*?)<\/i>$/s);
      if (m) { if (!it.name) it.name = m[1]; it.tip = (m[2] + ' (' + m[3] + ')').replace(/\s+/g, ' ').trim(); }
      else it.tip = html.replace(/<[^>]+>/g, '').trim();
      if (!it.name) it.name = it.tip.split('.')[0];
    }
    await p.evaluate(() => { const t = document.querySelector('#tip'); if (t) t.hidden = true; const o = document.querySelector('#overlay'); if (o) o.style.display = 'none'; });
    await p.mouse.move(2, 2);
    const num = (it) => { const k = it.name + '|' + it.tip + '|' + it.kind; if (!seen.has(k)) seen.set(k, ++n); return seen.get(k); };
    if (!result.top) { result.top = d.top.map(t => ({ ...t, n: num(t) })); }
    const badges = [];
    result.top.forEach(t => badges.push([t.n, t.box[0], t.box[1], '#FF1383']));
    for (const P of d.panels) {
      const first = !result.items.some(q => q.title === P.title);
      const rec = first ? { title: P.title, wide: P.wide, disp: P.disp[0] || null, disps: P.disp, tab, items: [], box: P.box, img: '' } : result.items.find(q => q.title === P.title);
      P.disp.forEach(x => { if (!x.letter) { const key = P.title + '|' + (x.id || ''); if (!letters.has(key)) letters.set(key, String.fromCharCode(65 + letters.size)); x.letter = letters.get(key); } badges.push([x.letter, x.box[0] + 2, x.box[1] + 2, '#0aa38a']); });
      P.items.forEach(it => { const nn = num(it); if (first) rec.items.push({ ...it, n: nn }); const sm = it.box[2] < 60 && it.box[3] < 40; badges.push([nn, it.box[0] + (sm ? -4 : 2), it.box[1] + (sm ? -6 : 2), '#FF1383']); });
      if (first) result.items.push(rec);
    }
    await badge(badges);
    const f = `hairball_full_${tab}.png`; await p.screenshot({ path: path.join(out, f), fullPage: true, clip: clip(d.box) }); result.fulls.push({ tab, label, img: f });
    // panel crops (once per panel)
    for (const P of d.panels) { const rec = result.items.find(q => q.title === P.title); if (!rec.img) { rec.img = `hairball_p${++panelNo}.png`; await p.screenshot({ path: path.join(out, rec.img), fullPage: true, clip: clip(P.box) }); } }
    await unbadge();
  }
  fs.writeFileSync(path.join(out, 'hairball.json'), JSON.stringify(result, null, 1));
  fs.writeFileSync(path.join(out, 'key.txt'), 'hairball\n');
  console.log('controls numbered:', n); result.items.forEach(P => console.log(P.title, '|', P.items.length, 'items, displays', (P.disps || []).map(x => x.letter + ':' + (x.id || '')).join(',')));
  await b.close();
})();
