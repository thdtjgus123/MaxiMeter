#include "EqualizerPanel.h"

using SID = Skin::SpriteID;

//==============================================================================
static const char* kBandLabels[EqualizerPanel::kNumBands] = {
    "60", "170", "310", "600", "1K", "3K", "6K", "12K", "14K", "16K"
};

//==============================================================================
EqualizerPanel::EqualizerPanel()
{
    setSize(275 * scale, 116 * scale);
    bandGains.fill(0.0f);
}

//==============================================================================
void EqualizerPanel::setSkinModel(const Skin::SkinModel* model)
{
    skin = model;
    repaint();
}

void EqualizerPanel::setScale(int s)
{
    scale = juce::jlimit(1, 4, s);
    setSize(275 * scale, 116 * scale);
    repaint();
}

void EqualizerPanel::setPreamp(float db)
{
    preampDb = juce::jlimit(-12.0f, 12.0f, db);
    repaint();
}

float EqualizerPanel::getBandGain(int band) const
{
    if (band < 0 || band >= kNumBands) return 0.0f;
    return bandGains[static_cast<size_t>(band)];
}

void EqualizerPanel::setBandGain(int band, float db)
{
    if (band < 0 || band >= kNumBands) return;
    bandGains[static_cast<size_t>(band)] = juce::jlimit(-12.0f, 12.0f, db);
    repaint();
}

void EqualizerPanel::resetToFlat()
{
    preampDb = 0.0f;
    bandGains.fill(0.0f);
    repaint();
}

//==============================================================================
void EqualizerPanel::paint(juce::Graphics& g)
{
    float sx = static_cast<float>(getWidth())  / 275.0f;
    float sy = static_cast<float>(getHeight()) / 116.0f;
    g.addTransform(juce::AffineTransform::scale(sx, sy));

    if (hasSkin())
        drawSkinned(g);
    else
        drawOriginal(g);
}

