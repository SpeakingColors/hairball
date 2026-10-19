#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "Presets.h"
#include "ui/Format.h"

using namespace hb;

//==============================================================================
struct HairballProcessor::Source                              // what the user loaded or captured, kept so a sample-rate change can rebuild the engine copy
{
    std::vector<float> l, r;
    double sr = 48000.0;
    juce::String name, path;
    bool captured = false;
};

static juce::AudioProcessorValueTreeState::ParameterLayout createLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    for (int i = 0; i < pCount; ++i)
    {
        const auto& d = kParams[i];
        const juce::ParameterID pid { d.id, 1 };
        float def = d.def;
        if (i == pMode && HairballProcessor::kFxBuild) def = 1.0f;
        if (d.choice)
        {
            layout.add (std::make_unique<juce::AudioParameterChoice> (pid, d.name, juce::StringArray::fromTokens (d.choices, "|", ""), (int) def));
        }
        else if (fmt::isInt (i))
        {
            layout.add (std::make_unique<juce::AudioParameterInt> (pid, d.name, (int) d.lo, (int) d.hi, (int) def,
                        juce::AudioParameterIntAttributes().withStringFromValueFunction ([i] (int v, int) { return fmt::value (i, (float) v); })));
        }
        else
        {
            juce::NormalisableRange<float> range (d.lo, d.hi);
            if (fmt::isLog (i))
                range = juce::NormalisableRange<float> (d.lo, d.hi,
                    [] (float lo, float hi, float n) { return lo * std::pow (hi / lo, n); },
                    [] (float lo, float hi, float v) { return std::log (juce::jlimit (lo, hi, v) / lo) / std::log (hi / lo); },
                    [] (float lo, float hi, float v) { return juce::jlimit (lo, hi, v); });
            else if (fmt::isSkew (i))
                range = juce::NormalisableRange<float> (d.lo, d.hi, 0.0f, 0.5f);          // value = lo + n^2 * range, like the mockup
            layout.add (std::make_unique<juce::AudioParameterFloat> (pid, d.name, range, def,
                        juce::AudioParameterFloatAttributes().withLabel (d.unit).withStringFromValueFunction ([i] (float v, int) { return fmt::value (i, v); })));
        }
    }
    return layout;
}

//==============================================================================
HairballProcessor::HairballProcessor()
    : AudioProcessor (BusesProperties()
                        .withInput ("Input", juce::AudioChannelSet::stereo(), kFxBuild)
                        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "Hairball", createLayout())
{
    for (int i = 0; i < pCount; ++i) raw[i] = apvts.getRawParameterValue (kParams[i].id);
    formats.registerBasicFormats();
    for (auto& r : retired) r.store (nullptr);
    startTimerHz (10);
}

HairballProcessor::~HairballProcessor()
{
    stopTimer();
    delete pending.exchange (nullptr);
    for (auto& r : retired) delete r.exchange (nullptr);
}

bool HairballProcessor::isBusesLayoutSupported (const BusesLayout& l) const
{
    if (l.getMainOutputChannelSet() != juce::AudioChannelSet::stereo()) return false;
    const auto in = l.getMainInputChannelSet();
    return in.isDisabled() || in == juce::AudioChannelSet::stereo();
}

void HairballProcessor::prepareToPlay (double sampleRate, int)
{
    {
        const juce::SpinLock::ScopedLockType sl (displayLock);       // the editor does not read the buffers while they are reallocated
        sr = sampleRate;
        engine.prepare (sampleRate);
        capBuf.l.clear(); capBuf.r.clear();
        capBuf.l.reserve ((size_t) (hb::Engine::kMaxRingSeconds * sampleRate) + 16); capBuf.r.reserve ((size_t) (hb::Engine::kMaxRingSeconds * sampleRate) + 16);
    }
    setLatencySamples (engine.latencySamples());
    captureReady.store (false); captureReq.store (false);
    submitSource();                                              // the engine copy depends on the sample rate
}

