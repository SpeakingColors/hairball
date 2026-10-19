// Hairball plugin processor: the pure C++ engine (src/core) wrapped for JUCE. Every one of the 57 engine parameters
// is a host parameter, built from the table in HairballParams.h, so nothing can drift from the engine or the manual.
#pragma once
#include <juce_audio_utils/juce_audio_utils.h>
#include <atomic>
#include <memory>
#include "core/HairballCore.h"

#ifndef HB_FX_BUILD
 #define HB_FX_BUILD 0
#endif

class HairballProcessor : public juce::AudioProcessor, private juce::Timer
{
public:
    static constexpr bool kFxBuild = HB_FX_BUILD != 0;       // the "Hairball FX" build opens in FX mode, the "Hairball" build in Instrument mode

    HairballProcessor();
    ~HairballProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using juce::AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 8.0; }   // longest amp release; a frozen sound keeps playing while the host runs

    int getNumPrograms() override;
    int getCurrentProgram() override { return currentFactory; }
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    // ---- user presets: one XML file per sound in <app data>/Studio 87/Hairball/presets ----
    static juce::File userPresetDir();
    juce::StringArray listUserPresets() const;
    bool saveUserPreset (const juce::String& name);
    bool renameUserPreset (const juce::String& from, const juce::String& to);
    bool deleteUserPreset (const juce::String& name);
    bool loadUserPreset (const juce::String& name);
    // text form for Export / Import: {"hairball":1,"name":...,"p":{id:value}}; import is validated and returns an error text, empty on success
    juce::String presetAsText() const;
    juce::String importPresetText (const juce::String& text, juce::String& importedName);

    // ---- the sound (message thread) ----
    bool loadAudioFile (const juce::File&, juce::String& error);
    void requestCapture();                                   // FX mode: turn what the buffer holds into the sample
    juce::String sampleName() const;
    juce::String missingSample() const;                      // name of a file the saved session used but that was not found
    struct Peaks { juce::String name; double seconds = 0; std::vector<float> lo, hi; std::vector<float> marks; };
    std::shared_ptr<const Peaks> peaks() const;
    uint32_t sampleSerial() const { return serial.load(); }
    juce::SpinLock displayLock;                              // held while the engine reallocates its buffers (prepareToPlay); the editor only draws when it can take it

    // ---- shared with the editor ----
    juce::AudioProcessorValueTreeState apvts;
    juce::MidiKeyboardState keyState;                        // on-screen piano, mixed into the MIDI input
    juce::String currentPresetName { "Init" };
    int editorWidth = 0;                                     // remembered while the host session runs

    struct Live                                              // written by the audio thread, read by the editor (relaxed atomics, display only)
    {
        std::atomic<float> peakL { 0 }, peakR { 0 }, rmsL { 0 }, rmsR { 0 }, riser { 0 }, grDb { 0 }, cpu { 0 };
        std::atomic<int> voices { 0 }, grains { 0 }, nHeads { 0 }, nMarks { 0 };
        std::atomic<bool> frozen { false };
        std::atomic<float> heads[8], marks[192];
        Live() { for (auto& h : heads) h = 0.0f; for (auto& m : marks) m = 0.0f; }
    } live;

    // editor settings that belong to this instance
    std::atomic<bool> tipsOn { true }, powerSave { false }, showGrains { true };

    const hb::Engine& engineForDisplay() const { return engine; }
    double sampleRateNow() const { return sr; }

private:
    void timerCallback() override;
    struct Source;
    void setSource (std::shared_ptr<const Source>);
    void submitSource();
    void applyFactory (int index);
    static juce::String sanitize (const juce::String&);
    void readParams();
    void retire (hb::Engine::Sample*);

    hb::Engine engine;
    hb::Params hp;
    std::atomic<float>* raw[hb::pCount] {};
    double sr = 48000.0;
    int currentFactory = 0;

    // sample hand-over between threads: built on the message thread, swapped in by the audio thread, freed on the message thread
    std::atomic<hb::Engine::Sample*> pending { nullptr };
    std::atomic<hb::Engine::Sample*> retired[4] {};
    std::atomic<uint32_t> serial { 0 };
    std::atomic<bool> captureReq { false }, captureReady { false };
    hb::Engine::Buf capBuf;

    mutable juce::SpinLock lock;                             // guards the shared_ptrs below, never taken by the audio thread
    std::shared_ptr<const Source> source;
    std::shared_ptr<const Peaks> peaksPtr;
    juce::String missing;
    juce::AudioFormatManager formats;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (HairballProcessor)
};
