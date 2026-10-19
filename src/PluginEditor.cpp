#include "PluginEditor.h"
#include "Presets.h"
#include <catui/CatLookAndFeel.h>
#include <catui/Logo.h>

using namespace hbui;

#ifndef JucePlugin_VersionString
 #define JucePlugin_VersionString "0.0.0"
#endif

//==============================================================================
// Look: dark menus, dialogs and info bubbles that match the panel.
struct HairballEditor::Look : public juce::LookAndFeel_V4
{
    Look()
    {
        setColour (juce::PopupMenu::backgroundColourId, juce::Colour (0xff0d0e11));
        setColour (juce::PopupMenu::textColourId, col::ink);
        setColour (juce::PopupMenu::highlightedBackgroundColourId, col::panel2);
        setColour (juce::PopupMenu::highlightedTextColourId, col::green);
        setColour (juce::PopupMenu::headerTextColourId, col::inkDim);
        setColour (juce::AlertWindow::backgroundColourId, col::panel);
        setColour (juce::AlertWindow::textColourId, col::ink);
        setColour (juce::AlertWindow::outlineColourId, col::line);
        setColour (juce::TextButton::buttonColourId, col::panel2);
        setColour (juce::TextButton::textColourOffId, col::ink);
        setColour (juce::TextEditor::backgroundColourId, col::field);
        setColour (juce::TextEditor::textColourId, col::ink);
        setColour (juce::TextEditor::outlineColourId, col::line);
        setColour (juce::TextEditor::focusedOutlineColourId, col::green);
        setColour (juce::TooltipWindow::backgroundColourId, juce::Colour (0xff0d0e11));
        setColour (juce::TooltipWindow::textColourId, col::ink);
        setColour (juce::TooltipWindow::outlineColourId, col::blue);
    }
    juce::Font getPopupMenuFont() override { return monoFont (14.0f); }
    juce::Rectangle<int> getTooltipBounds (const juce::String& text, juce::Point<int> pos, juce::Rectangle<int> area) override
    {
        const auto tl = layoutFor (text, 270.0f);
        const int w = (int) tl.getWidth() + 20, h = (int) tl.getHeight() + 14;
        return juce::Rectangle<int> (pos.x > area.getCentreX() ? pos.x - w - 12 : pos.x + 14, pos.y + 18, w, h).constrainedWithin (area);
    }
    void drawTooltip (juce::Graphics& g, const juce::String& text, int w, int h) override
    {
        g.setColour (juce::Colour (0xff0d0e11)); g.fillRoundedRectangle (0, 0, (float) w, (float) h, 4.0f);
        g.setColour (col::blue); g.drawRoundedRectangle (0.5f, 0.5f, (float) w - 1.0f, (float) h - 1.0f, 4.0f, 1.0f);
        layoutFor (text, 270.0f).draw (g, juce::Rectangle<float> (10.0f, 7.0f, (float) w - 20.0f, (float) h - 14.0f));
    }
    // first line is the name, then the explanation, last line (when there are three) the range
    static juce::TextLayout layoutFor (const juce::String& text, float maxW)
    {
        juce::AttributedString as; as.setJustification (juce::Justification::topLeft);
        const auto parts = juce::StringArray::fromLines (text);
        as.append (parts[0].toUpperCase() + (parts.size() > 1 ? "\n" : ""), labelFont (13.0f, true, 0.14f), col::blue);
        if (parts.size() > 1) as.append (parts[1] + (parts.size() > 2 ? "\n" : ""), juce::Font (juce::FontOptions (13.0f)), col::ink);
        if (parts.size() > 2) as.append (parts[2], monoFont (11.0f), col::inkDim);
        juce::TextLayout tl; tl.createLayout (as, maxW); return tl;
    }
};

