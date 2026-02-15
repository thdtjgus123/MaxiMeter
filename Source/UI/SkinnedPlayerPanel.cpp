#include "SkinnedPlayerPanel.h"

using SID = Skin::SpriteID;

//==============================================================================
SkinnedPlayerPanel::SkinnedPlayerPanel()
{
    setSize(275 * scale, 116 * scale);
    startTimerHz(30);
}

SkinnedPlayerPanel::~SkinnedPlayerPanel()
{
    stopTimer();
}

//==============================================================================
void SkinnedPlayerPanel::setSkinModel(const Skin::SkinModel* model)
{
    skin = model;
    if (skin)
        fontRenderer.setSkin(skin);
    repaint();
}

void SkinnedPlayerPanel::setScale(int s)
{
    scale = juce::jlimit(1, 4, s);
    setSize(275 * scale, 116 * scale);
    repaint();
}

void SkinnedPlayerPanel::setTime(int m, int s)
{
    timeMinutes = m;
    timeSeconds = s;
}

void SkinnedPlayerPanel::setTitleText(const juce::String& t)
{
    if (titleText != t)
    {
        titleText = t;
        scrollOffset = 0;
    }
}

void SkinnedPlayerPanel::setPlayState(PlayState s)  { playState = s; }
void SkinnedPlayerPanel::setMonoStereo(bool s)      { stereoMode = s; }

void SkinnedPlayerPanel::setPosition(double v)
{
    if (!isDraggingPosition)
        positionValue = juce::jlimit(0.0, 1.0, v);
}

void SkinnedPlayerPanel::setVolume(double v)
{
    if (!isDraggingVolume)
        volumeValue = juce::jlimit(0.0, 1.0, v);
}

void SkinnedPlayerPanel::setBalance(double v)
{
    if (!isDraggingBalance)
        balanceValue = juce::jlimit(0.0, 1.0, v);
}

void SkinnedPlayerPanel::setSpectrumData(const float* data, int n)
{
    int c = juce::jmin(n, 20);
    for (int i = 0; i < c; ++i)
        specBands[static_cast<size_t>(i)] = data[i];
}

void SkinnedPlayerPanel::setOscilloscopeData(const float* data, int n)
{
    oscSampleCount = juce::jmin(n, 512);
    std::memcpy(oscSamples.data(), data, sizeof(float) * static_cast<size_t>(oscSampleCount));
}

//==============================================================================
void SkinnedPlayerPanel::timerCallback()
{
    int textWidth = fontRenderer.getTextWidth(titleText, 1);
    int displayWidth = 154;
    if (textWidth > displayWidth)
    {
        scrollOffset += 1;
        if (scrollOffset > textWidth + 30)
            scrollOffset = 0;
    }
    else
    {
        scrollOffset = 0;
    }
    repaint();
}

//==============================================================================
// PAINTING
//==============================================================================
void SkinnedPlayerPanel::paint(juce::Graphics& g)
{
    if (!hasSkin())
    {
        g.fillAll(juce::Colour(0xFF1A1A2E));
        return;
    }

    // Scale from native 275×116 space to actual component size
    float sx = static_cast<float>(getWidth())  / 275.0f;
    float sy = static_cast<float>(getHeight()) / 116.0f;
    g.addTransform(juce::AffineTransform::scale(sx, sy));

    drawBackground(g);
    drawTitleBar(g);
    drawTimeDisplay(g);
    drawScrollingTitle(g);
    drawPlayStateIndicator(g);
    drawMonoStereo(g);
    drawVisualization(g);
    drawVolumeSlider(g);
    drawBalanceSlider(g);
    drawEqPlToggles(g);
    drawPositionBar(g);
    drawTransportButtons(g);
    drawShuffleRepeat(g);
}

void SkinnedPlayerPanel::drawSprite(juce::Graphics& g, SID id, int nx, int ny) const
{
    auto img = skin->getSprite(id);
    if (!img.isValid()) return;
    g.drawImage(img,
        juce::Rectangle<float>(
            static_cast<float>(nx),
            static_cast<float>(ny),
            static_cast<float>(img.getWidth()),
            static_cast<float>(img.getHeight())));
}