//==============================================================================
void HairballProcessor::retire (hb::Engine::Sample* s)
{
    for (auto& r : retired)
    {
        hb::Engine::Sample* expected = nullptr;
        if (r.compare_exchange_strong (expected, s)) return;
    }
    // all four slots busy cannot happen: the message thread empties them 10 times a second and only one sample is handed over per block. Leaking one sample is safer than freeing on the audio thread.
}

void HairballProcessor::readParams()
{
    for (int i = 0; i < pCount; ++i) hp.v[i] = raw[i]->load();
    engine.setParams (hp);
}

void HairballProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    const auto t0 = juce::Time::getHighResolutionTicks();
    const int n = buffer.getNumSamples();
    if (buffer.getNumChannels() < 2) { buffer.clear(); return; }

    // a new sample from the message thread (only when a slot is free to hand the old one back)
    bool slotFree = false; for (auto& r : retired) if (r.load() == nullptr) { slotFree = true; break; }
    if (slotFree)
        if (auto* p = pending.exchange (nullptr)) { engine.swapSample (*p); retire (p); }
    if (captureReq.exchange (false) && ! captureReady.load()) captureReady.store (engine.captureInto (capBuf));

    const bool haveIn = getBus (true, 0) != nullptr && getBus (true, 0)->isEnabled();
    float* L = buffer.getWritePointer (0);
    float* R = buffer.getWritePointer (1);
    if (! haveIn) buffer.clear();

    keyState.processNextMidiBuffer (midi, 0, n, true);           // notes played on the editor piano
    readParams();

    int pos = 0;
    auto render = [&] (int end)
    {
        if (end > pos) { engine.process (haveIn ? L + pos : nullptr, haveIn ? R + pos : nullptr, L + pos, R + pos, end - pos); pos = end; }
    };
    for (const auto meta : midi)
    {
        const auto m = meta.getMessage();
        render (juce::jmin (meta.samplePosition, n));
        if (m.isNoteOn()) engine.noteOn (m.getNoteNumber(), m.getFloatVelocity());
        else if (m.isNoteOff()) engine.noteOff (m.getNoteNumber());
        else if (m.isAllNotesOff() || m.isAllSoundOff()) engine.allNotesOff();
    }
    render (n);

    float pl = 0.0f, pr = 0.0f; double sl = 0.0, sr2 = 0.0; bool bad = false;
    for (int i = 0; i < n; ++i)
    {
        if (! std::isfinite (L[i]) || ! std::isfinite (R[i])) { bad = true; L[i] = R[i] = 0.0f; }
        pl = std::max (pl, std::fabs (L[i])); pr = std::max (pr, std::fabs (R[i])); sl += (double) L[i] * L[i]; sr2 += (double) R[i] * R[i];
    }
    if (bad) engine.reset();                                     // a runaway value should never reach the speakers
    live.peakL.store (std::max (live.peakL.load (std::memory_order_relaxed), pl), std::memory_order_relaxed);
    live.peakR.store (std::max (live.peakR.load (std::memory_order_relaxed), pr), std::memory_order_relaxed);
    live.rmsL.store ((float) std::sqrt (sl / std::max (1, n)), std::memory_order_relaxed); live.rmsR.store ((float) std::sqrt (sr2 / std::max (1, n)), std::memory_order_relaxed);

    const bool fx = hp[pMode] >= 0.5f;
    live.riser.store (engine.riserProgress(), std::memory_order_relaxed);
    live.voices.store (fx ? 1 : engine.activeVoices(), std::memory_order_relaxed);
    live.grains.store (engine.activeGrains(), std::memory_order_relaxed);
    live.frozen.store (engine.isFrozen(), std::memory_order_relaxed);
    live.grDb.store (juce::jlimit (0.0f, 12.0f, -engine.lastGainReductionDb()), std::memory_order_relaxed);
    float tmp[192]; int k = engine.voiceHeads (tmp, 8);
    for (int i = 0; i < k; ++i) live.heads[i].store (tmp[i], std::memory_order_relaxed);
    live.nHeads.store (k, std::memory_order_relaxed);
    k = engine.grainMarks (tmp, 192);
    for (int i = 0; i < k; ++i) live.marks[i].store (tmp[i], std::memory_order_relaxed);
    live.nMarks.store (k, std::memory_order_relaxed);
    const double used = juce::Time::highResolutionTicksToSeconds (juce::Time::getHighResolutionTicks() - t0) / ((double) n / sr);
    live.cpu.store (live.cpu.load (std::memory_order_relaxed) * 0.9f + (float) used * 0.1f, std::memory_order_relaxed);
}

