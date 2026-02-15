#include "PresetTemplates.h"

//==============================================================================
std::vector<PresetTemplates::Template> PresetTemplates::getBuiltInTemplates()
{
    std::vector<Template> templates;

    // ── 1.  EDM / Electronic Production ────────────────────────────────────
    {
        Template t;
        t.name = "EDM Production";
        t.description = "Full-range analysis for electronic music: spectrum, spectrogram, loudness, stereo field.";
        t.meters = {
            { MeterType::MultiBandAnalyzer,  10,  10, 500, 280 },
            { MeterType::Spectrogram,        10, 310, 500, 220 },
            { MeterType::LoudnessMeter,     530,  10, 300, 280 },
            { MeterType::Goniometer,        530, 310, 250, 250 },
            { MeterType::CorrelationMeter,  530, 570, 300,  30 },
            { MeterType::PeakMeter,         850,  10,  80, 520 },
            { MeterType::LevelHistogram,    850, 540, 300, 200 },
        };
        templates.push_back(std::move(t));
    }

    // ── 2.  Podcast / Voice ────────────────────────────────────────────────
    {
        Template t;
        t.name = "Podcast / Voice";
        t.description = "Loudness-focused layout for speech recording and mastering.";
        t.meters = {
            { MeterType::LoudnessMeter,      10,  10, 400, 350 },
            { MeterType::PeakMeter,         430,  10,  80, 350 },
            { MeterType::MultiBandAnalyzer,  10, 380, 500, 250 },
            { MeterType::CorrelationMeter,   10, 640, 500,  30 },
            { MeterType::LevelHistogram,    530,  10, 350, 300 },
        };
        templates.push_back(std::move(t));
    }

    // ── 3.  Lo-Fi / Vinyl ─────────────────────────────────────────────────
    {
        Template t;
        t.name = "Lo-Fi / Vinyl";
        t.description = "Retro Winamp-skinned meters with spectrogram and oscilloscope.";
        t.meters = {
            { MeterType::SkinnedSpectrum,      10,  10, 280, 100 },
            { MeterType::SkinnedOscilloscope,  10, 120, 280, 100 },
            { MeterType::SkinnedVUMeter,      310,  10, 120, 120 },
            { MeterType::SkinnedVUMeter,      310, 140, 120, 120 },
            { MeterType::Spectrogram,          10, 280, 420, 200 },
            { MeterType::LissajousScope,      450,  10, 250, 250 },
        };
        templates.push_back(std::move(t));
    }

    // ── 4.  Mastering ──────────────────────────────────────────────────────
    {
        Template t;
        t.name = "Mastering";
        t.description = "Precision layout for mastering: loudness, peaks, histogram, stereo.";
        t.meters = {
            { MeterType::LoudnessMeter,      10,  10, 350, 320 },
            { MeterType::PeakMeter,         380,  10,  80, 320 },
            { MeterType::MultiBandAnalyzer,  10, 350, 450, 250 },
            { MeterType::LevelHistogram,    480,  10, 350, 250 },
            { MeterType::Goniometer,        480, 280, 250, 250 },
            { MeterType::CorrelationMeter,  480, 540, 350,  30 },
            { MeterType::Spectrogram,        10, 610, 820, 180 },
        };
        templates.push_back(std::move(t));
    }

    // ── 5.  Minimal ────────────────────────────────────────────────────────
    {
        Template t;
        t.name = "Minimal";
        t.description = "Simple two-meter layout: spectrum analyzer and peak meter.";
        t.meters = {
            { MeterType::MultiBandAnalyzer,  10,  10, 600, 350 },
            { MeterType::PeakMeter,         630,  10,  80, 350 },
        };
        templates.push_back(std::move(t));
    }

    // ── 6.  Stereo Analysis ────────────────────────────────────────────────
    {
        Template t;
        t.name = "Stereo Analysis";
        t.description = "Focused on stereo imaging: goniometer, Lissajous, correlation.";
        t.meters = {
            { MeterType::Goniometer,         10,  10, 300, 300 },
            { MeterType::LissajousScope,    330,  10, 300, 300 },
            { MeterType::CorrelationMeter,   10, 320, 620,  30 },
            { MeterType::PeakMeter,         650,  10,  80, 340 },
            { MeterType::MultiBandAnalyzer,  10, 370, 720, 250 },
        };
        templates.push_back(std::move(t));
    }

    return templates;
}

//==============================================================================
int PresetTemplates::getNumTemplates()
{
    return static_cast<int>(getBuiltInTemplates().size());
}

PresetTemplates::Template PresetTemplates::getTemplate(int index)
{
    auto all = getBuiltInTemplates();
    if (index >= 0 && index < static_cast<int>(all.size()))
        return all[static_cast<size_t>(index)];
    return {};
}