//==============================================================================
struct HairballEditor::Content : public juce::Component
{
    Content (HairballEditor& o) : owner (o), proc (o.proc),
        prev ("<", o.shared, "Previous preset"), next (">", o.shared, "Next preset"),
        save ("Save", o.shared, "Save the current sound. Overwrite your own preset or save as a new one."),
        rename ("Rename", o.shared, "Rename your preset."), del ("Delete", o.shared, "Delete your preset. Factory presets cannot be deleted."),
        exp ("Export", o.shared, "Show the preset as text you can copy."), imp ("Import", o.shared, "Paste preset text. It is checked before it is used."),
        guide ("Guide", o.shared), settings ("Settings", o.shared), about ("About & Support", o.shared),
        zoom ("100%", o.shared, "Window size, percent of the default. Drag any edge or corner to resize; double click the wood to go back to 100%."),
        load ("Load audio", o.shared, "Instrument: pick an audio file to play with the keys. You can also drop a file on the display. WAV, AIFF, FLAC and OGG work everywhere; only the first 60 seconds are used."),
        freeze ("Freeze", o.shared, "FX: stop recording and loop what is in the buffer. Grains then play only the frozen audio."),
        hold ("Hold", o.shared, "Holds the playing position still. Click to latch, or press and hold for a stutter."),
        play ("Play", o.shared, "Instrument: plays the sound at its root key and keeps playing until you press again. You can also click the piano keys."),
        rise ("Riser", o.shared, "Starts the riser: grains shrink, get closer and fuse into a pitched tone while pitch and filter climb."),
        cap ("Capture", o.shared, "FX: record what the buffer holds into Hairball as a small sample and switch to Instrument to play it."),
        octDn ("-", o.shared), octUp ("+", o.shared),
        display (o.proc, L::fixed.disp), meter (o.proc, L::fixed.meter), piano (o.proc, L::fixed.kbd)
    {
        setOpaque (true);
        for (juce::Component* c : std::initializer_list<juce::Component*> { &prev, &next, &nameBox, &save, &rename, &del, &exp, &imp, &guide, &settings, &about, &zoom, &load, &freeze, &hold, &play, &rise, &cap, &octDn, &octUp, &display, &meter, &piano })
            addAndMakeVisible (c);
        nameBox.setBounds (rc (L::fixed.pname));
        for (auto* p : { &prev, &next, &save, &rename, &del, &exp, &imp, &guide, &settings, &about, &zoom, &load, &freeze, &hold, &play, &rise, &cap, &octDn, &octUp }) p->fontH = 13.0f;
        prev.setBounds (rc (L::fixed.prev)); next.setBounds (rc (L::fixed.next)); save.setBounds (rc (L::fixed.save)); rename.setBounds (rc (L::fixed.rename)); del.setBounds (rc (L::fixed.del));
        exp.setBounds (rc (L::fixed.exp)); imp.setBounds (rc (L::fixed.imp)); guide.setBounds (rc (L::fixed.guide)); settings.setBounds (rc (L::fixed.set)); about.setBounds (rc (L::fixed.about)); zoom.setBounds (rc (L::fixed.zoom));
        load.setBounds (rc (L::fixed.srcSel)); freeze.setBounds (rc (L::fixed.freeze)); hold.setBounds (rc (L::fixed.hold)); play.setBounds (rc (L::fixed.play)); rise.setBounds (rc (L::fixed.rise)); cap.setBounds (rc (L::fixed.cap));
        octDn.setBounds (rc (L::fixed.octDn)); octUp.setBounds (rc (L::fixed.octUp));
        play.onColour = col::green; play.fontH = 14.0f; play.solid = true; freeze.onColour = col::blue; hold.onColour = col::blue; rise.onColour = col::pink;

        const auto m = L::R { L::fixed.modeInst.x, L::fixed.modeInst.y, L::fixed.modeFx.x + L::fixed.modeFx.w - L::fixed.modeInst.x, L::fixed.modeInst.h };
        modeSeg = std::make_unique<Choice> (proc, "mode", m, m, o.shared, false); addAndMakeVisible (*modeSeg);
        for (auto& k : L::srcKnobs) { srcKnobs.add (new Knob (proc, k, o.shared)); addAndMakeVisible (srcKnobs.getLast()); }
        for (auto& k : L::outKnobs) { outKnobs.add (new Knob (proc, k, o.shared)); addAndMakeVisible (outKnobs.getLast()); }
        for (auto& s : L::outSegs) { outSegs.add (new Choice (proc, s.id, s.label, s.r, o.shared)); addAndMakeVisible (outSegs.getLast()); }

        static const char* tabNames[3] = { "Grains", "Pitch & Riser", "Thicken" };
        for (int t = 0; t < 3; ++t)
        {
            tabs[t] = std::make_unique<Btn> (tabNames[t], o.shared); tabs[t]->setBounds (rc (L::fixed.tabs[t])); tabs[t]->fontH = 14.0f;
            tabs[t]->onClick = [this, t] { showPage (t); }; addAndMakeVisible (*tabs[t]);
            const auto& pg = L::pages[t];
            for (int i = 0; i < pg.knobN; ++i) if (proc.apvts.getParameter (pg.knobs[i].id)) { auto* k = new Knob (proc, pg.knobs[i], o.shared); pageKnobs[t].add (k); addChildComponent (k); if (juce::String (pg.knobs[i].group) == "Amp") ampKnobs.add (k); }
            for (int i = 0; i < pg.segN; ++i) if (proc.apvts.getParameter (pg.segs[i].id)) { auto* c = new Choice (proc, pg.segs[i].id, pg.segs[i].label, pg.segs[i].r, o.shared); pageSegs[t].add (c); addChildComponent (c); }
            for (int i = 0; i < pg.gfxN; ++i)
            {
                const juce::String id (pg.gfx[i].id); Gfx* gfx = nullptr;
                if (id == "gshape") gfx = new GfxGshape (proc, pg.gfx[i]); else if (id == "lfo") gfx = new GfxLfo (proc, pg.gfx[i]); else if (id == "penv") gfx = new GfxPenv (proc, pg.gfx[i]);
                else if (id == "rise") gfx = new GfxRise (proc, pg.gfx[i]); else if (id == "resp") gfx = new GfxResp (proc, pg.gfx[i]); else if (id == "xfer") gfx = new GfxXfer (proc, pg.gfx[i]); else if (id == "gr") gfx = new GfxGr (proc, pg.gfx[i]);
                if (gfx != nullptr) { pageGfx[t].add (gfx); addChildComponent (gfx); }
            }
        }
        nameBox.onClick = [&o] { o.openList(); };
        showPage (juce::jlimit (0, 2, lastPage));
        piano.setBase (60);
    }