void SkinnedPlayerPanel::drawBackground(juce::Graphics& g) const
{
    auto img = skin->getSprite(SID::MainBackground);
    if (img.isValid())
        g.drawImage(img, juce::Rectangle<float>(0, 0, 275, 116),
                    juce::RectanglePlacement::stretchToFit);
}

void SkinnedPlayerPanel::drawTitleBar(juce::Graphics& g) const
{
    drawSprite(g, SID::TitleBarSelected, 0, 0);

    // Window buttons
    bool minPressed = (pressedZone == HitZone::Minimize);
    bool shdPressed = (pressedZone == HitZone::Shade);
    bool clsPressed = (pressedZone == HitZone::Close);

    drawSprite(g, minPressed ? SID::MinimizeButtonPressed : SID::MinimizeButton, 244, 3);
    drawSprite(g, shdPressed ? SID::ShadeButtonPressed    : SID::ShadeButton,    254, 3);
    drawSprite(g, clsPressed ? SID::CloseButtonPressed    : SID::CloseButton,    264, 3);
}

void SkinnedPlayerPanel::drawTimeDisplay(juce::Graphics& g) const
{
    fontRenderer.drawTime(g, timeMinutes, timeSeconds, 48, 26, 1);
}

void SkinnedPlayerPanel::drawScrollingTitle(juce::Graphics& g) const
{
    juce::Rectangle<int> area(111, 27, 154, 6);
    fontRenderer.drawScrollingText(g, titleText, area, scrollOffset, 1);
}

void SkinnedPlayerPanel::drawPlayStateIndicator(juce::Graphics& g) const
{
    SID id;
    switch (playState)
    {
        case PlayState::Playing: id = SID::PlayingIndicator; break;
        case PlayState::Paused:  id = SID::PausedIndicator;  break;
        default:                 id = SID::StoppedIndicator;  break;
    }
    drawSprite(g, id, 24, 28);
}

void SkinnedPlayerPanel::drawMonoStereo(juce::Graphics& g) const
{
    drawSprite(g, stereoMode ? SID::StereoActive : SID::StereoInactive, 212, 41);
    drawSprite(g, stereoMode ? SID::MonoInactive : SID::MonoActive, 239, 41);
}

//--- Transport Buttons ---
void SkinnedPlayerPanel::drawTransportButtons(juce::Graphics& g) const
{
    bool pr = (pressedZone == HitZone::Prev);
    bool pl = (pressedZone == HitZone::Play);
    bool pa = (pressedZone == HitZone::Pause);
    bool st = (pressedZone == HitZone::Stop);
    bool nx = (pressedZone == HitZone::Next);
    bool ej = (pressedZone == HitZone::Eject);

    drawSprite(g, pr ? SID::PreviousButtonPressed : SID::PreviousButton, 16, kBtnY);
    drawSprite(g, pl ? SID::PlayButtonPressed     : SID::PlayButton,     39, kBtnY);
    drawSprite(g, pa ? SID::PauseButtonPressed    : SID::PauseButton,    62, kBtnY);
    drawSprite(g, st ? SID::StopButtonPressed     : SID::StopButton,     85, kBtnY);
    drawSprite(g, nx ? SID::NextButtonPressed     : SID::NextButton,    108, kBtnY);
    drawSprite(g, ej ? SID::EjectButtonPressed    : SID::EjectButton,   136, kBtnY);
}

//--- Position Bar ---
void SkinnedPlayerPanel::drawPositionBar(juce::Graphics& g) const
{
    // Background: 248×10 at (16, 72)
    drawSprite(g, SID::PositionBackground, 16, 72);

    // Thumb: 29×10, positioned by positionValue along 248-29 = 219 px range
    int thumbRange = 248 - 29;
    int thumbX = 16 + static_cast<int>(positionValue * thumbRange);
    bool pressed = isDraggingPosition;
    drawSprite(g, pressed ? SID::PositionThumbSelected : SID::PositionThumb, thumbX, 72);
}