//==============================================================================
void EqualizerPanel::drawSkinned(juce::Graphics& g) const
{
    // 1. Background — already contains slider wells/tracks baked in
    drawSprite(g, SID::EqBackground, 0, 0);

    // 2. Title bar (overlays top 14px of background)
    drawSprite(g, SID::EqTitleBarSelected, 0, 0);

    // 3. Close button
    drawSprite(g, SID::EqCloseButton, 264, 3);

    // 4. ON button
    {
        bool pressed = (pressedZone == HitZone::OnButton);
        SID id;
        if (eqOn)
            id = pressed ? SID::EqOnButtonSelectedPressed : SID::EqOnButtonSelected;
        else
            id = pressed ? SID::EqOnButtonPressed : SID::EqOnButton;
        drawSprite(g, id, 14, 18);
    }

    // 5. AUTO button
    {
        bool pressed = (pressedZone == HitZone::AutoButton);
        SID id;
        if (autoOn)
            id = pressed ? SID::EqAutoButtonSelectedPressed : SID::EqAutoButtonSelected;
        else
            id = pressed ? SID::EqAutoButtonPressed : SID::EqAutoButton;
        drawSprite(g, id, 39, 18);
    }

    // 6. Presets button
    {
        bool pressed = (pressedZone == HitZone::PresetsButton);
        drawSprite(g, pressed ? SID::EqPresetsButtonSelected : SID::EqPresetsButton, 217, 18);
    }

    // 7. EQ graph background + response curve
    drawSprite(g, SID::EqGraphBg, 86, 17);
    drawEqGraph(g, graphRect().toFloat());

    // 8. Slider background gradient fills (covers the U-shaped grooves in EqBackground)
    //    EqSliderBg (209x129) has 11 columns of 19px each (preamp + 10 bands).
    //    Vertically, it's a tall gradient: for slider position p (0-27),
    //    we take a 63-px tall window starting at srcY = p * 66 / 27.
    {
        auto sliderBgImg = skin->getSprite(SID::EqSliderBg);
        if (sliderBgImg.isValid())
        {
            auto drawSliderBg = [&](juce::Rectangle<int> trackR, float db, int col)
            {
                int offset = dbToThumbOffset(db);
                int frame  = juce::jlimit(0, 27, 27 - offset * 27 / kTravel);
                int srcX   = col * 19;
                int srcY   = frame * (sliderBgImg.getHeight() - kTrackH) / 27;
                int srcW   = juce::jmin(kTrackW, sliderBgImg.getWidth() - srcX);
                int srcH   = juce::jmin(kTrackH, sliderBgImg.getHeight() - srcY);

                if (srcW > 0 && srcH > 0)
                {
                    auto clipped = sliderBgImg.getClippedImage(
                        juce::Rectangle<int>(srcX, srcY, srcW, srcH));
                    if (clipped.isValid())
                        g.drawImage(clipped,
                            juce::Rectangle<float>(
                                static_cast<float>(trackR.getX()),
                                static_cast<float>(trackR.getY()),
                                static_cast<float>(trackR.getWidth()),
                                static_cast<float>(trackR.getHeight())));
                }
            };

            drawSliderBg(preampSliderRect(), preampDb, 0);
            for (int i = 0; i < kNumBands; ++i)
                drawSliderBg(bandSliderRect(i), bandGains[static_cast<size_t>(i)], i + 1);
        }
    }

    // 9. Slider thumbs
    {
        auto thumbImg = skin->getSprite(SID::EqSliderThumb);
        auto thumbSelImg = skin->getSprite(SID::EqSliderThumbSelected);

        auto drawThumb = [&](juce::Rectangle<int> trackR, float db, HitZone zone)
        {
            int offset = dbToThumbOffset(db);
            // Thumb is centered horizontally in the track, positioned vertically by offset
            int thumbX = trackR.getX() + (trackR.getWidth() - kThumbW) / 2;
            int thumbY = trackR.getY() + offset;
            bool sel = (pressedZone == zone);
            auto& img = sel ? thumbSelImg : thumbImg;
            if (img.isValid())
            {
                g.drawImage(img,
                    juce::Rectangle<float>(static_cast<float>(thumbX),
                                           static_cast<float>(thumbY),
                                           static_cast<float>(kThumbW),
                                           static_cast<float>(kThumbH)));
            }
        };

        drawThumb(preampSliderRect(), preampDb, HitZone::Preamp);
        for (int i = 0; i < kNumBands; ++i)
        {
            auto zone = static_cast<HitZone>(static_cast<int>(HitZone::Band0) + i);
            drawThumb(bandSliderRect(i), bandGains[static_cast<size_t>(i)], zone);
        }
    }
}