//==============================================================================
// Message thread: the sound
void HairballProcessor::timerCallback()
{
    for (auto& r : retired) delete r.exchange (nullptr);
    if (captureReady.load())
    {
        auto s = std::make_shared<Source>();
        s->l.assign (capBuf.l.begin(), capBuf.l.begin() + (long) capBuf.n); s->r.assign (capBuf.r.begin(), capBuf.r.begin() + (long) capBuf.n);
        s->sr = sr; s->name = "Captured audio"; s->captured = true;
        captureReady.store (false);
        setSource (s);
        if (auto* p = apvts.getParameter ("freeze")) p->setValueNotifyingHost (0.0f);
        if (auto* p = apvts.getParameter ("hold")) p->setValueNotifyingHost (0.0f);
        if (auto* p = apvts.getParameter ("mode")) p->setValueNotifyingHost (0.0f);
    }
}

void HairballProcessor::requestCapture() { captureReq.store (true); }

void HairballProcessor::setSource (std::shared_ptr<const Source> s)
{
    { const juce::SpinLock::ScopedLockType sl (lock); source = s; missing = {}; }
    submitSource();
    serial.fetch_add (1);
}

void HairballProcessor::submitSource()
{
    std::shared_ptr<const Source> s;
    { const juce::SpinLock::ScopedLockType sl (lock); s = source; }
    if (s == nullptr || s->l.size() < 64) return;
    auto* smp = new hb::Engine::Sample();
    hb::Engine::makeSample (*smp, s->l.data(), s->r.data(), s->l.size(), s->sr, sr);

    auto pk = std::make_shared<Peaks>();
    pk->name = s->name; pk->seconds = (double) s->l.size() / s->sr;
    const size_t cols = 1024, n = s->l.size();
    pk->lo.assign (cols, 0.0f); pk->hi.assign (cols, 0.0f);
    for (size_t c = 0; c < cols; ++c)
    {
        const size_t a = c * n / cols, b = std::max (a + 1, (c + 1) * n / cols); float mn = 0, mx = 0;
        const size_t step = std::max<size_t> (1, (b - a) / 24);
        for (size_t i = a; i < b && i < n; i += step) { mn = std::min (mn, s->l[i]); mx = std::max (mx, s->l[i]); }
        pk->lo[c] = mn; pk->hi[c] = mx;
    }
    for (auto t : smp->detected) pk->marks.push_back ((float) t / (float) n);

    delete pending.exchange (smp);
    { const juce::SpinLock::ScopedLockType sl (lock); peaksPtr = pk; }
}

bool HairballProcessor::loadAudioFile (const juce::File& f, juce::String& error)
{
    if (! f.existsAsFile()) { error = "That file does not exist."; return false; }
    if (f.getSize() > 400 * 1024 * 1024) { error = "That file is larger than 400 MB."; return false; }
    std::unique_ptr<juce::AudioFormatReader> rd (formats.createReaderFor (f));
    if (rd == nullptr || rd->lengthInSamples < 64 || rd->numChannels < 1) { error = "Could not read that file as audio. WAV, AIFF, FLAC and OGG work everywhere."; return false; }
    const int64_t maxN = (int64_t) (60.0 * rd->sampleRate);
    const int64_t n = std::min<int64_t> (rd->lengthInSamples, maxN);
    juce::AudioBuffer<float> b ((int) std::min<unsigned> (rd->numChannels, 2u), (int) n);
    rd->read (&b, 0, (int) n, 0, true, true);
    auto s = std::make_shared<Source>();
    s->l.assign (b.getReadPointer (0), b.getReadPointer (0) + n);
    if (b.getNumChannels() > 1) s->r.assign (b.getReadPointer (1), b.getReadPointer (1) + n); else s->r = s->l;
    s->sr = rd->sampleRate; s->name = f.getFileName(); s->path = f.getFullPathName();
    setSource (s);
    error = rd->lengthInSamples > maxN ? "Only the first 60 seconds are used." : juce::String();
    return true;
}

