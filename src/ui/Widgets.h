// Interface parts that follow the approved mockup (docs/mockups/Hairball.html). Everything is drawn in a fixed design space
// (hb::layout::W x H, generated from the mockup by tools/gen_layout.js); the editor scales it to the window.
#pragma once
#include <juce_audio_utils/juce_audio_utils.h>
#include <map>
#include <set>
#include "../PluginProcessor.h"
#include "Layout.h"
#include "Format.h"

namespace hbui
{
using namespace juce;
namespace L = hb::layout;

namespace col
{
    inline const Colour ground { 0xff0b0b0d }, panel { 0xff15161a }, panel2 { 0xff1c1d22 }, line { 0xff2b2d34 }, ink { 0xfff4f5f7 }, inkDim { 0xffa9adb8 },
                        green { 0xff02EBBA }, yellow { 0xffFFD100 }, orange { 0xffFF9000 }, pink { 0xffFF1383 }, violet { 0xffA43DFF }, blue { 0xff6CC9F5 },
                        field { 0xff0a0a0c }, grid { 0xff2a2d36 }, dim { 0xff8d92a0 };
}

inline Rectangle<int> rc (const L::R& r) { return { r.x, r.y, r.w, r.h }; }
inline Font labelFont (float h, bool bold = false, float tracking = 0.04f)
{
    return Font (FontOptions (h).withName (Font::getDefaultSansSerifFontName()).withStyle (bold ? "Bold" : "Regular").withHorizontalScale (0.86f).withKerningFactor (tracking));
}
inline Font monoFont (float h) { return Font (FontOptions (h).withName (Font::getDefaultMonospacedFontName()).withStyle ("Regular")); }

inline Colour groupColour (const String& g)
{
    if (g == "Grains" || g == "Riser") return col::pink;
    if (g == "Effects" || g == "Filter") return col::violet;
    if (g == "Transients" || g == "Colour") return col::orange;
    if (g == "Time" || g == "Thicken") return col::blue;
    if (g == "Amp" || g == "Output") return col::yellow;
    return col::green;                                           // Pitch, Dance, Source
}

struct Shared { std::atomic<bool> tips { true }; };             // info bubbles on or off, shared by every control

//==============================================================================
// Rotary knob: drag up or down (Shift for fine), wheel, arrow keys, double click for the default, click the value to type one.
class Knob : public Component, public SettableTooltipClient
{
public:
    Knob (HairballProcessor& p, const L::KnobPos& d, Shared& s) : proc (p), pos (d), shared (s), index (hb::indexOf (d.id)), param (*p.apvts.getParameter (d.id)),
        attach (param, [this] (float) { repaint(); }), colour (groupColour (d.group))
    {
        setBounds (rc (d.r)); attach.sendInitialUpdate(); setWantsKeyboardFocus (true);
        const auto& pi = hb::kParams[index];
        setTooltip (String (pi.name) + "\n" + pi.tip + "\n" + hb::fmt::rangeText (index));
        addChildComponent (edit);
        edit.setFont (monoFont (12.0f)); edit.setJustification (Justification::centred); edit.setBorder ({ 0, 0, 0, 0 }); edit.setInputRestrictions (14);
        edit.onReturnKey = [this] { commit (true); }; edit.onEscapeKey = [this] { commit (false); }; edit.onFocusLost = [this] { commit (true); };
    }
    String getTooltip() override { return shared.tips.load() ? SettableTooltipClient::getTooltip() : String(); }

    float value() const { return param.convertFrom0to1 (param.getValue()); }

    void paint (Graphics& g) override
    {
        const auto& pi = hb::kParams[index];
        const float n = param.getValue(), cx = (float) getWidth() * 0.5f, cy = 14.0f + 25.0f;
        g.setColour (col::inkDim); g.setFont (labelFont (12.5f, false, 0.05f)); g.drawText (String (pi.name).toUpperCase(), 0, 0, getWidth(), 13, Justification::centred, false);
        const float a0 = degreesToRadians (-135.0f), a1 = degreesToRadians (135.0f);
        auto arc = [&] (float from, float to, Colour c)
        {
            if (std::abs (to - from) < 0.012f) return;
            Path p; p.addCentredArc (cx, cy, 21.0f, 21.0f, 0.0f, jmin (from, to), jmax (from, to), true);
            g.setColour (c); g.strokePath (p, PathStrokeType (4.0f, PathStrokeType::curved, PathStrokeType::rounded));
        };
        arc (a0, a1, Colour (0xff343741));
        const bool bip = pi.lo < 0 && pi.hi > 0;
        const float zeroN = bip ? param.convertTo0to1 (0.0f) : 0.0f, av = a0 + n * (a1 - a0);
        arc (a0 + zeroN * (a1 - a0), av, colour);
        g.setColour (Colour (0xff23252c)); g.fillEllipse (cx - 14.0f, cy - 14.0f, 28.0f, 28.0f);
        g.setColour (Colour (0xff3c3f4a)); g.drawEllipse (cx - 14.0f, cy - 14.0f, 28.0f, 28.0f, 1.0f);
        Path t; t.addRoundedRectangle (-1.2f, -12.0f, 2.4f, 12.0f, 1.2f);
        g.setColour (col::ink); g.fillPath (t, AffineTransform::rotation (av).translated (cx, cy));
        if (hasKeyboardFocus (false)) { g.setColour (col::blue); g.drawEllipse (cx - 27.0f, cy - 27.0f, 54.0f, 54.0f, 1.5f); }
        if (! edit.isVisible()) { g.setColour (col::ink); g.setFont (monoFont (12.0f)); g.drawText (hb::fmt::value (index, value()), 0, 66, getWidth(), 14, Justification::centred, false); }
    }
    void resized() override { edit.setBounds (4, 65, getWidth() - 8, 15); }