//--- Volume Slider ---
void SkinnedPlayerPanel::drawVolumeSlider(juce::Graphics& g) const
{
    // Volume background: a 68×420 strip of 28 frames each 68×15
    // Pick the frame that matches current volume (0..27)
    int frameIdx = static_cast<int>(volumeValue * 27.0);
    frameIdx = juce::jlimit(0, 27, frameIdx);

    // Each frame is 68×15 in the strip; we need to clip from the full volume bitmap
    if (skin->hasBitmap(Skin::SkinBitmap::Volume))
    {
        auto& volBmp = skin->bitmaps[static_cast<size_t>(Skin::SkinBitmap::Volume)];
        if (volBmp.isValid())
        {
            auto frame = volBmp.getClippedImage(juce::Rectangle<int>(0, frameIdx * 15, 68, 15));
            g.drawImage(frame,
                juce::Rectangle<float>(107.0f, 57.0f, 68.0f, 15.0f));
        }
    }

    // Thumb: 14×11, range is 0..51 px (68 - 14 - 3)
    int thumbRange = 68 - 14;
    int thumbX = 107 + static_cast<int>(volumeValue * thumbRange);
    bool pressed = isDraggingVolume;
    drawSprite(g, pressed ? SID::VolumeThumbSelected : SID::VolumeThumb, thumbX, 59);
}

//--- Balance Slider ---
void SkinnedPlayerPanel::drawBalanceSlider(juce::Graphics& g) const
{
    // Balance background: 38×420 strip at x=9 within balance.bmp; 28 frames of 38×15
    // Frame 0 = full left, 14 = center, 27 = full right
    int frameIdx = static_cast<int>(balanceValue * 27.0);
    frameIdx = juce::jlimit(0, 27, frameIdx);

    if (skin->hasBitmap(Skin::SkinBitmap::Balance))
    {
        auto& balBmp = skin->bitmaps[static_cast<size_t>(Skin::SkinBitmap::Balance)];
        if (balBmp.isValid())
        {
            auto frame = balBmp.getClippedImage(juce::Rectangle<int>(9, frameIdx * 15, 38, 15));
            g.drawImage(frame,
                juce::Rectangle<float>(177.0f, 57.0f, 38.0f, 15.0f));
        }
    }

    // Thumb: 14×11, range is 0..24 px (38 - 14)
    int thumbRange = 38 - 14;
    int thumbX = 177 + static_cast<int>(balanceValue * thumbRange);
    bool pressed = isDraggingBalance;
    drawSprite(g, pressed ? SID::BalanceThumbSelected : SID::BalanceThumb, thumbX, 59);
}

//--- Shuffle / Repeat ---
void SkinnedPlayerPanel::drawShuffleRepeat(juce::Graphics& g) const
{
    bool shP = (pressedZone == HitZone::Shuffle);
    bool rpP = (pressedZone == HitZone::Repeat);

    SID shId;
    if (shuffleOn)
        shId = shP ? SID::ShuffleButtonSelectedPressed : SID::ShuffleButtonSelected;
    else
        shId = shP ? SID::ShuffleButtonPressed : SID::ShuffleButton;

    SID rpId;
    if (repeatOn)
        rpId = rpP ? SID::RepeatButtonSelectedPressed : SID::RepeatButtonSelected;
    else
        rpId = rpP ? SID::RepeatButtonPressed : SID::RepeatButton;

    drawSprite(g, shId, 164, 89);
    drawSprite(g, rpId, 210, 89);
}

//--- EQ / PL toggle buttons ---
void SkinnedPlayerPanel::drawEqPlToggles(juce::Graphics& g) const
{
    // EQ button at (219, 58) — we use it as decorative for now
    bool eqP = (pressedZone == HitZone::EqToggle);
    drawSprite(g, eqP ? SID::EqButtonPressed : SID::EqButton, 219, 58);

    // PL button at (242, 58)
    bool plP = (pressedZone == HitZone::PlToggle);
    drawSprite(g, plP ? SID::PlButtonPressed : SID::PlButton, 242, 58);
}