    void showPage (int p)
    {
        page = p; lastPage = p;
        for (int t = 0; t < 3; ++t)
        {
            const bool on = t == p;
            for (auto* c : pageKnobs[t]) c->setVisible (on);
            for (auto* c : pageSegs[t]) c->setVisible (on);
            for (auto* c : pageGfx[t]) c->setVisible (on);
            tabs[t]->setOn (on);
        }
        repaint();
    }

    static void box (juce::Graphics& g, juce::Rectangle<float> r, const juce::String& title = {})
    {
        g.setColour (col::panel2); g.fillRoundedRectangle (r, 4.0f);
        g.setColour (col::line); g.drawRoundedRectangle (r.reduced (0.5f), 4.0f, 1.0f);
        if (title.isNotEmpty()) { g.setColour (col::inkDim); g.setFont (labelFont (13.0f, true, 0.16f)); g.drawText (title.toUpperCase(), (int) r.getX() + 10, (int) r.getY() + 7, (int) r.getWidth() - 20, 16, juce::Justification::left, false); }
    }

    void paint (juce::Graphics& g) override
    {
        const auto& f = L::fixed;
        g.fillAll (col::panel);
        { const auto pr = juce::Rectangle<float> ((float) L::cheek, 0.0f, (float) (L::W - 2 * L::cheek), (float) L::H);
          g.setGradientFill (juce::ColourGradient (juce::Colour (0xff1e1f23), pr.getCentreX(), 0.0f, col::panel, pr.getCentreX(), 560.0f, true)); g.fillRect (pr); }
        catui::drawWoodCheeks (g, getLocalBounds(), L::cheek);

        // header
        catui::drawCatHead (g, juce::Rectangle<float> ((float) f.brand.x, (float) f.brand.y, 38.0f, 38.0f), col::ink, col::panel);
        const juce::String prod = proc.getName();
        g.setColour (col::ink); g.setFont (labelFont (31.0f, true, 0.12f)); g.drawText (prod.toUpperCase(), f.brand.x + 48, f.brand.y - 2, 220, 42, juce::Justification::centredLeft, false);
        const float nameW = juce::GlyphArrangement::getStringWidth (labelFont (31.0f, true, 0.12f), prod.toUpperCase());
        g.setColour (col::inkDim); g.setFont (labelFont (12.0f, false, 0.16f)); g.drawText ("STUDIO 87", f.brand.x + 48 + (int) nameW + 12, f.brand.y + 14, 120, 20, juce::Justification::centredLeft, false);
        g.setColour (col::line); g.drawHorizontalLine (f.pname.y + f.pname.h + 10, (float) L::cheek + 14.0f, (float) (L::W - L::cheek - 14));

        // source strip
        box (g, rc (f.srcBox).toFloat(), "Source");
        box (g, rc (f.disp).toFloat());
        box (g, rc (f.bufBox).toFloat(), "Buffer");
        g.setColour (col::inkDim); g.setFont (labelFont (12.0f, false, 0.03f));
        g.drawFittedText (hint, juce::Rectangle<int> (f.srcBox.x + 11, f.srcBox.y + f.srcBox.h - 20, f.srcBox.w - 20, 16), juce::Justification::centredLeft, 1, 0.9f);

        // tabs and page
        g.setColour (col::inkDim); g.setFont (monoFont (12.0f)); g.drawText (stat, rc (f.stat), juce::Justification::centredRight, false);
        const auto& pg = L::pages[page];
        for (int i = 0; i < pg.secN; ++i) box (g, rc (pg.secs[i].r).toFloat(), pg.secs[i].title);
        g.setColour (col::inkDim); g.setFont (labelFont (12.5f, false, 0.03f));
        for (int i = 0; i < pg.noteN; ++i) if (juce::String (pg.notes[i].text).isNotEmpty()) g.drawFittedText (pg.notes[i].text, rc (pg.notes[i].r), juce::Justification::topLeft, 3, 1.0f);

        // output column
        box (g, rc (f.outBox).toFloat(), "Output");
        box (g, juce::Rectangle<float> ((float) f.outBox.x, (float) (f.outBox.y + f.outBox.h + 10), (float) f.outBox.w, (float) (f.kbd.y - 10 - (f.outBox.y + f.outBox.h + 10))));

        // piano controls
        g.setColour (col::inkDim); g.setFont (labelFont (12.0f, false, 0.08f)); g.drawText ("OCTAVE", f.octBox.x, f.octBox.y + 2, 60, 14, juce::Justification::left, false);
        g.setColour (col::ink); g.setFont (labelFont (14.0f, true, 0.04f)); g.drawText (hb::fmt::noteName (piano.base), rc (f.octLbl).withX (f.octBox.x + 56).withWidth (60), juce::Justification::centredLeft, false);

        // footer
        g.setColour (col::line); g.drawHorizontalLine (f.footer.y, (float) f.footer.x, (float) (f.footer.x + f.footer.w));
        g.setColour (col::inkDim); g.setFont (labelFont (12.5f, false, 0.06f));
        g.drawText (juce::String ("Studio 87 \xc2\xb7 ") + proc.getName() + " v" + JucePlugin_VersionString, f.footer.x, f.footer.y + 6, 500, 16, juce::Justification::left, false);
        g.drawText (juce::String ("Collects nothing. No network calls. Reports ") + juce::String (proc.getLatencySamples()) + " samples of latency.", f.footer.x + f.footer.w - 520, f.footer.y + 6, 520, 16, juce::Justification::right, false);
    }

