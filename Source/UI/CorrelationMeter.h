#pragma once

#include <JuceHeader.h>
#include "MeterBase.h"

//==============================================================================
/// CorrelationMeter — stereo phase correlation meter with bar + numeric display.
/// Range: -1 (fully anti-phase) to +1 (fully correlated).
/// Configurable integration time (50ms–5000ms).
class CorrelationMeter : public juce::Component,
                         public MeterBase
{
public:
    CorrelationMeter();
    ~CorrelationMeter() override = default;

    /// Set the current correlation value (-1..+1)
    void setCorrelation(float value);

    /// Configuration
    void setIntegrationTimeMs(float ms)  { integrationMs = juce::jlimit(50.0f, 5000.0f, ms); }
    void setOrientation(bool horizontal) { isHorizontal = horizontal; }
    void setShowNumeric(bool show)       { showNumeric = show; }

    // Getters for export/serialization
    float getIntegrationTimeMs() const { return integrationMs; }
    bool  getShowNumeric()       const { return showNumeric; }

    void paint(juce::Graphics& g) override;

private:
    float displayCorrelation = 0.0f;
    float targetCorrelation  = 0.0f;
    float integrationMs      = 300.0f;
    bool  isHorizontal       = true;
    bool  showNumeric        = true;

    float smoothCoeff = 0.1f;

    juce::Colour getCorrelationColour(float value) const;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(CorrelationMeter)
};
