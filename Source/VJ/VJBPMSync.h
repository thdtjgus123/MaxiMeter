#pragma once

#include <JuceHeader.h>
#include "../Audio/BPMDetector.h"
#include "VJTransitionEngine.h"

//==============================================================================
/// Listens to BPMDetector beats and fires scene-switch requests on a configurable
/// beat interval.  Owns no audio data — purely a beat-count → scene trigger.
class VJBPMSync : private juce::Timer
{
public:
    VJBPMSync() = default;
    ~VJBPMSync() override { stopTimer(); }

    //==========================================================================
    /// Connect to a BPMDetector.  Call once, before enabling auto-switch.
    void attach(BPMDetector& detector);

    /// Detach from current detector (if any).
    void detach();

    //==========================================================================
    void setAutoSwitch(bool enabled)          { autoSwitch_ = enabled; beatCount_ = 0; }
    bool isAutoSwitchEnabled()   const        { return autoSwitch_; }

    /// How many beats between auto scene switches (default = 16).
    void setBeatsPerSwitch(int beats)         { beatsPerSwitch_ = juce::jmax(1, beats); }
    int  getBeatsPerSwitch()     const        { return beatsPerSwitch_; }

    /// Scene count must be kept in sync with VJSceneManager (set each time scenes change).
    void setSceneCount(int n)                 { sceneCount_ = juce::jmax(1, n); }
    int  getSceneCount()         const        { return sceneCount_; }

    /// Current beat phase: 0..1 within one beat period.
    float getBeatPhase()         const        { return detector_ ? detector_->getBeatPhase() : 0.f; }

    /// Live BPM (0 if no detector connected).
    float getBPM()               const        { return detector_ ? detector_->getBPM() : 0.f; }

    //==========================================================================
    /// Called with the index of the NEXT scene that should be loaded.
    std::function<void(int nextSceneIndex)> onSceneSwitchRequested;

    /// Called on every beat (even when auto-switch is disabled) — useful for
    /// visual pulse effects in the control panel.
    std::function<void()> onBeat;

private:
    void handleBeat();
    void timerCallback() override;

    BPMDetector*  detector_      = nullptr;
    bool          autoSwitch_    = false;
    int           beatsPerSwitch_= 16;
    int           beatCount_     = 0;
    int           sceneCount_    = 1;
    int           currentScene_  = 0;
    int           lastBeatCount_ = 0;  ///< tracks BPMDetector::getBeatCount()
};
