// The Hairball window. Layout, colours and wording follow docs/mockups/Hairball.html; it is drawn in a fixed design space
// (src/ui/Layout.h, generated from the mockup) and scaled to the window. Resizable from all four sides and corners with the
// aspect ratio locked; a double click on the wood goes back to 100%.
#pragma once
#include "PluginProcessor.h"
#include "ui/Widgets.h"
#include <catui/RenderGate.h>

class HairballEditor : public juce::AudioProcessorEditor, public juce::FileDragAndDropTarget
{
public:
    explicit HairballEditor (HairballProcessor&);
    ~HairballEditor() override;
    void resized() override;
    void paint (juce::Graphics& g) override { g.fillAll (hbui::col::ground); }

    bool isInterestedInFileDrag (const juce::StringArray&) override { return true; }
    void filesDropped (const juce::StringArray& files, int, int) override;
    void fileDragEnter (const juce::StringArray&, int, int) override;
    void fileDragExit (const juce::StringArray&) override;

private:
    struct Look; struct Content;
    void frame();
    void loadFile (const juce::File&);
    void refreshNames();
    void step (int dir);
    void openList();
    void saveDialog();
    void saveNamed (const juce::String&);
    void renameDialog();
    void deleteDialog();
    void exportDialog();
    void importDialog();
    void textDialog (const juce::String& title, const juce::String& hint, const juce::String& text, bool readOnly, const juce::String& ok,
                     std::function<juce::String (const juce::String&)> onOk);
    void showGuide();
    void showAbout();
    void showSettings();
    void cycleZoom();
    void restartTimer();

    HairballProcessor& proc;
    hbui::Shared shared;
    std::unique_ptr<Look> look;
    std::unique_ptr<Content> content;
    juce::TooltipWindow tooltip { this, 1000 };
    juce::ComponentBoundsConstrainer constrainer;
    juce::ResizableBorderComponent border { this, &constrainer };
    catui::RenderGate gate { *this };
    juce::StringArray names;           // factory first, then user presets
    int numFactory = 0;
    std::unique_ptr<juce::AlertWindow> dialog;
    std::unique_ptr<juce::FileChooser> chooser;
    juce::String note; double noteUntil = 0;
    double holdDownAt = 0; bool holdWasOn = false;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (HairballEditor)
};