//==============================================================================
void EqualizerPanel::drawOriginal(juce::Graphics& g) const
{
    // Modern dark EQ panel
    g.setColour(juce::Colour(0xFF1A1A2E));
    g.fillRect(0.0f, 0.0f, 275.0f, 116.0f);

    // Title bar
    g.setColour(juce::Colour(0xFF0E1226));
    g.fillRect(0.0f, 0.0f, 275.0f, 14.0f);
    g.setColour(juce::Colours::white.withAlpha(0.7f));
    g.setFont(9.0f);
    g.drawText("EQUALIZER", juce::Rectangle<float>(0, 0, 275, 14), juce::Justification::centred);

    // ON / AUTO buttons
    auto drawBtn = [&](juce::Rectangle<int> r, const juce::String& text, bool active, bool pressed)
    {
        auto rf = r.toFloat();
        if (active)
            g.setColour(pressed ? juce::Colour(0xFF2060FF) : juce::Colour(0xFF3A7BFF));
        else
            g.setColour(pressed ? juce::Colour(0xFF333355) : juce::Colour(0xFF222244));
        g.fillRoundedRectangle(rf, 2.0f);
        g.setColour(active ? juce::Colours::white : juce::Colours::white.withAlpha(0.5f));
        g.setFont(7.0f);
        g.drawText(text, rf, juce::Justification::centred);
    };

    drawBtn(onBtnRect(), "ON", eqOn, pressedZone == HitZone::OnButton);
    drawBtn(autoBtnRect(), "AUTO", autoOn, pressedZone == HitZone::AutoButton);
    drawBtn(presetBtnRect(), "PRESETS", false, pressedZone == HitZone::PresetsButton);

    // EQ graph
    {
        auto gr = graphRect().toFloat();
        g.setColour(juce::Colour(0xFF0D0D1A));
        g.fillRect(gr);
        g.setColour(juce::Colours::white.withAlpha(0.1f));
        // Horizontal center line (0 dB)
        g.drawHorizontalLine(static_cast<int>(gr.getCentreY()), gr.getX(), gr.getRight());
        drawEqGraph(g, gr);
    }

    // Slider tracks and thumbs
    auto drawSlider = [&](juce::Rectangle<int> r, float db, HitZone zone, const char* label)
    {
        auto rf = r.toFloat();
        // Track background
        float trackX = rf.getCentreX() - 1.5f;
        g.setColour(juce::Colour(0xFF333355));
        g.fillRoundedRectangle(trackX, rf.getY(), 3.0f, rf.getHeight(), 1.5f);

        // Center line
        g.setColour(juce::Colours::white.withAlpha(0.15f));
        float cy = rf.getCentreY();
        g.drawHorizontalLine(static_cast<int>(cy), rf.getX(), rf.getRight());

        // Gradient fill from center to thumb
        int offset = dbToThumbOffset(db);
        float thumbCenterY = rf.getY() + static_cast<float>(offset) + static_cast<float>(kThumbH) / 2.0f;
        if (std::abs(db) > 0.5f)
        {
            auto fillColour = (db > 0) ? juce::Colour(0xFF3A7BFF) : juce::Colour(0xFF00C896);
            float top = juce::jmin(thumbCenterY, cy);
            float bot = juce::jmax(thumbCenterY, cy);
            g.setColour(fillColour.withAlpha(0.4f));
            g.fillRoundedRectangle(trackX, top, 3.0f, bot - top, 1.5f);
        }

        // Thumb
        bool sel = (pressedZone == zone);
        g.setColour(sel ? juce::Colour(0xFF5599FF) : juce::Colour(0xFFCCDDFF));
        g.fillRoundedRectangle(rf.getX(), thumbCenterY - 4.0f, rf.getWidth(), 8.0f, 3.0f);

        // Label below
        if (label != nullptr)
        {
            g.setColour(juce::Colours::white.withAlpha(0.4f));
            g.setFont(6.0f);
            g.drawText(label, juce::Rectangle<float>(rf.getX() - 3, rf.getBottom() + 1, rf.getWidth() + 6, 8),
                       juce::Justification::centred, false);
        }
    };

    drawSlider(preampSliderRect(), preampDb, HitZone::Preamp, "PRE");
    for (int i = 0; i < kNumBands; ++i)
    {
        auto zone = static_cast<HitZone>(static_cast<int>(HitZone::Band0) + i);
        drawSlider(bandSliderRect(i), bandGains[static_cast<size_t>(i)], zone, kBandLabels[i]);
    }
}

//==============================================================================
void EqualizerPanel::drawSprite(juce::Graphics& g, Skin::SpriteID id, int nx, int ny) const
{
    if (!skin) return;
    auto img = skin->getSprite(id);
    if (!img.isValid()) return;
    g.drawImage(img,
        juce::Rectangle<float>(
            static_cast<float>(nx),
            static_cast<float>(ny),
            static_cast<float>(img.getWidth()),
            static_cast<float>(img.getHeight())));
}

//==============================================================================
void EqualizerPanel::drawEqGraph(juce::Graphics& g, juce::Rectangle<float> area) const
{
    // Draw a smooth spline through all band gain points
    juce::Path path;
    float w = area.getWidth();
    float h = area.getHeight();

    for (int i = 0; i < kNumBands; ++i)
    {
        float xNorm = static_cast<float>(i) / static_cast<float>(kNumBands - 1);
        float yNorm = (12.0f - bandGains[static_cast<size_t>(i)]) / 24.0f;  // 0 = +12dB top, 1 = -12dB bottom
        float px = area.getX() + xNorm * w;
        float py = area.getY() + yNorm * h;

        if (i == 0)
            path.startNewSubPath(px, py);
        else
            path.lineTo(px, py);
    }

    // Draw the spline
    if (hasSkin())
    {
        // Use EQ graph line colors from skin if available
        auto lineImg = skin->getSprite(SID::EqGraphLineColors);
        if (lineImg.isValid() && lineImg.getHeight() > 0)
        {
            // Sample color from middle of the line color strip
            auto col = juce::Colour(lineImg.getPixelAt(0, lineImg.getHeight() / 2));
            g.setColour(col);
        }
        else
        {
            g.setColour(juce::Colour(0xFF00FF00));
        }
    }
    else
    {
        g.setColour(juce::Colour(0xFF3A7BFF));
    }

    g.strokePath(path, juce::PathStrokeType(1.5f));
}