//--- Visualization ---
void SkinnedPlayerPanel::drawVisualization(juce::Graphics& g) const
{
    auto area = visAreaRect();
    const auto& colors = skin->visColors.colors;

    // Background
    g.setColour(colors[0]);
    g.fillRect(area);

    if (visMode == VisMode::Spectrum)
    {
        const int numBands = 20;
        float barW = static_cast<float>(area.getWidth()) / numBands;
        float maxH = static_cast<float>(area.getHeight());

        for (int i = 0; i < numBands; ++i)
        {
            float norm = (specBands[static_cast<size_t>(i)] + 60.0f) / 60.0f;
            norm = juce::jlimit(0.0f, 1.0f, norm);
            int numCells = static_cast<int>(std::ceil(norm * 16.0f));
            float cellH = maxH / 16.0f;
            float x = area.getX() + i * barW;

            for (int c = 0; c < numCells; ++c)
            {
                int ci = juce::jlimit(1, 23,
                    static_cast<int>(static_cast<float>(c) / 16.0f * 23.0f));
                float cy = area.getBottom() - (c + 1) * cellH;
                g.setColour(colors[static_cast<size_t>(ci)]);
                g.fillRect(x + 1.0f, cy, barW - 2.0f, cellH - 1.0f);
            }
        }
    }
    else
    {
        if (oscSampleCount < 2) return;
        g.setColour(colors[18]);
        juce::Path path;
        float xStep = static_cast<float>(area.getWidth()) / (oscSampleCount - 1);
        float cY = static_cast<float>(area.getCentreY());
        float hH = area.getHeight() * 0.5f;

        for (int i = 0; i < oscSampleCount; ++i)
        {
            float x = area.getX() + i * xStep;
            float y = cY - oscSamples[static_cast<size_t>(i)] * hH;
            y = juce::jlimit(static_cast<float>(area.getY()),
                             static_cast<float>(area.getBottom()), y);
            if (i == 0) path.startNewSubPath(x, y);
            else        path.lineTo(x, y);
        }
        g.strokePath(path, juce::PathStrokeType(1.0f));
    }
}

//==============================================================================
// MOUSE INTERACTION
//==============================================================================
SkinnedPlayerPanel::HitZone SkinnedPlayerPanel::hitTest(juce::Point<int> np) const
{
    // Transport buttons (highest priority — most commonly clicked)
    if (prevRect().contains(np))     return HitZone::Prev;
    if (playRect().contains(np))     return HitZone::Play;
    if (pauseRect().contains(np))    return HitZone::Pause;
    if (stopRect().contains(np))     return HitZone::Stop;
    if (nextRect().contains(np))     return HitZone::Next;
    if (ejectRect().contains(np))    return HitZone::Eject;
    // Sliders
    if (posBarRect().contains(np))   return HitZone::PositionBar;
    if (volRect().contains(np))      return HitZone::VolumeSlider;
    if (balRect().contains(np))      return HitZone::BalanceSlider;
    // Toggles
    if (shuffleRect().contains(np))  return HitZone::Shuffle;
    if (repeatRect().contains(np))   return HitZone::Repeat;
    if (eqBtnRect().contains(np))    return HitZone::EqToggle;
    if (plBtnRect().contains(np))    return HitZone::PlToggle;
    // Title bar
    if (minimizeRect().contains(np)) return HitZone::Minimize;
    if (shadeRect().contains(np))    return HitZone::Shade;
    if (closeRect().contains(np))    return HitZone::Close;
    // Vis area click toggles mode
    if (visAreaRect().contains(np))  return HitZone::VisArea;
    // Time click (could toggle time mode)
    if (timeAreaRect().contains(np)) return HitZone::TimeDisplay;

    return HitZone::None;
}