    void mouseDown (const MouseEvent& e) override
    {
        if (e.y > 64) return;
        grabKeyboardFocus(); dragN = param.getValue(); attach.beginGesture(); dragging = true;
    }
    void mouseDrag (const MouseEvent& e) override
    {
        if (! dragging) return;
        const float dy = (float) -e.getDistanceFromDragStartY();
        const float n = jlimit (0.0f, 1.0f, dragN + dy / (e.mods.isShiftDown() ? 900.0f : 190.0f));
        attach.setValueAsPartOfGesture (param.convertFrom0to1 (n));
    }
    void mouseUp (const MouseEvent& e) override
    {
        if (dragging) { attach.endGesture(); dragging = false; return; }
        if (e.y > 64 && e.getDistanceFromDragStart() < 4) beginEdit();
    }
    void mouseDoubleClick (const MouseEvent& e) override { if (e.y <= 64) attach.setValueAsCompleteGesture (param.convertFrom0to1 (param.getDefaultValue())); }
    void mouseWheelMove (const MouseEvent&, const MouseWheelDetails& w) override
    {
        if (w.deltaY == 0.0f) return;
        const float st = (w.deltaY > 0 ? 1.0f : -1.0f) * (w.isReversed ? -1.0f : 1.0f) * 0.01f;
        attach.setValueAsCompleteGesture (param.convertFrom0to1 (jlimit (0.0f, 1.0f, param.getValue() + st)));
    }
    bool keyPressed (const KeyPress& k) override
    {
        const float st = k.getModifiers().isShiftDown() ? 0.002f : 0.01f; float n = param.getValue();
        if (k.getKeyCode() == KeyPress::upKey || k.getKeyCode() == KeyPress::rightKey) n += st;
        else if (k.getKeyCode() == KeyPress::downKey || k.getKeyCode() == KeyPress::leftKey) n -= st;
        else if (k.getKeyCode() == KeyPress::homeKey) n = 0.0f; else if (k.getKeyCode() == KeyPress::endKey) n = 1.0f; else return false;
        attach.setValueAsCompleteGesture (param.convertFrom0to1 (jlimit (0.0f, 1.0f, n))); return true;
    }
private:
    void beginEdit() { edit.setText (hb::fmt::value (index, value()), false); edit.setVisible (true); edit.grabKeyboardFocus(); edit.selectAll(); repaint(); }
    void commit (bool apply)
    {
        if (! edit.isVisible()) return;
        const String s = edit.getText().trim().toLowerCase(); edit.setVisible (false);
        if (apply)
        {
            const auto& pi = hb::kParams[index];
            const String num = s.initialSectionContainingOnly ("+-.0123456789"), unit = s.substring (num.length()).trim();
            if (num.containsAnyOf ("0123456789"))
            {
                double v = num.getDoubleValue();
                if (unit.startsWith ("k")) v *= 1000.0; else if (unit == "s" && String (pi.unit) == "ms") v *= 1000.0;
                attach.setValueAsCompleteGesture (param.convertFrom0to1 (param.convertTo0to1 (jlimit (pi.lo, pi.hi, (float) v))));
            }
        }
        repaint();
    }
    HairballProcessor& proc; const L::KnobPos& pos; Shared& shared; int index; RangedAudioParameter& param; ParameterAttachment attach; Colour colour;
    TextEditor edit; float dragN = 0.0f; bool dragging = false;
};

//==============================================================================
// Row of choice buttons for an enumerated parameter (the mockup's .seg): one pink button is on.
class Choice : public Component, public SettableTooltipClient
{
public:
    Choice (HairballProcessor& p, const char* id, const L::R& label, const L::R& box, Shared& s, bool showLabel = true)
        : shared (s), index (hb::indexOf (id)), param (*p.apvts.getParameter (id)), attach (param, [this] (float) { repaint(); }), labelR (label), showLab (showLabel)
    {
        const auto& pi = hb::kParams[index];
        labels = StringArray::fromTokens (pi.choices, "|", "");
        auto b = rc (box); if (showLabel) b = b.getUnion (rc (label));
        setBounds (b); segTop = box.y - b.getY(); attach.sendInitialUpdate(); setMouseCursor (MouseCursor::PointingHandCursor);
        setTooltip (String (pi.name) + "\n" + pi.tip + "\n" + hb::fmt::rangeText (index));
        segH = box.h; segLeft = box.x - b.getX(); segW = box.w;
    }
    String getTooltip() override { return shared.tips.load() ? SettableTooltipClient::getTooltip() : String(); }
    int current() const { return roundToInt (param.convertFrom0to1 (param.getValue())); }
    void paint (Graphics& g) override
    {
        if (showLab) { g.setColour (col::inkDim); g.setFont (labelFont (13.0f, false, 0.08f)); g.drawText (String (hb::kParams[index].name).toUpperCase(), segLeft, 0, segW, segTop, Justification::centred, false); }
        const int n = labels.size(); const float bw = (float) segW / (float) n;
        for (int i = 0; i < n; ++i)
        {
            const auto r = Rectangle<float> ((float) segLeft + bw * (float) i, (float) segTop, bw, (float) segH);
            const bool on = i == current();
            Path p; p.addRoundedRectangle (r.getX(), r.getY(), r.getWidth(), r.getHeight(), 3.0f, 3.0f, i == 0, i == n - 1, i == 0, i == n - 1);
            g.setColour (on ? col::pink.withAlpha (0.13f) : col::panel2); g.fillPath (p);
            g.setColour (on ? col::pink : col::line); g.strokePath (p, PathStrokeType (1.0f));
            g.setColour (on ? Colours::white : col::ink); g.setFont (labelFont (14.0f, true, 0.08f));
            g.drawText (labels[i].toUpperCase(), r, Justification::centred, false);
        }
    }
    void mouseDown (const MouseEvent& e) override
    {
        const int n = labels.size(); const float bw = (float) segW / (float) n;
        const int i = (int) std::floor (((float) e.x - (float) segLeft) / bw);
        if (e.y >= segTop && i >= 0 && i < n) attach.setValueAsCompleteGesture ((float) i);
    }
private:
    Shared& shared; int index; RangedAudioParameter& param; ParameterAttachment attach; L::R labelR; bool showLab; StringArray labels; int segTop = 0, segH = 28, segLeft = 0, segW = 100;
};

//==============================================================================
// Plain push button in the mockup's button style. Optional "on" look (pink, teal or blue).
class Btn : public Component, public SettableTooltipClient
{
public:
    Btn (const String& t, Shared& s, const String& tip = {}) : text (t), shared (s) { setMouseCursor (MouseCursor::PointingHandCursor); setTooltip (tip); }
    String getTooltip() override { return shared.tips.load() ? SettableTooltipClient::getTooltip() : String(); }
    void paint (Graphics& g) override
    {
        const auto r = getLocalBounds().toFloat();
        Colour fill = over ? Colour (0xff23252c) : col::panel2, edge = over ? Colour (0xff4a4d58) : col::line, ink = col::ink;
        if (on) { fill = onColour.withAlpha (0.14f); edge = onColour; ink = Colours::white; }
        if (solid && on) { fill = onColour; ink = Colour (0xff04110e); }
        g.setColour (fill); g.fillRoundedRectangle (r, 3.0f);
        g.setColour (edge); g.drawRoundedRectangle (r.reduced (0.5f), 3.0f, 1.0f);
        g.setColour (isEnabled() ? ink : ink.withAlpha (0.35f)); g.setFont (labelFont (fontH, true, 0.08f));
        g.drawText (text.toUpperCase(), getLocalBounds().reduced (3, 0), Justification::centred, false);
    }
    void mouseEnter (const MouseEvent&) override { over = true; repaint(); }
    void mouseExit (const MouseEvent&) override { over = false; repaint(); }
    void mouseDown (const MouseEvent&) override { if (onDown) onDown(); }
    void mouseUp (const MouseEvent& e) override { if (onUp) onUp(); if (getLocalBounds().contains (e.getPosition()) && onClick) onClick(); }
    void setOn (bool b) { if (on != b) { on = b; repaint(); } }
    void setText (const String& t) { if (text != t) { text = t; repaint(); } }
    String text; Shared& shared; std::function<void()> onClick, onDown, onUp; bool on = false, over = false, solid = false; Colour onColour { col::pink }; float fontH = 14.0f;
};

//==============================================================================
// Preset name box: click for the list.
class NameBox : public Component, public SettableTooltipClient
{
public:
    NameBox (Shared& s) : shared (s) { setMouseCursor (MouseCursor::PointingHandCursor); setTooltip ("Presets\nClick to pick a sound. Factory sounds are our own starting points; your saved sounds are listed too."); }
    String getTooltip() override { return shared.tips.load() ? SettableTooltipClient::getTooltip() : String(); }
    void paint (Graphics& g) override
    {
        const auto r = getLocalBounds().toFloat();
        g.setColour (col::field); g.fillRoundedRectangle (r, 3.0f); g.setColour (col::line); g.drawRoundedRectangle (r.reduced (0.5f), 3.0f, 1.0f);
        g.setColour (col::ink); g.setFont (monoFont (14.0f)); g.drawText (name, 12, 0, getWidth() - 110, getHeight(), Justification::centredLeft, true);
        g.setColour (col::green); g.setFont (monoFont (12.0f)); g.drawText (category.toUpperCase(), getWidth() - 100, 0, 88, getHeight(), Justification::centredRight, false);
    }
    void mouseUp (const MouseEvent& e) override { if (getLocalBounds().contains (e.getPosition()) && onClick) onClick(); }
    String name { "Init" }, category { "Init" }; Shared& shared; std::function<void()> onClick;
};

//==============================================================================
// Small graphs. All read the host parameters directly so they follow automation.
class Gfx : public Component
{
public:
    Gfx (HairballProcessor& p, const L::GfxPos& d) : proc (p) { setBounds (rc (d.r)); setInterceptsMouseClicks (false, false); }
    float v (int i) const { return proc.apvts.getRawParameterValue (hb::kParams[i].id)->load(); }
    void bg (Graphics& g) { g.setColour (col::field); g.fillRect (getLocalBounds()); g.setColour (col::line); g.drawRoundedRectangle (getLocalBounds().toFloat().reduced (0.5f), 3.0f, 1.0f); }
    static void dashed (Graphics& g, const Path& p, float a, float b, float thick) { Path d; const float pat[2] = { a, b }; PathStrokeType (thick).createDashedStroke (d, p, pat, 2); g.fillPath (d); }
    HairballProcessor& proc;
};

class GfxRise : public Gfx
{
public:
    using Gfx::Gfx;
    void paint (Graphics& g) override
    {
        bg (g); const float w = (float) getWidth(), h = (float) getHeight(), Lm = 34, B = 16, T = 8, R = 8, pw = w - Lm - R, ph = h - T - B;
        const float curve = v (hb::pRiseCurve), tone = v (hb::pRiseTone) * 0.01f; const bool on = v (hb::pRiseOn) >= 0.5f;
        g.setColour (col::grid); g.drawRect (Lm, T, pw, ph, 1.0f);
        g.setColour (col::dim); g.setFont (monoFont (10.0f));
        g.drawText ("+" + String (v (hb::pRisePitch), 0) + "st", 0, (int) T, (int) Lm - 3, 12, Justification::right, false); g.drawText ("0", 0, (int) (T + ph) - 10, (int) Lm - 3, 12, Justification::right, false);
        g.drawText ("0 s", (int) Lm, (int) h - 13, 40, 12, Justification::left, false); g.drawText (String (v (hb::pRiseTime), 1) + " s", (int) (w - R) - 60, (int) h - 13, 60, 12, Justification::right, false);
        Path p; for (int i = 0; i <= 80; ++i) { const float t = (float) i / 80.0f, u = std::pow (t, curve), x = Lm + t * pw, y = T + ph - u * ph; if (i) p.lineTo (x, y); else p.startNewSubPath (x, y); }
        g.setColour (on ? col::pink : Colour (0xff6a2146)); g.strokePath (p, PathStrokeType (2.0f));
        Path q; for (int i = 0; i <= 80; ++i) { const float t = (float) i / 80.0f, u = std::pow (t, curve) * tone, x = Lm + t * pw, y = T + ph - u * ph * 0.5f; if (i) q.lineTo (x, y); else q.startNewSubPath (x, y); }
        g.setColour (col::green); dashed (g, q, 3.0f, 3.0f, 1.0f);
        g.drawText ("grains fuse to tone", (int) Lm + 6, (int) (T + ph) - 14, 140, 12, Justification::left, false);
        const float u = proc.live.riser.load (std::memory_order_relaxed);
        if (u > 0.001f) { const float t = std::pow (u, 1.0f / curve); g.setColour (Colours::white); g.fillEllipse (Lm + t * pw - 4.0f, T + ph - u * ph - 4.0f, 8.0f, 8.0f); }
    }
};

class GfxPenv : public Gfx
{
public:
    using Gfx::Gfx;
    void paint (Graphics& g) override
    {
        bg (g); const float w = (float) getWidth(), h = (float) getHeight(), Lm = 34, B = 16, T = 8, R = 8, pw = w - Lm - R, ph = h - T - B;
        const float amt = v (hb::pPitchEnvAmt), tt = v (hb::pPitchEnvTime), cv = v (hb::pPitchEnvCurve), rng = jmax (12.0f, std::abs (amt)), zy = T + ph / 2;
        g.setColour (col::grid); g.drawRect (Lm, T, pw, ph, 1.0f); g.drawHorizontalLine ((int) zy, Lm, Lm + pw);
        g.setColour (col::dim); g.setFont (monoFont (10.0f));
        g.drawText ("+" + String (rng, 0), 0, (int) T, (int) Lm - 3, 12, Justification::right, false); g.drawText ("-" + String (rng, 0), 0, (int) (T + ph) - 10, (int) Lm - 3, 12, Justification::right, false);
        g.drawText ("0 st", 0, (int) zy - 6, (int) Lm - 3, 12, Justification::right, false);
        g.drawText ("0", (int) Lm, (int) h - 13, 20, 12, Justification::left, false); g.drawText (String (tt * 1.25f / 1000.0f, 2) + " s", (int) (w - R) - 60, (int) h - 13, 60, 12, Justification::right, false);
        Path p; const float tmax = tt * 1.25f;
        for (int i = 0; i <= 80; ++i) { const float t = (float) i / 80.0f * tmax, val = amt * std::pow (jmax (0.0f, 1.0f - jmin (1.0f, t / tt)), cv), x = Lm + (float) i / 80.0f * pw, y = zy - val / rng * ph / 2; if (i) p.lineTo (x, y); else p.startNewSubPath (x, y); }
        g.setColour (col::green); g.strokePath (p, PathStrokeType (2.0f));
    }
};

class GfxResp : public Gfx
{
public:
    using Gfx::Gfx;
    void paint (Graphics& g) override
    {
        bg (g); const float w = (float) getWidth(), h = (float) getHeight(), Lm = 30, B = 14, T = 6, R = 6, pw = w - Lm - R, ph = h - T - B, sr = (float) proc.sampleRateNow();
        hb::Biquad lo, mid, hi; lo.lowShelf (sr, 120, v (hb::pLowG)); mid.peak (sr, v (hb::pMidF), v (hb::pMidG), v (hb::pMidQ)); hi.highShelf (sr, 8000, v (hb::pHighG));
        auto mag = [&] (const hb::Biquad& b, float f)
        {
            const float w0 = 2 * hb::kPi * f / sr, c1 = std::cos (w0), s1 = std::sin (w0), c2 = std::cos (2 * w0), s2 = std::sin (2 * w0);
            const float nr = b.b0 + b.b1 * c1 + b.b2 * c2, ni = -(b.b1 * s1 + b.b2 * s2), dr = 1 + b.a1 * c1 + b.a2 * c2, di = -(b.a1 * s1 + b.a2 * s2);
            return std::hypot (nr, ni) / std::hypot (dr, di);
        };
        const int ft = roundToInt (v (hb::pFiltType)); const float reso = v (hb::pReso);
        auto filt = [&] (float f, float fc)
        {
            if (! ft) return 1.0f;
            const float s = f / fc, k = 1.0f / reso, dn = std::hypot (1 - s * s, k * s);
            return ft == 1 ? 1.0f / dn : (ft == 2 ? s * s / dn : 2.0f * s / dn);
        };
        auto yDb = [&] (float db) { return T + ph / 2 - jlimit (-24.0f, 24.0f, db) / 24.0f * ph / 2; };
        g.setFont (monoFont (10.0f));
        for (int db : { -12, 0, 12 }) { g.setColour (col::grid); g.drawHorizontalLine ((int) yDb ((float) db), Lm, Lm + pw); g.setColour (col::dim); g.drawText ((db > 0 ? "+" : "") + String (db), 0, (int) yDb ((float) db) - 6, (int) Lm - 3, 12, Justification::right, false); }
        auto fx = [&] (float f) { return Lm + std::log (f / 20.0f) / std::log (1000.0f) * pw; };
        for (float f : { 100.0f, 1000.0f, 10000.0f }) { g.setColour (col::grid); g.drawVerticalLine ((int) fx (f), T, T + ph); g.setColour (col::dim); g.drawText (f >= 1000 ? String (f / 1000, 0) + "k" : String ((int) f), (int) fx (f) - 15, (int) h - 13, 30, 12, Justification::centred, false); }
        auto curve = [&] (float fcut, Colour c, bool dash)
        {
            Path p; bool first = true;
            for (int i = 0; i <= (int) pw; i += 2)
            {
                const float f = 20.0f * std::pow (1000.0f, (float) i / pw), m = mag (lo, f) * mag (mid, f) * mag (hi, f) * filt (f, fcut), db = 20.0f * std::log10 (jmax (1.0e-5f, m)), y = yDb (db);
                if (first) { p.startNewSubPath (Lm + (float) i, y); first = false; } else p.lineTo (Lm + (float) i, y);
            }
            g.setColour (c); if (dash) dashed (g, p, 3.0f, 3.0f, 1.0f); else g.strokePath (p, PathStrokeType (2.0f));
        };
        const float cut = v (hb::pCutoff), rf = v (hb::pRiseFilter), prog = proc.live.riser.load (std::memory_order_relaxed);
        if (v (hb::pRiseOn) >= 0.5f && rf > 0 && ft) curve (jlimit (20.0f, 20000.0f, cut * std::pow (2.0f, rf)), col::pink.withAlpha (0.53f), true);
        curve (cut, col::blue, false);
        if (prog > 0 && ft && v (hb::pRiseOn) >= 0.5f) curve (jlimit (20.0f, 20000.0f, cut * std::pow (2.0f, prog * rf)), Colours::white.withAlpha (0.33f), true);
    }
};

class GfxGshape : public Gfx
{
public:
    using Gfx::Gfx;
    void paint (Graphics& g) override
    {
        bg (g); const float w = (float) getWidth(), h = (float) getHeight();
        const float att = jmax (0.8f, v (hb::pAttack)), hold = v (hb::pGrainLen), nominal = hold + att, hop = nominal * v (hb::pSpacing) * 0.01f, len = hold + 2 * att, span = 3 * hop + len;
        const float pad = 8, pw = w - 2 * pad, base = h - 16, top = 8; auto X = [&] (float ms) { return pad + ms / span * pw; };
        g.setColour (col::grid); g.drawHorizontalLine ((int) base, pad, w - pad);
        const Colour cols[2] = { col::pink, col::violet };
        for (int k = 0; k < 4; ++k)
        {
            const float x0 = (float) k * hop; Path p; p.startNewSubPath (X (x0), base);
            for (int i = 0; i <= 24; ++i) { const float t = (float) i / 24 * att; p.lineTo (X (x0 + t), base - (0.5f - 0.5f * std::cos (hb::kPi * t / att)) * (base - top)); }
            p.lineTo (X (x0 + att + hold), top);
            for (int i = 0; i <= 24; ++i) { const float t = (float) i / 24 * att; p.lineTo (X (x0 + att + hold + t), base - (0.5f + 0.5f * std::cos (hb::kPi * t / att)) * (base - top)); }
            p.closeSubPath(); g.setColour (cols[k % 2].withAlpha (0.27f)); g.fillPath (p); g.setColour (cols[k % 2]); g.strokePath (p, PathStrokeType (1.5f));
        }
        g.setColour (col::dim); g.setFont (monoFont (10.0f));
        g.drawText ("hop " + String (hop, 0) + " ms", (int) pad, (int) h - 13, 100, 12, Justification::left, false); g.drawText ("overlap " + String (len / hop, 1) + "x", (int) (w - pad) - 100, (int) h - 13, 100, 12, Justification::right, false);
    }
};

class GfxLfo : public Gfx
{
public:
    using Gfx::Gfx;
    void paint (Graphics& g) override
    {
        bg (g); const float w = (float) getWidth(), h = (float) getHeight(), Lm = 30, pw = w - Lm - 6, T = 6, B = 14, ph = h - T - B;
        auto y = [&] (float val) { return T + ph / 2 - val / 300.0f * ph / 2; };
        g.setFont (monoFont (10.0f));
        for (int val : { 300, 100, 0, -300 }) { g.setColour (col::grid); g.drawHorizontalLine ((int) y ((float) val), Lm, Lm + pw); g.setColour (col::dim); g.drawText (String (val), 0, (int) y ((float) val) - 6, (int) Lm - 3, 12, Justification::right, false); }
        Path p; const float ws = v (hb::pWaveSpacing), dp = v (hb::pLfoDepth), sp = v (hb::pLfoSpeed);
        for (int i = 0; i <= (int) pw; ++i) { const float t = (float) i / pw, val = jlimit (-300.0f, 300.0f, ws + dp * std::sin (2 * hb::kPi * sp * t)); if (i) p.lineTo (Lm + (float) i, y (val)); else p.startNewSubPath (Lm, y (val)); }
        g.setColour (col::violet); g.strokePath (p, PathStrokeType (2.0f));
        g.setColour (col::dim); g.drawText ("1 s", (int) (w - 6) - 40, (int) h - 13, 40, 12, Justification::right, false); g.drawText ("wave spacing %", (int) Lm, (int) h - 13, 110, 12, Justification::left, false);
    }
};

class GfxXfer : public Gfx
{
public:
    using Gfx::Gfx;
    void paint (Graphics& g) override
    {
        bg (g); const float w = (float) getWidth(), h = (float) getHeight();
        g.setColour (col::grid); g.drawVerticalLine ((int) (w / 2), 0, h); g.drawHorizontalLine ((int) (h / 2), 0, w);
        const float cl = v (hb::pNasty) >= 0.5f ? 2.0f : 1.0f, d = jmin (v (hb::pDrive) * 0.01f, cl), x = jmin (v (hb::pTransistor) * 0.01f, cl), f = jmin (v (hb::pFuzz) * 0.01f, cl);
        const float key = d * 1000.0f + x * 100.0f + f; if (key != lastKey) { lastKey = key; mk = hb::Colour::makeup (d, x, f); }
        Path diag; diag.startNewSubPath (0, h); diag.lineTo (w, 0); g.setColour (Colour (0xff55596a)); dashed (g, diag, 3.0f, 3.0f, 1.0f);
        Path p; for (int i = 0; i <= (int) w; ++i) { const float xi = (float) i / w * 2 - 1, y = hb::Colour::shaper (xi, d, x, f) * mk, py = h / 2 - jlimit (-1.2f, 1.2f, y) / 1.2f * h / 2; if (i) p.lineTo ((float) i, py); else p.startNewSubPath (0, py); }
        g.setColour (col::orange); g.strokePath (p, PathStrokeType (2.0f));
        g.setColour (col::dim); g.setFont (monoFont (10.0f)); g.drawText ("out", 3, 1, 40, 12, Justification::left, false); g.drawText ("in", (int) w - 43, (int) (h / 2) - 14, 40, 12, Justification::right, false);
    }
    float lastKey = -1, mk = 1;
};

class GfxGr : public Gfx
{
public:
    using Gfx::Gfx;
    void paint (Graphics& g) override
    {
        bg (g); const float w = (float) getWidth(), h = (float) getHeight(), Lm = 32, T = 6, B = 16, ph = h - T - B;
        smooth += (proc.live.grDb.load (std::memory_order_relaxed) - smooth) * 0.35f;
        g.setFont (monoFont (10.0f));
        for (int db : { 0, 3, 6, 9, 12 }) { const float y = T + (float) db / 12 * ph; g.setColour (col::dim); g.drawText ("-" + String (db), 0, (int) y - 6, (int) Lm - 4, 12, Justification::right, false); g.setColour (col::grid); g.drawHorizontalLine ((int) y, Lm, w - 6); }
        g.setColour (col::green); g.fillRect (Lm + 8, T, w - Lm - 22, smooth / 12 * ph);
        g.setColour (col::dim); g.drawText ("GR dB", 4, (int) h - 13, 60, 12, Justification::left, false);
    }
    float smooth = 0;
};

//==============================================================================
// Output meter: translucent bar is the peak, solid inner bar the level (RMS), a line holds the peak for 1.5 s.
// Neon blue is safe, purple is a warning, pink is clipping. Scale in dBFS.
class MeterView : public Component
{
public:
    MeterView (HairballProcessor& p, const L::R& r) : proc (p) { setBounds (rc (r)); setInterceptsMouseClicks (false, false); }
    void tick()
    {
        const float pl = proc.live.peakL.exchange (0.0f), pr = proc.live.peakR.exchange (0.0f);
        peakL = jmax (pl, peakL * 0.9f); peakR = jmax (pr, peakR * 0.9f);
        rmsL += (proc.live.rmsL.load() - rmsL) * 0.5f; rmsR += (proc.live.rmsR.load() - rmsR) * 0.5f;
        const double now = Time::getMillisecondCounterHiRes();
        if (pl > holdL || now > holdT) { holdL = pl; holdR = pr; holdT = now + 1500.0; } else { holdL = jmax (holdL, pl); holdR = jmax (holdR, pr); }
        repaint();
    }
    void paint (Graphics& g) override
    {
        const float h = (float) getHeight(); const float bw = 14, x1 = 30, x2 = 50;
        auto dbY = [h] (float db) { return 3.0f + (1.0f - (jlimit (-60.0f, 0.0f, db) + 60.0f) / 60.0f) * (h - 20.0f); };
        auto toDb = [] (float v) { return 20.0f * std::log10 (jmax (1.0e-6f, v)); };
        g.setColour (col::field); g.fillRect (x1, 2.0f, bw, h - 17.0f); g.fillRect (x2, 2.0f, bw, h - 17.0f);
        g.setFont (monoFont (10.0f));
        for (int db : { 0, -6, -12, -24, -36, -48, -60 }) { const float y = dbY ((float) db); g.setColour (col::dim); g.drawText (String (db), 0, (int) y - 6, 26, 12, Justification::right, false); g.setColour (col::grid); g.drawHorizontalLine ((int) y, 28.0f, x2 + bw + 3); }
        ColourGradient grad (Colour (0xff00d4ff), 0, dbY (-60), Colour (0xffff1383), 0, dbY (0), false);
        grad.addColour (0.70, Colour (0xff1ea7ff)); grad.addColour (0.74, Colour (0xffa43dff)); grad.addColour (0.91, Colour (0xffb43dff)); grad.addColour (0.94, Colour (0xffff1383));
        auto bar = [&] (float x, float pk, float rm, float hold)
        {
            const float yP = dbY (toDb (pk)), yR = dbY (toDb (rm)), bottom = h - 15.0f;
            g.setGradientFill (grad); g.setOpacity (0.45f); g.fillRect (x, yP, bw, bottom - yP);
            g.setGradientFill (grad); g.setOpacity (1.0f); g.fillRect (x + 3.0f, yR, bw - 6.0f, bottom - yR);
            g.setColour (hold >= 0.98f ? col::pink : Colours::white); g.fillRect (x, dbY (toDb (hold)), bw, 2.0f);
        };
        bar (x1, peakL, rmsL, holdL); bar (x2, peakR, rmsR, holdR);
        g.setColour (col::dim); g.drawText ("dBFS", 20, (int) h - 13, 40, 12, Justification::centred, false);
    }
private:
    HairballProcessor& proc; float peakL = 0, peakR = 0, rmsL = 0, rmsR = 0, holdL = 0, holdR = 0; double holdT = 0;
};

//==============================================================================
// Main display: the loaded sound (Instrument) or the capture buffer (FX), playing position, start marker, hit marks and grains.
class MainDisplay : public Component, public SettableTooltipClient
{
public:
    MainDisplay (HairballProcessor& p, const L::R& r) : proc (p) { setBounds (rc (r)); setTooltip ("Display\nInstrument: the loaded sound. Click to set where playing starts. Drop an audio file here.\nFX: the audio the track sent, newest on the right."); }
    bool hot = false;
    void paint (Graphics& g) override
    {
        const float w = (float) getWidth(), h = (float) getHeight();
        g.setColour (col::field); g.fillRect (getLocalBounds());
        g.setColour (col::grid); g.drawHorizontalLine ((int) (h / 2), 0, w);
        const bool fx = v (hb::pMode) >= 0.5f; bool drew = false; double seconds = 0; bool frozen = proc.live.frozen.load (std::memory_order_relaxed);
        if (fx)
        {
            const SpinLock::ScopedTryLockType tl (proc.displayLock);
            const auto& eng = proc.engineForDisplay(); const auto& buf = eng.liveBuf();
            const size_t N = eng.ringLength(), written = eng.liveWritten();
            if (tl.isLocked() && N > 16 && buf.l.size() >= N && written > 0)
            {
                const size_t have = std::min (written, N), wr = written % N; const int cols = (int) w; seconds = (double) N / proc.sampleRateNow();
                g.setColour (frozen ? col::blue : col::green); g.setOpacity (0.9f);
                for (int x = 0; x < cols; ++x)
                {
                    const size_t a = (size_t) x * N / (size_t) cols, b = (size_t) (x + 1) * N / (size_t) cols; float mn = 0, mx = 0; const size_t step = std::max<size_t> (1, (b - a) / 24);
                    for (size_t i = a; i < b; i += step) { if (i < N - have) continue; const float s = buf.l[(wr + i) % N]; mn = jmin (mn, s); mx = jmax (mx, s); }
                    g.fillRect ((float) x, h / 2 - mx * h * 0.46f, 1.0f, jmax (1.0f, (mx - mn) * h * 0.46f));
                }
                drew = true;
            }
        }
        else if (auto pk = proc.peaks())
        {
            const int cols = (int) w; seconds = pk->seconds; g.setColour (col::green); g.setOpacity (0.9f);
            for (int x = 0; x < cols; ++x)
            {
                const size_t c = (size_t) x * pk->lo.size() / (size_t) cols;
                g.fillRect ((float) x, h / 2 - pk->hi[c] * h * 0.46f, 1.0f, jmax (1.0f, (pk->hi[c] - pk->lo[c]) * h * 0.46f));
            }
            g.setOpacity (1.0f);
            const float sx = v (hb::pStart) * 0.01f * w; Path st; st.startNewSubPath (sx, 0); st.lineTo (sx, h); g.setColour (col::green); Gfx::dashed (g, st, 4.0f, 3.0f, 1.0f);
            if (v (hb::pTransMode) > 0.5f) { g.setColour (col::orange); for (float m : pk->marks) g.drawVerticalLine ((int) (m * w), 0.0f, 14.0f); }
            drew = true;
        }
        g.setOpacity (1.0f);
        if (drew)
        {
            if (proc.showGrains.load())
            {
                g.setColour (Colours::white); const int nm = proc.live.nMarks.load (std::memory_order_relaxed);
                for (int i = 0; i < nm; ++i) { g.setOpacity (0.7f); g.fillRect (proc.live.marks[i].load (std::memory_order_relaxed) * w - 1.0f, h / 2 - 9.0f, 2.0f, 18.0f); }
                g.setOpacity (1.0f);
            }
            g.setColour (col::pink); const int nh = proc.live.nHeads.load (std::memory_order_relaxed);
            for (int i = 0; i < nh; ++i) g.fillRect (proc.live.heads[i].load (std::memory_order_relaxed) * w - 1.0f, 0.0f, 2.0f, h);
            g.setColour (col::dim); g.setFont (monoFont (11.0f));
            g.drawText (fx ? String (seconds, 1) + " s buffer" + (frozen ? ", frozen" : "") : String (seconds, 2) + " s", 0, (int) h - 20, (int) w - 8, 14, Justification::right, false);
        }
        else
        {
            g.setColour (col::dim); g.setFont (Font (FontOptions (14.0f)));
            const String miss = proc.missingSample();
            g.drawText (fx ? "Waiting for audio" : (miss.isNotEmpty() ? "Sound file not found: " + miss : "No sound loaded"), getLocalBounds(), Justification::centred, false);
        }
        g.setColour (hot ? col::pink : col::dim); g.setFont (labelFont (12.0f, false, 0.1f));
        g.drawText (fx ? "FX: the track plays through Hairball" : "Drop an audio file here", 8, (int) h - 22, 300, 16, Justification::left, false);
        g.setColour (col::dim); g.setFont (labelFont (11.0f, false, 0.1f));
        g.drawFittedText (fx ? "Right edge is now\npink line: where grains read" : "Pink line: playing position\nGreen: start, orange: hits", Rectangle<int> ((int) w - 220, 4, 212, 30), Justification::topRight, 2);
        if (hot) { g.setColour (col::pink); Path o; o.addRectangle (Rectangle<float> (3, 3, w - 6, h - 6)); Gfx::dashed (g, o, 6.0f, 4.0f, 2.0f); }
    }
    void mouseDown (const MouseEvent& e) override
    {
        if (v (hb::pMode) >= 0.5f) return;
        if (auto* p = proc.apvts.getParameter ("start")) p->setValueNotifyingHost (p->convertTo0to1 (jlimit (0.0f, 99.0f, (float) e.x / (float) getWidth() * 100.0f)));
    }
    float v (int i) const { return proc.apvts.getRawParameterValue (hb::kParams[i].id)->load(); }
private:
    HairballProcessor& proc;
};

//==============================================================================
// Piano: 36 keys from the octave base, computer keys Z S X D C V G B H N J M and Q 2 W 3 E R 5 T 6 Y 7 U I. Green marks show the keys
// above C5 when "Key to" is not Pitch.
class Piano : public Component
{
public:
    Piano (HairballProcessor& p, const L::R& r) : proc (p), state (p.keyState) { setBounds (rc (r)); setWantsKeyboardFocus (true); }
    ~Piano() override { releaseAll(); }
    int base = 60;
    static constexpr int N = 36;
    static bool isBlack (int n) { const int m = n % 12; return m == 1 || m == 3 || m == 6 || m == 8 || m == 10; }
    Rectangle<float> keyRect (int n) const
    {
        int whites = 0; for (int i = 0; i < N; ++i) if (! isBlack (base + i)) ++whites;
        const float ww = (float) getWidth() / (float) whites; int wi = 0; for (int i = 0; i < n - base; ++i) if (! isBlack (base + i)) ++wi;
        if (isBlack (n)) return { (float) wi * ww - ww * 0.3f, 0.0f, ww * 0.6f, (float) getHeight() * 0.7f };
        return { (float) wi * ww, 0.0f, ww, (float) getHeight() };
    }
    void paint (Graphics& g) override
    {
        g.fillAll (Colours::black); const bool km = proc.apvts.getRawParameterValue ("keyTo")->load() >= 0.5f;
        for (int pass = 0; pass < 2; ++pass)
            for (int i = 0; i < N; ++i)
            {
                const int n = base + i; if (isBlack (n) != (pass == 1)) continue;
                const auto r = keyRect (n); const bool down = held.count (n) > 0 || state.isNoteOn (1, n);
                if (pass == 0)
                {
                    g.setGradientFill (ColourGradient (Colour (0xffe9e9ee), 0, 0, Colour (0xfff9f9fb), 0, r.getBottom(), false)); if (down) g.setColour (col::pink);
                    g.fillRect (r.withTrimmedRight (1.0f)); g.setColour (Colours::black); g.drawVerticalLine ((int) r.getRight() - 1, 0.0f, r.getBottom());
                    if (down) { g.setColour (col::pink); g.fillRect (r.withTrimmedRight (1.0f)); }
                    if (n % 12 == 0) { g.setColour (down ? Colours::white : Colour (0xff222222)); g.setFont (monoFont (10.0f)); g.drawText (hb::fmt::noteName (n), r.toNearestInt().withTrimmedBottom (3), Justification::centredBottom, false); }
                    if (km && n >= 72) { g.setColour (col::green); g.fillRoundedRectangle (r.getX() + r.getWidth() * 0.2f, r.getBottom() - 16.0f, r.getWidth() * 0.6f, 3.0f, 1.5f); }
                }
                else
                {
                    g.setGradientFill (ColourGradient (down ? Colour (0xffc40d66) : Colour (0xff35373f), 0, 0, down ? Colour (0xffc40d66) : Colour (0xff17181c), 0, r.getBottom(), false));
                    g.fillRect (r); g.setColour (Colours::black); g.drawRect (r, 1.0f);
                    if (km && n >= 72) { g.setColour (col::green); g.fillRoundedRectangle (r.getX() + r.getWidth() * 0.2f, r.getBottom() - 8.0f, r.getWidth() * 0.6f, 3.0f, 1.5f); }
                }
            }
        g.setColour (col::line); g.drawRect (getLocalBounds(), 1);
    }
    int keyAt (Point<float> p) const
    {
        for (int i = 0; i < N; ++i) if (isBlack (base + i) && keyRect (base + i).contains (p)) return base + i;
        for (int i = 0; i < N; ++i) if (! isBlack (base + i) && keyRect (base + i).contains (p)) return base + i;
        return -1;
    }
    void mouseDown (const MouseEvent& e) override { grabKeyboardFocus(); press (keyAt (e.position)); }
    void mouseDrag (const MouseEvent& e) override { const int k = keyAt (e.position); if (k != mouseKey) { release (mouseKey); press (k); } }
    void mouseUp (const MouseEvent&) override { release (mouseKey); mouseKey = -1; }
    void mouseEnter (const MouseEvent&) override { grabKeyboardFocus(); }
    bool keyPressed (const KeyPress&) override { return false; }
    bool keyStateChanged (bool) override
    {
        bool any = false;
        for (auto& kv : qwerty)
        {
            const bool d = KeyPress::isKeyCurrentlyDown (kv.first); const int n = base + kv.second;
            if (d && ! keysDown.count (kv.first)) { keysDown.insert (kv.first); state.noteOn (1, n, 0.85f); held.insert (n); any = true; }
            else if (! d && keysDown.count (kv.first)) { keysDown.erase (kv.first); state.noteOff (1, n, 0.0f); held.erase (n); any = true; }
        }
        return any;
    }
    void setBase (int b) { releaseAll(); base = jlimit (24, 84, b); repaint(); }
    void releaseAll() { for (int n : std::set<int> (held)) state.noteOff (1, n, 0.0f); held.clear(); keysDown.clear(); if (mouseKey >= 0) { mouseKey = -1; } }
private:
    void press (int n) { if (n < 0) return; mouseKey = n; held.insert (n); state.noteOn (1, n, 0.85f); repaint(); }
    void release (int n) { if (n < 0) return; held.erase (n); state.noteOff (1, n, 0.0f); repaint(); }
    HairballProcessor& proc; MidiKeyboardState& state; std::set<int> held; std::set<int> keysDown; int mouseKey = -1;
    inline static const std::map<int, int> qwerty { { 'Z', 0 }, { 'S', 1 }, { 'X', 2 }, { 'D', 3 }, { 'C', 4 }, { 'V', 5 }, { 'G', 6 }, { 'B', 7 }, { 'H', 8 }, { 'N', 9 }, { 'J', 10 }, { 'M', 11 },
                                                    { 'Q', 12 }, { '2', 13 }, { 'W', 14 }, { '3', 15 }, { 'E', 16 }, { 'R', 17 }, { '5', 18 }, { 'T', 19 }, { '6', 20 }, { 'Y', 21 }, { '7', 22 }, { 'U', 23 }, { 'I', 24 } };
};

} // namespace hbui