juce::String HairballProcessor::sampleName() const { const juce::SpinLock::ScopedLockType sl (lock); return source ? source->name : juce::String(); }
juce::String HairballProcessor::missingSample() const { const juce::SpinLock::ScopedLockType sl (lock); return missing; }
std::shared_ptr<const HairballProcessor::Peaks> HairballProcessor::peaks() const { const juce::SpinLock::ScopedLockType sl (lock); return peaksPtr; }

//==============================================================================
// Programs and presets
int HairballProcessor::getNumPrograms() { return (int) factoryPresets().size(); }
const juce::String HairballProcessor::getProgramName (int i) { auto& f = factoryPresets(); return i >= 0 && i < (int) f.size() ? juce::String (f[(size_t) i].name) : juce::String(); }
void HairballProcessor::setCurrentProgram (int i) { applyFactory (i); }

void HairballProcessor::applyFactory (int index)
{
    auto& list = factoryPresets();
    if (index < 0 || index >= (int) list.size()) return;
    for (auto* prm : getParameters())                                  // start from defaults so a preset never inherits leftovers
        if (auto* r = dynamic_cast<juce::RangedAudioParameter*> (prm)) r->setValueNotifyingHost (r->getDefaultValue());
    for (auto& kv : list[(size_t) index].values)
        if (auto* r = apvts.getParameter (kv.first))
        {
            const int pi = indexOf (kv.first); const float v = pi >= 0 ? juce::jlimit (kParams[pi].lo, kParams[pi].hi, kv.second) : kv.second;
            r->setValueNotifyingHost (r->convertTo0to1 (v));
        }
    currentFactory = index; currentPresetName = list[(size_t) index].name;
}

juce::String HairballProcessor::sanitize (const juce::String& n) { return juce::File::createLegalFileName (n.trim()).substring (0, 40); }

juce::File HairballProcessor::userPresetDir()
{
    return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory).getChildFile ("Studio 87").getChildFile ("Hairball").getChildFile ("presets");
}
juce::StringArray HairballProcessor::listUserPresets() const
{
    juce::StringArray out;
    for (auto& f : userPresetDir().findChildFiles (juce::File::findFiles, false, "*.hairball")) out.add (f.getFileNameWithoutExtension());
    out.sort (true); return out;
}
bool HairballProcessor::saveUserPreset (const juce::String& name)
{
    auto clean = sanitize (name); if (clean.isEmpty()) return false;
    auto dir = userPresetDir(); if (! dir.createDirectory().wasOk()) return false;
    auto state = apvts.copyState(); std::unique_ptr<juce::XmlElement> xml (state.createXml());
    xml->setAttribute ("presetName", clean); xml->setAttribute ("format", 1);
    currentPresetName = clean;
    return xml->writeTo (dir.getChildFile (clean + ".hairball"));
}
bool HairballProcessor::renameUserPreset (const juce::String& from, const juce::String& to)
{
    auto cleanTo = sanitize (to); if (cleanTo.isEmpty()) return false;
    auto src = userPresetDir().getChildFile (sanitize (from) + ".hairball");
    auto dst = userPresetDir().getChildFile (cleanTo + ".hairball");
    if (! src.existsAsFile() || (dst.existsAsFile() && dst != src)) return false;
    std::unique_ptr<juce::XmlElement> xml (juce::XmlDocument::parse (src));
    if (xml == nullptr) return false;
    xml->setAttribute ("presetName", cleanTo);
    if (! xml->writeTo (dst)) return false;
    if (dst != src) src.deleteFile();
    if (currentPresetName == from) currentPresetName = cleanTo;
    return true;
}
bool HairballProcessor::deleteUserPreset (const juce::String& name)
{
    return userPresetDir().getChildFile (sanitize (name) + ".hairball").deleteFile();
}