    void mouseDoubleClick (const juce::MouseEvent& e) override
    {
        if (e.x < L::cheek || e.x >= L::W - L::cheek) owner.setSize (L::W, L::H);     // double click on the wood: back to 100%
    }

    // called every frame
    void frame (bool statNow)
    {
        meter.tick(); display.repaint();
        for (auto* c : pageGfx[page]) c->repaint();
        const bool fx = proc.apvts.getRawParameterValue ("mode")->load() >= 0.5f;
        auto on = [&] (const char* id) { return proc.apvts.getRawParameterValue (id)->load() >= 0.5f; };
        freeze.setOn (on ("freeze")); hold.setOn (on ("hold")); rise.setOn (on ("riseOn"));
        if (fx != lastFx)
        {
            lastFx = fx;
            for (auto* k : ampKnobs) { k->setAlpha (fx ? 0.4f : 1.0f); k->setInterceptsMouseClicks (! fx, ! fx); }
            for (auto* k : srcKnobs) { k->setAlpha (fx ? 1.0f : 0.35f); k->setInterceptsMouseClicks (fx, fx); }
            freeze.setEnabled (fx); freeze.setAlpha (fx ? 1.0f : 0.5f); cap.setEnabled (fx); cap.setAlpha (fx ? 1.0f : 0.5f); play.setEnabled (! fx); play.setAlpha (fx ? 0.5f : 1.0f);
            if (fx) stopPlay();
        }
        if (statNow)
            stat = "Voices " + juce::String (proc.live.voices.load()) + " \xc2\xb7 Grains " + juce::String (proc.live.grains.load()) + " \xc2\xb7 CPU " + juce::String (proc.live.cpu.load() * 100.0f, 1) + "%";
        const auto s = proc.sampleSerial();
        const juce::String h = owner.note.isNotEmpty() && juce::Time::getMillisecondCounterHiRes() < owner.noteUntil ? owner.note
                                : (proc.missingSample().isNotEmpty() ? "Not found: " + proc.missingSample() : (proc.sampleName().isNotEmpty() ? "Sound: " + proc.sampleName() : juce::String()));
        if (h != hint || s != lastSerial) { hint = h; lastSerial = s; }
        repaint (juce::Rectangle<int> (L::fixed.stat.x - 4, L::fixed.stat.y - 2, L::fixed.stat.w + 8, L::fixed.stat.h + 4));
        repaint (rc (L::fixed.srcBox));
    }

