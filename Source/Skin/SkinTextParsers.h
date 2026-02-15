#pragma once

#include "SkinModel.h"

//==============================================================================
/// Parsers for Winamp skin text configuration files.
namespace SkinTextParsers
{
    /// Parse viscolor.txt — 24 RGB colors, one per line.
    /// Format: "R,G,B" or "R G B" per line. Lines starting with // are comments.
    Skin::VisColors parseVisColors(const juce::String& text);

    /// Parse region.txt — INI-style polygon regions.
    /// Sections: [Normal], [WindowShade], [Equalizer]
    /// Keys: NumPoints=N, PointList=x1,y1,x2,y2,...
    Skin::RegionData parseRegion(const juce::String& text);

    /// Parse pledit.txt — INI-style playlist editor config.
    /// Section: [Text]
    /// Keys: Normal=#RRGGBB, Current=#RRGGBB, NormalBG=#RRGGBB, SelectedBG=#RRGGBB, Font=name
    Skin::PleditConfig parsePledit(const juce::String& text);
}
