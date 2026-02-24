#include "VJBPMSync.h"

//==============================================================================
void VJBPMSync::attach(BPMDetector& detector)
{
    detach();
    detector_ = &detector;
    lastBeatCount_ = detector_->getBeatCount();
    startTimerHz(120);   // poll at 120 Hz for responsive beat tracking
}

void VJBPMSync::detach()
{
    stopTimer();
    detector_ = nullptr;
}

//==============================================================================
void VJBPMSync::timerCallback()
{
    if (detector_ == nullptr) return;

    const int current = detector_->getBeatCount();
    while (lastBeatCount_ < current)
    {
        ++lastBeatCount_;
        handleBeat();
    }
}

//==============================================================================
void VJBPMSync::handleBeat()
{
    // Always fire the visual-pulse callback.
    if (onBeat)
        onBeat();

    if (!autoSwitch_ || sceneCount_ <= 1)
        return;

    ++beatCount_;
    if (beatCount_ >= beatsPerSwitch_)
    {
        beatCount_    = 0;
        currentScene_ = (currentScene_ + 1) % sceneCount_;
        if (onSceneSwitchRequested)
            onSceneSwitchRequested(currentScene_);
    }
}