    void togglePlay()
    {
        if (playing >= 0) { stopPlay(); return; }
        playing = juce::roundToInt (proc.apvts.getRawParameterValue ("root")->load());
        proc.keyState.noteOn (1, playing, 0.85f); play.setOn (true); play.setText (juce::String (juce::CharPointer_UTF8 ("\xe2\x96\xa0 Stop")));
    }
    void stopPlay()
    {
        if (playing >= 0) proc.keyState.noteOff (1, playing, 0.0f);
        playing = -1; play.setOn (false); play.setText (juce::String (juce::CharPointer_UTF8 ("\xe2\x96\xb6 Play")));
    }

    HairballEditor& owner; HairballProcessor& proc;
    Btn prev, next, save, rename, del, exp, imp, guide, settings, about, zoom, load, freeze, hold, play, rise, cap, octDn, octUp;
    NameBox nameBox { owner.shared };
    MainDisplay display; MeterView meter; Piano piano;
    std::unique_ptr<Choice> modeSeg;
    std::unique_ptr<Btn> tabs[3];
    juce::OwnedArray<Knob> pageKnobs[3], srcKnobs, outKnobs; juce::OwnedArray<Choice> pageSegs[3], outSegs; juce::OwnedArray<Gfx> pageGfx[3];
    juce::Array<Knob*> ampKnobs;
    juce::String hint, stat; uint32_t lastSerial = 0; int page = 0; static inline int lastPage = 0; bool lastFx = false; int playing = -1;
};

//==============================================================================
HairballEditor::HairballEditor (HairballProcessor& p) : AudioProcessorEditor (&p), proc (p)
{
    shared.tips = proc.tipsOn.load();
    look = std::make_unique<Look>(); setLookAndFeel (look.get());
    content = std::make_unique<Content> (*this); addAndMakeVisible (*content); content->setSize (L::W, L::H);
    tooltip.setLookAndFeel (look.get());

    content->save.onClick = [this] { saveDialog(); }; content->rename.onClick = [this] { renameDialog(); }; content->del.onClick = [this] { deleteDialog(); };
    content->exp.onClick = [this] { exportDialog(); }; content->imp.onClick = [this] { importDialog(); };
    content->prev.onClick = [this] { step (-1); }; content->next.onClick = [this] { step (1); };
    content->guide.onClick = [this] { showGuide(); }; content->about.onClick = [this] { showAbout(); }; content->settings.onClick = [this] { showSettings(); };
    content->zoom.onClick = [this] { cycleZoom(); };
    content->load.onClick = [this]
    {
        chooser = std::make_unique<juce::FileChooser> ("Pick an audio file", juce::File(), "*.wav;*.aif;*.aiff;*.flac;*.ogg;*.mp3", true);
        chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles, [this] (const juce::FileChooser& fc) { if (fc.getResult().existsAsFile()) loadFile (fc.getResult()); });
    };
    content->freeze.onClick = [this] { if (auto* q = proc.apvts.getParameter ("freeze")) q->setValueNotifyingHost (q->getValue() > 0.5f ? 0.0f : 1.0f); };
    // Hold: a click latches it; press and hold gives a stutter that lets go on release
    content->hold.onDown = [this] { holdDownAt = juce::Time::getMillisecondCounterHiRes(); auto* q = proc.apvts.getParameter ("hold"); holdWasOn = q->getValue() > 0.5f; q->setValueNotifyingHost (holdWasOn ? 0.0f : 1.0f); };
    content->hold.onUp = [this] { if (! holdWasOn && juce::Time::getMillisecondCounterHiRes() - holdDownAt > 300.0) proc.apvts.getParameter ("hold")->setValueNotifyingHost (0.0f); };
    content->rise.onClick = [this]
    {
        auto* q = proc.apvts.getParameter ("riseOn"); const bool turnOn = q->getValue() < 0.5f; q->setValueNotifyingHost (turnOn ? 1.0f : 0.0f);
        if (turnOn && proc.apvts.getRawParameterValue ("mode")->load() < 0.5f && proc.live.voices.load() == 0) content->togglePlay();
    };
    content->cap.onClick = [this] { proc.requestCapture(); note = "Capturing..."; noteUntil = juce::Time::getMillisecondCounterHiRes() + 1500.0; };
    content->play.onClick = [this] { content->togglePlay(); };
    content->octDn.onClick = [this] { content->piano.setBase (content->piano.base - 12); content->repaint(); };
    content->octUp.onClick = [this] { content->piano.setBase (content->piano.base + 12); content->repaint(); };
    refreshNames();

    constrainer.setFixedAspectRatio ((double) L::W / (double) L::H);
    constrainer.setSizeLimits ((int) (L::W * 0.6), (int) (L::H * 0.6), L::W * 2, L::H * 2);
    setConstrainer (&constrainer);
    setResizable (true, false);
    addAndMakeVisible (border); border.setBorderThickness (juce::BorderSize<int> (6));
    const int w = proc.editorWidth > 0 ? juce::jlimit ((int) (L::W * 0.6), L::W * 2, proc.editorWidth) : L::W;
    setSize (w, (int) std::lround (w * (double) L::H / L::W));

    gate.onFrame = [this] { frame(); };
    restartTimer();
}

