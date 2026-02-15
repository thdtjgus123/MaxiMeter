#pragma once

#include <JuceHeader.h>
#include "../Canvas/CanvasItem.h"
#include <vector>

//==============================================================================
/// Built-in preset templates for common workflows.
/// Each template describes a set of meter placements to populate a blank canvas.
class PresetTemplates
{
public:
    /// Describes one meter placement in a preset.
    struct MeterPlacement
    {
        MeterType type;
        float x, y, w, h;
    };

    /// A complete preset template.
    struct Template
    {
        juce::String              name;
        juce::String              description;
        std::vector<MeterPlacement> meters;
    };

    /// Get all built-in templates.
    static std::vector<Template> getBuiltInTemplates();

    /// Get template by index.
    static Template getTemplate(int index);

    /// Number of built-in templates.
    static int getNumTemplates();
};