static void resetAll (juce::AudioProcessor& p)
{
    for (auto* prm : p.getParameters()) if (auto* r = dynamic_cast<juce::RangedAudioParameter*> (prm)) r->setValueNotifyingHost (r->getDefaultValue());
}
static void setClamped (juce::AudioProcessorValueTreeState& a, const juce::String& id, float v)
{
    const int pi = indexOf (id.toRawUTF8());
    if (pi < 0 || ! std::isfinite (v)) return;
    if (auto* r = a.getParameter (id)) r->setValueNotifyingHost (r->convertTo0to1 (juce::jlimit (kParams[pi].lo, kParams[pi].hi, v)));
}

bool HairballProcessor::loadUserPreset (const juce::String& name)
{
    auto file = userPresetDir().getChildFile (sanitize (name) + ".hairball");
    if (! file.existsAsFile() || file.getSize() > 1024 * 256) return false;
    std::unique_ptr<juce::XmlElement> xml (juce::XmlDocument::parse (file));
    if (xml == nullptr || ! xml->hasTagName (apvts.state.getType())) return false;
    resetAll (*this);                                              // reset to defaults, then apply only known parameters, clamped
    for (auto* child : xml->getChildWithTagNameIterator ("PARAM")) setClamped (apvts, child->getStringAttribute ("id"), (float) child->getDoubleAttribute ("value"));
    currentPresetName = name; return true;
}

juce::String HairballProcessor::presetAsText() const
{
    auto* p = new juce::DynamicObject();
    for (int i = 0; i < pCount; ++i)
    {
        auto* r = apvts.getParameter (kParams[i].id); if (r == nullptr) continue;
        const float v = r->convertFrom0to1 (r->getValue()), def = r->convertFrom0to1 (r->getDefaultValue());
        if (std::abs (v - def) > 1.0e-6f) p->setProperty (kParams[i].id, std::round (v * 1.0e4) / 1.0e4);
    }
    auto* o = new juce::DynamicObject();
    o->setProperty ("hairball", 1); o->setProperty ("name", currentPresetName); o->setProperty ("p", juce::var (p));
    return juce::JSON::toString (juce::var (o), true);
}

juce::String HairballProcessor::importPresetText (const juce::String& text, juce::String& importedName)
{
    if (text.length() > 64 * 1024) return "That text is too long to be a Hairball preset.";
    const auto v = juce::JSON::parse (text);
    auto* o = v.getDynamicObject();
    if (o == nullptr || (int) o->getProperty ("hairball") != 1) return "That is not a valid Hairball preset. Nothing was changed.";
    auto* p = o->getProperty ("p").getDynamicObject();
    if (p == nullptr) return "That is not a valid Hairball preset. Nothing was changed.";
    for (auto& kv : p->getProperties())
        if (indexOf (kv.name.toString().toRawUTF8()) < 0 || ! (kv.value.isDouble() || kv.value.isInt() || kv.value.isInt64()) || ! std::isfinite ((double) kv.value))
            return "That is not a valid Hairball preset. Nothing was changed.";
    juce::String nm = o->getProperty ("name").toString().trim().substring (0, 40); if (nm.isEmpty()) nm = "Imported";
    nm = sanitize (nm); if (nm.isEmpty()) nm = "Imported";
    auto taken = [this] (const juce::String& s)
    {
        for (auto& f : factoryPresets()) if (juce::String (f.name).equalsIgnoreCase (s)) return true;
        for (auto& u : listUserPresets()) if (u.equalsIgnoreCase (s)) return true;
        return false;
    };
    juce::String use = nm; int k = 2; while (taken (use)) use = nm + " " + juce::String (k++);
    resetAll (*this);
    for (auto& kv : p->getProperties()) setClamped (apvts, kv.name.toString(), (float) (double) kv.value);
    importedName = use;
    return saveUserPreset (use) ? juce::String() : juce::String ("Could not write the preset file.");
}