HairballEditor::~HairballEditor()
{
    gate.stop();
    tooltip.setLookAndFeel (nullptr); setLookAndFeel (nullptr);
}

void HairballEditor::restartTimer() { gate.start (proc.powerSave.load() ? 15 : 30); }

void HairballEditor::resized()
{
    const float s = (float) getWidth() / (float) L::W;
    content->setTransform (juce::AffineTransform());
    content->setBounds (0, 0, L::W, L::H);
    content->setTransform (juce::AffineTransform::scale (s));
    border.setBounds (getLocalBounds());
    proc.editorWidth = getWidth();
    content->zoom.setText (juce::String (juce::roundToInt (s * 100.0f)) + "%");
}

void HairballEditor::cycleZoom()
{
    const int steps[] = { 75, 100, 125, 150, 200 }; const int cur = juce::roundToInt ((float) getWidth() / L::W * 100.0f);
    int next = steps[0]; for (int v : steps) if (v > cur + 1) { next = v; break; }
    const int w = L::W * next / 100; setSize (w, (int) std::lround (w * (double) L::H / L::W));
}

void HairballEditor::frame()
{
    static int n = 0; ++n;
    shared.tips = proc.tipsOn.load();
    content->frame (n % 6 == 0);
    auto& nb = content->nameBox;
    if (nb.name != proc.currentPresetName) refreshNames();
}

//==============================================================================
void HairballEditor::loadFile (const juce::File& f)
{
    juce::String err;
    if (proc.loadAudioFile (f, err))
    {
        if (auto* q = proc.apvts.getParameter ("mode")) q->setValueNotifyingHost (0.0f);
        note = err; noteUntil = juce::Time::getMillisecondCounterHiRes() + 5000.0;
    }
    else { note = err; noteUntil = juce::Time::getMillisecondCounterHiRes() + 6000.0; }
}
void HairballEditor::filesDropped (const juce::StringArray& files, int, int)
{
    content->display.hot = false;
    if (files.size() > 0) loadFile (juce::File (files[0]));
}
void HairballEditor::fileDragEnter (const juce::StringArray&, int, int) { content->display.hot = true; }
void HairballEditor::fileDragExit (const juce::StringArray&) { content->display.hot = false; }

//==============================================================================
void HairballEditor::refreshNames()
{
    names.clear();
    for (auto& f : hb::factoryPresets()) names.add (f.name);
    numFactory = names.size();
    names.addArray (proc.listUserPresets());
    auto& nb = content->nameBox; nb.name = proc.currentPresetName; nb.category = "User";
    for (auto& f : hb::factoryPresets()) if (proc.currentPresetName == f.name) nb.category = f.category;
    nb.repaint();
}

void HairballEditor::step (int dir)
{
    refreshNames(); if (names.isEmpty()) return;
    const int cur = names.indexOf (proc.currentPresetName), n = names.size();
    const int next = cur < 0 ? 0 : (cur + dir + n) % n;
    if (next < numFactory) proc.setCurrentProgram (next); else proc.loadUserPreset (names[next]);
    refreshNames();
}

