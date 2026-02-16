#pragma once

#include <JuceHeader.h>
#include <deque>
#include "MeterBase.h"

//==============================================================================
/// LoudnessMeter — EBU R128 / ITU-R BS.1770-4 loudness display.
/// Shows Momentary, Short-term, Integrated LUFS + LRA + True Peak.
/// Includes target reference line and color-coded zones.
class LoudnessMeter : public juce::Component,
                      public MeterBase
{
public:
    LoudnessMeter();
    ~LoudnessMeter() override = default;

    /// Set current loudness values (call from GUI timer)
    void setMomentaryLUFS(float lufs)   { momentary = lufs; repaint(); }
    void setShortTermLUFS(float lufs)   { shortTerm = lufs; repaint(); }
    void setIntegratedLUFS(float lufs)  { integrated = lufs; }
    void setLRA(float value)            { lra = value; }
    void setTruePeakL(float tp)         { truePeakL = tp; }
    void setTruePeakR(float tp)         { truePeakR = tp; }
    void setTargetLUFS(float target)    { targetLUFS = target; repaint(); }

    /// Configuration
    void setDisplayRange(float minLUFS, float maxLUFS) { minRange = minLUFS; maxRange = maxLUFS; }
    void setShowHistory(bool show)      { showHistory = show; }

    // Getters for export/serialization
    float getTargetLUFS()  const { return targetLUFS; }
    bool  getShowHistory() const { return showHistory; }

    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    float momentary  = -100.0f;
    float shortTerm  = -100.0f;
    float integrated = -100.0f;
    float lra        = 0.0f;
    float truePeakL  = 0.0f;
    float truePeakR  = 0.0f;
    float targetLUFS = -14.0f;

    float minRange = -50.0f;
    float maxRange = 0.0f;
    bool  showHistory = true;

    // Scrolling short-term history (deque, one sample per frame)
    static constexpr int kHistoryMaxLen = 1800;  // 30 s * 60 fps
    std::deque<float> shortTermHistory;
    int historyFrameDiv = 0;              // push every N-th paint

    float lufsToNormalized(float lufs) const;
    juce::Colour lufsToColour(float lufs) const;

    void drawMeterBar(juce::Graphics& g, juce::Rectangle<int> area, float value,
                      const juce::String& label, bool showTarget);
    void drawHistoryGraph(juce::Graphics& g, juce::Rectangle<int> area);
    void drawInfoPanel(juce::Graphics& g, juce::Rectangle<int> area);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LoudnessMeter)
};