void SkinnedPlayerPanel::mouseDown(const juce::MouseEvent& e)
{
    auto np = toNative(e.getPosition());
    pressedZone = hitTest(np);

    if (pressedZone == HitZone::PositionBar)
    {
        isDraggingPosition = true;
        auto r = posBarRect();
        double rel = static_cast<double>(np.x - r.getX()) / r.getWidth();
        positionValue = juce::jlimit(0.0, 1.0, rel);
    }
    else if (pressedZone == HitZone::VolumeSlider)
    {
        isDraggingVolume = true;
        auto r = volRect();
        double rel = static_cast<double>(np.x - r.getX()) / r.getWidth();
        volumeValue = juce::jlimit(0.0, 1.0, rel);
        if (onVolumeChanged) onVolumeChanged(volumeValue);
    }
    else if (pressedZone == HitZone::BalanceSlider)
    {
        isDraggingBalance = true;
        auto r = balRect();
        double rel = static_cast<double>(np.x - r.getX()) / r.getWidth();
        balanceValue = juce::jlimit(0.0, 1.0, rel);
        if (onBalanceChanged) onBalanceChanged(balanceValue);
    }

    repaint();
}

void SkinnedPlayerPanel::mouseDrag(const juce::MouseEvent& e)
{
    auto np = toNative(e.getPosition());

    if (isDraggingPosition)
    {
        auto r = posBarRect();
        double rel = static_cast<double>(np.x - r.getX()) / r.getWidth();
        positionValue = juce::jlimit(0.0, 1.0, rel);
        repaint();
    }
    else if (isDraggingVolume)
    {
        auto r = volRect();
        double rel = static_cast<double>(np.x - r.getX()) / r.getWidth();
        volumeValue = juce::jlimit(0.0, 1.0, rel);
        if (onVolumeChanged) onVolumeChanged(volumeValue);
        repaint();
    }
    else if (isDraggingBalance)
    {
        auto r = balRect();
        double rel = static_cast<double>(np.x - r.getX()) / r.getWidth();
        balanceValue = juce::jlimit(0.0, 1.0, rel);
        if (onBalanceChanged) onBalanceChanged(balanceValue);
        repaint();
    }
}

void SkinnedPlayerPanel::mouseUp(const juce::MouseEvent& e)
{
    auto np = toNative(e.getPosition());
    auto zone = hitTest(np);

    // Only fire if released inside the same zone that was pressed
    if (zone == pressedZone)
    {
        switch (zone)
        {
            case HitZone::Prev:    if (onPrevClicked)  onPrevClicked();  break;
            case HitZone::Play:    if (onPlayClicked)  onPlayClicked();  break;
            case HitZone::Pause:   if (onPauseClicked) onPauseClicked(); break;
            case HitZone::Stop:    if (onStopClicked)  onStopClicked();  break;
            case HitZone::Next:    if (onNextClicked)  onNextClicked();  break;
            case HitZone::Eject:   if (onEjectClicked) onEjectClicked(); break;
            case HitZone::Shuffle:
                shuffleOn = !shuffleOn;
                if (onShuffleToggled) onShuffleToggled(shuffleOn);
                break;
            case HitZone::Repeat:
                repeatOn = !repeatOn;
                if (onRepeatToggled) onRepeatToggled(repeatOn);
                break;
            case HitZone::VisArea:
                visMode = (visMode == VisMode::Spectrum) ? VisMode::Oscilloscope : VisMode::Spectrum;
                break;
            default: break;
        }
    }

    // Finish slider drags
    if (isDraggingPosition)
    {
        isDraggingPosition = false;
        if (onPositionChanged) onPositionChanged(positionValue);
    }
    if (isDraggingVolume)   isDraggingVolume = false;
    if (isDraggingBalance)  isDraggingBalance = false;

    pressedZone = HitZone::None;
    repaint();
}

void SkinnedPlayerPanel::mouseMove(const juce::MouseEvent&)
{
    // Could set cursor based on hover zone in the future
}