void HairballEditor::openList()
{
    refreshNames();
    juce::PopupMenu menu; int id = 1;
    for (auto& f : hb::factoryPresets()) { menu.addItem (id, juce::String (f.category).toUpperCase() + "   " + f.name, true, proc.currentPresetName == f.name); ++id; }
    if (names.size() > numFactory) { menu.addSeparator(); for (int i = numFactory; i < names.size(); ++i) menu.addItem (id++, "USER   " + names[i], true, proc.currentPresetName == names[i]); }
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&content->nameBox).withMinimumWidth (content->nameBox.getWidth()).withStandardItemHeight (26),
                        [this] (int r) { if (r <= 0) return; const int i = r - 1; if (i < numFactory) proc.setCurrentProgram (i); else proc.loadUserPreset (names[i]); refreshNames(); });
}

void HairballEditor::saveNamed (const juce::String& nm)
{
    if (proc.listUserPresets().contains (juce::File::createLegalFileName (nm.trim()).substring (0, 40)))   // never overwrite silently
    {
        juce::AlertWindow::showAsync (juce::MessageBoxOptions().withIconType (juce::MessageBoxIconType::QuestionIcon).withTitle ("Overwrite sound?")
            .withMessage ("A sound called \"" + nm + "\" already exists. Overwrite it with the current settings?").withButton ("Overwrite").withButton ("Cancel"),
            [this, nm] (int pick) { if (pick == 1) { proc.saveUserPreset (nm); refreshNames(); } });
    }
    else { proc.saveUserPreset (nm); refreshNames(); }
}

void HairballEditor::saveDialog()
{
    dialog = std::make_unique<juce::AlertWindow> ("Save sound", "Name for this sound. Saving under an existing name of yours asks before it overwrites.", juce::MessageBoxIconType::NoIcon);
    dialog->addTextEditor ("name", proc.currentPresetName == "Init" ? "My sound" : proc.currentPresetName);
    dialog->addButton ("Save", 1, juce::KeyPress (juce::KeyPress::returnKey)); dialog->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));
    dialog->enterModalState (true, juce::ModalCallbackFunction::create ([this] (int r)
    {
        if (r == 1 && dialog != nullptr) { const auto nm = dialog->getTextEditorContents ("name").trim(); if (nm.isNotEmpty()) saveNamed (nm); }
        dialog.reset();
    }), false);
}

void HairballEditor::renameDialog()
{
    if (! proc.listUserPresets().contains (proc.currentPresetName))
    {
        juce::AlertWindow::showAsync (juce::MessageBoxOptions().withIconType (juce::MessageBoxIconType::InfoIcon).withTitle ("Rename")
            .withMessage ("Factory sounds keep their names. Save a copy first, then rename the copy.").withButton ("OK"), nullptr);
        return;
    }
    dialog = std::make_unique<juce::AlertWindow> ("Rename sound", "New name", juce::MessageBoxIconType::NoIcon);
    dialog->addTextEditor ("name", proc.currentPresetName);
    dialog->addButton ("Rename", 1, juce::KeyPress (juce::KeyPress::returnKey)); dialog->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));
    dialog->enterModalState (true, juce::ModalCallbackFunction::create ([this] (int r)
    {
        if (r == 1 && dialog != nullptr) { const auto nm = dialog->getTextEditorContents ("name").trim(); if (nm.isNotEmpty() && nm != proc.currentPresetName) proc.renameUserPreset (proc.currentPresetName, nm); refreshNames(); }
        dialog.reset();
    }), false);
}

void HairballEditor::deleteDialog()
{
    if (! proc.listUserPresets().contains (proc.currentPresetName))
    {
        juce::AlertWindow::showAsync (juce::MessageBoxOptions().withIconType (juce::MessageBoxIconType::InfoIcon).withTitle ("Delete")
            .withMessage ("Factory sounds cannot be deleted.").withButton ("OK"), nullptr);
        return;
    }
    const auto nm = proc.currentPresetName;
    juce::AlertWindow::showAsync (juce::MessageBoxOptions().withIconType (juce::MessageBoxIconType::QuestionIcon).withTitle ("Delete sound")
        .withMessage ("Delete \"" + nm + "\"? This cannot be undone.").withButton ("Delete").withButton ("Cancel"),
        [this, nm] (int pick) { if (pick == 1) { proc.deleteUserPreset (nm); proc.setCurrentProgram (0); refreshNames(); } });
}