//==============================================================================
// State: all parameters, the preset name and the sound. A loaded file is remembered by its path; captured audio is stored inside the session (16 bit).
void HairballProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    auto state = apvts.copyState(); std::unique_ptr<juce::XmlElement> xml (state.createXml());
    xml->setAttribute ("format", 1); xml->setAttribute ("presetName", currentPresetName);
    xml->setAttribute ("tips", tipsOn.load() ? 1 : 0); xml->setAttribute ("powerSave", powerSave.load() ? 1 : 0); xml->setAttribute ("grains", showGrains.load() ? 1 : 0);
    if (editorWidth > 0) xml->setAttribute ("editorWidth", editorWidth);
    std::shared_ptr<const Source> s; { const juce::SpinLock::ScopedLockType sl (lock); s = source; }
    if (s != nullptr)
    {
        if (! s->captured) { xml->setAttribute ("samplePath", s->path); xml->setAttribute ("sampleName", s->name); }
        else if (s->l.size() <= 3000000)                           // about 60 s at 48 kHz; larger captures are not stored in the session
        {
            juce::MemoryBlock pcm (s->l.size() * 4);
            auto* d = static_cast<int16_t*> (pcm.getData());
            for (size_t i = 0; i < s->l.size(); ++i)
            {
                d[2 * i] = (int16_t) juce::roundToInt (juce::jlimit (-1.0f, 1.0f, s->l[i]) * 32767.0f);
                d[2 * i + 1] = (int16_t) juce::roundToInt (juce::jlimit (-1.0f, 1.0f, s->r[i]) * 32767.0f);
            }
            auto* c = xml->createNewChildElement ("CAPTURE");
            c->setAttribute ("sr", s->sr); c->setAttribute ("frames", (int) s->l.size()); c->setAttribute ("data", pcm.toBase64Encoding());
        }
    }
    copyXmlToBinary (*xml, dest);
}

void HairballProcessor::setStateInformation (const void* data, int size)
{
    if (size <= 0 || size > 32 * 1024 * 1024) return;
    std::unique_ptr<juce::XmlElement> xml (getXmlFromBinary (data, size));
    if (xml == nullptr || ! xml->hasTagName (apvts.state.getType())) return;
    currentPresetName = xml->getStringAttribute ("presetName", "Init").substring (0, 60);
    tipsOn = xml->getIntAttribute ("tips", 1) != 0; powerSave = xml->getIntAttribute ("powerSave", 0) != 0; showGrains = xml->getIntAttribute ("grains", 1) != 0;
    editorWidth = xml->getIntAttribute ("editorWidth", 0);
    for (auto* child : xml->getChildWithTagNameIterator ("PARAM")) setClamped (apvts, child->getStringAttribute ("id"), (float) child->getDoubleAttribute ("value"));
    if (auto* c = xml->getChildByName ("CAPTURE"))
    {
        const int frames = c->getIntAttribute ("frames"); const double csr = c->getDoubleAttribute ("sr", 48000.0);
        juce::MemoryBlock pcm; pcm.fromBase64Encoding (c->getStringAttribute ("data"));
        if (frames >= 64 && frames <= 3000000 && csr >= 8000.0 && csr <= 384000.0 && pcm.getSize() >= (size_t) frames * 4)
        {
            auto s = std::make_shared<Source>(); s->l.resize ((size_t) frames); s->r.resize ((size_t) frames);
            auto* d = static_cast<const int16_t*> (pcm.getData());
            for (int i = 0; i < frames; ++i) { s->l[(size_t) i] = (float) d[2 * i] / 32767.0f; s->r[(size_t) i] = (float) d[2 * i + 1] / 32767.0f; }
            s->sr = csr; s->name = "Captured audio"; s->captured = true; setSource (s);
        }
    }
    else if (xml->hasAttribute ("samplePath"))
    {
        const juce::File f (xml->getStringAttribute ("samplePath")); juce::String err;
        if (! loadAudioFile (f, err))
        {
            const juce::SpinLock::ScopedLockType sl (lock);
            missing = xml->getStringAttribute ("sampleName", f.getFileName());
        }
    }
}

juce::AudioProcessorEditor* HairballProcessor::createEditor() { return new HairballEditor (*this); }
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new HairballProcessor(); }