//==============================================================================
EqualizerPanel::HitZone EqualizerPanel::hitTest(juce::Point<int> nativePos) const
{
    if (onBtnRect().contains(nativePos))      return HitZone::OnButton;
    if (autoBtnRect().contains(nativePos))    return HitZone::AutoButton;
    if (presetBtnRect().contains(nativePos))  return HitZone::PresetsButton;
    if (closeBtnRect().contains(nativePos))   return HitZone::Close;

    // Preamp slider (expand hit area slightly)
    if (preampSliderRect().expanded(4, 2).contains(nativePos))
        return HitZone::Preamp;

    // Band sliders
    for (int i = 0; i < kNumBands; ++i)
    {
        if (bandSliderRect(i).expanded(4, 2).contains(nativePos))
            return static_cast<HitZone>(static_cast<int>(HitZone::Band0) + i);
    }

    return HitZone::None;
}

//==============================================================================
void EqualizerPanel::mouseDown(const juce::MouseEvent& e)
{
    auto np = toNative(e.getPosition());
    pressedZone = hitTest(np);
    isDragging = false;

    // Start drag immediately for sliders
    if (pressedZone == HitZone::Preamp ||
        (pressedZone >= HitZone::Band0 && pressedZone <= HitZone::Band9))
    {
        isDragging = true;
        handleSliderDrag(np);
    }

    repaint();
}

void EqualizerPanel::mouseUp(const juce::MouseEvent& e)
{
    auto np = toNative(e.getPosition());
    auto zone = hitTest(np);

    if (zone == pressedZone && !isDragging)
    {
        // Button clicks
        if (zone == HitZone::OnButton)
        {
            eqOn = !eqOn;
            if (onEqToggled) onEqToggled(eqOn);
        }
        else if (zone == HitZone::AutoButton)
        {
            autoOn = !autoOn;
            if (onAutoToggled) onAutoToggled(autoOn);
        }
        else if (zone == HitZone::PresetsButton)
        {
            resetToFlat(); // simple preset: flat
        }
    }

    pressedZone = HitZone::None;
    isDragging = false;
    repaint();
}

void EqualizerPanel::mouseDrag(const juce::MouseEvent& e)
{
    if (pressedZone == HitZone::Preamp ||
        (pressedZone >= HitZone::Band0 && pressedZone <= HitZone::Band9))
    {
        isDragging = true;
        handleSliderDrag(toNative(e.getPosition()));
    }
}

void EqualizerPanel::mouseMove(const juce::MouseEvent&)
{
    // Could update cursor — not required
}

//==============================================================================
void EqualizerPanel::handleSliderDrag(juce::Point<int> nativePos)
{
    if (pressedZone == HitZone::Preamp)
    {
        auto r = preampSliderRect();
        int relY = nativePos.y - r.getY();
        int offset = juce::jlimit(0, kTravel, relY - kThumbH / 2);
        float db = thumbOffsetToDb(offset);
        preampDb = db;
        if (onPreampChanged) onPreampChanged(db);
    }
    else if (pressedZone >= HitZone::Band0 && pressedZone <= HitZone::Band9)
    {
        int band = static_cast<int>(pressedZone) - static_cast<int>(HitZone::Band0);
        auto r = bandSliderRect(band);
        int relY = nativePos.y - r.getY();
        int offset = juce::jlimit(0, kTravel, relY - kThumbH / 2);
        float db = thumbOffsetToDb(offset);
        bandGains[static_cast<size_t>(band)] = db;
        if (onBandChanged) onBandChanged(band, db);
    }

    repaint();
}