void HairballEditor::textDialog (const juce::String& title, const juce::String& hint, const juce::String& text, bool readOnly, const juce::String& ok,
                                 std::function<juce::String (const juce::String&)> onOk)
{
    dialog = std::make_unique<juce::AlertWindow> (title, hint, juce::MessageBoxIconType::NoIcon);
    auto* te = new juce::TextEditor(); te->setMultiLine (true, true); te->setReturnKeyStartsNewLine (true); te->setReadOnly (readOnly); te->setFont (monoFont (12.0f));
    te->setSize (520, 170); te->setText (text, false); dialog->addCustomComponent (te);
    juce::Component::SafePointer<juce::TextEditor> sp (te);
    if (readOnly) { dialog->addButton ("Copy", 2); }
    dialog->addButton (ok, 1, juce::KeyPress()); dialog->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));
    dialog->enterModalState (true, juce::ModalCallbackFunction::create ([this, sp, onOk, readOnly] (int r)
    {
        const auto txt = sp != nullptr ? sp->getText() : juce::String();
        if (r == 2) juce::SystemClipboard::copyTextToClipboard (txt);
        const bool reopen = r == 2;
        if (r == 1 && onOk) { const auto err = onOk (txt); if (err.isNotEmpty()) { dialog.reset(); juce::AlertWindow::showAsync (juce::MessageBoxOptions().withIconType (juce::MessageBoxIconType::WarningIcon).withTitle ("Preset").withMessage (err).withButton ("OK"), nullptr); return; } }
        dialog.reset();
        if (reopen && readOnly) exportDialog();
    }), false);
}

void HairballEditor::exportDialog()
{
    textDialog ("Export preset", "Copy this text and keep it anywhere. Import takes it back.", proc.presetAsText(), true, "Close", nullptr);
}
void HairballEditor::importDialog()
{
    textDialog ("Import preset", "Paste preset text from Export. It is checked before it is used.", "", false, "Import", [this] (const juce::String& t)
    {
        juce::String imported; const auto err = proc.importPresetText (t, imported);
        refreshNames(); return err;
    });
}

//==============================================================================
void HairballEditor::showGuide()
{
    juce::AlertWindow::showAsync (juce::MessageBoxOptions().withIconType (juce::MessageBoxIconType::NoIcon).withTitle ("Guide").withMessage (
        "1. Instrument: drop an audio file on the display (or click Load audio), then play the piano or your MIDI keyboard.\n"
        "2. FX: the track's audio plays through Hairball. Press Freeze to loop what it just heard, or Hold for a stutter.\n"
        "3. Capture turns the buffer into a small sample you can play with the keys.\n"
        "4. Riser: grains shrink, get closer and fuse into a pitched tone while pitch and filter climb.\n"
        "5. Thicken: width, EQ, filter, Drive / Transistor / Fuzz and Dance.\n\n"
        "Key to: Percent, Step and Transient use the keys from C5 up (green marks on the piano). Every control is a host parameter, so it can be automated. "
        "Hover a control for one second to see what it does.").withButton ("Close"), nullptr);
}

void HairballEditor::showAbout()
{
    juce::AlertWindow::showAsync (juce::MessageBoxOptions().withIconType (juce::MessageBoxIconType::NoIcon).withTitle ("About & Support").withMessage (
        juce::String ("Hairball by Studio 87, version ") + JucePlugin_VersionString + ".\n\n"
        "Free and open source (AGPL-3.0-or-later). It collects nothing and makes no network calls. The button below only opens a link in your browser when you click it.\n\n"
        "Your saved sounds live in: " + HairballProcessor::userPresetDir().getFullPathName() + "\n\n"
        "This is a test build: not yet tuned by ear.").withButton ("Support on Ko-fi").withButton ("Close"),
        [] (int pick) { if (pick == 1) juce::URL ("https://ko-fi.com/studio87").launchInDefaultBrowser(); });
}

void HairballEditor::showSettings()
{
    juce::PopupMenu m;
    m.addItem (1, "Info bubbles (after 1 second)", true, proc.tipsOn.load());
    m.addItem (2, "Power saving (slower drawing)", true, proc.powerSave.load());
    m.addItem (3, "Show grains on the display", true, proc.showGrains.load());
    m.addSeparator();
    m.addItem (4, "Reset window size to 100%");
    m.addItem (5, "Open the GitHub page");
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&content->settings), [this] (int r)
    {
        if (r == 1) proc.tipsOn = ! proc.tipsOn.load();
        else if (r == 2) { proc.powerSave = ! proc.powerSave.load(); restartTimer(); }
        else if (r == 3) proc.showGrains = ! proc.showGrains.load();
        else if (r == 4) setSize (L::W, L::H);
        else if (r == 5) juce::URL ("https://github.com/SpeakingColors/hairball").launchInDefaultBrowser();
    });
}
