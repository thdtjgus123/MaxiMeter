#include "WinampSkinRenderer.h"

//==============================================================================
WinampSkinRenderer::WinampSkinRenderer()
{
    startTimerHz(30);  // Scroll timer
}

WinampSkinRenderer::~WinampSkinRenderer()
{
    stopTimer();
}

//==============================================================================
bool WinampSkinRenderer::loadSkin(const juce::File& wszFile)
{
    skinModel = parser.loadFromFile(wszFile);

    if (skinModel.isLoaded())
    {
        fontRenderer.setSkin(&skinModel);
        setSize(275 * scale, 116 * scale);
        repaint();
        DBG("WinampSkinRenderer: Skin loaded — " + skinModel.skinName);
        return true;
    }

    DBG("WinampSkinRenderer: Failed — " + parser.getLastError());
    return false;
}

void WinampSkinRenderer::setSkinModel(const Skin::SkinModel* model)
{
    if (model == nullptr || !model->isLoaded())
        return;

    skinModel = *model;
    fontRenderer.setSkin(&skinModel);
    setSize(275 * scale, 116 * scale);
    repaint();
    DBG("WinampSkinRenderer: Skin model applied — " + skinModel.skinName);
}

void WinampSkinRenderer::setScale(int newScale)
{
    scale = juce::jlimit(1, 4, newScale);
    setSize(275 * scale, 116 * scale);
    repaint();
}

void WinampSkinRenderer::setTitleText(const juce::String& text)
{
    titleText = text;
    scrollOffset = 0;
}

void WinampSkinRenderer::setTime(int minutes, int seconds)
{
    timeMinutes = minutes;
    timeSeconds = seconds;
}

void WinampSkinRenderer::setPlayState(PlayState state)
{
    playState = state;
}

void WinampSkinRenderer::setMonoStereo(bool isStereo)
{
    stereoMode = isStereo;
}

void WinampSkinRenderer::setSpectrumData(const float* data, int numBands)
{
    int count = juce::jmin(numBands, 20);
    for (int i = 0; i < count; ++i)
        spectrumBands[static_cast<size_t>(i)] = data[i];
}

void WinampSkinRenderer::setOscilloscopeData(const float* data, int numSamples)
{
    oscDataSize = juce::jmin(numSamples, 512);
    std::memcpy(oscData.data(), data, sizeof(float) * static_cast<size_t>(oscDataSize));
}

//==============================================================================
void WinampSkinRenderer::timerCallback()
{
    // Advance scroll offset for title text
    int textWidth = fontRenderer.getTextWidth(titleText, 1);
    int displayWidth = 154;  // Title display area width in Winamp

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
void WinampSkinRenderer::paint(juce::Graphics& g)
{
    if (!skinModel.isLoaded())
    {
        g.fillAll(juce::Colour(0xFF1A1A2E));
        g.setColour(juce::Colours::grey);
        g.setFont(14.0f);
        g.drawText("No skin loaded — drag a .wsz file here",
                    getLocalBounds(), juce::Justification::centred);
        return;
    }

    drawBackground(g);

    // Scale from native 275×116 space to actual component size
    float sx = static_cast<float>(getWidth())  / 275.0f;
    float sy = static_cast<float>(getHeight()) / 116.0f;
    g.addTransform(juce::AffineTransform::scale(sx, sy));

    drawTitleBar(g);
    drawTimeDisplay(g);
    drawScrollingTitle(g);
    drawMonoStereoIndicator(g);
    drawPlayStateIndicator(g);
    drawTransportButtons(g);
    drawVisualization(g);
}

void WinampSkinRenderer::resized() {}

//==============================================================================
void WinampSkinRenderer::drawBackground(juce::Graphics& g)
{
    auto img = skinModel.getSprite(Skin::SpriteID::MainBackground);
    if (img.isValid())
    {
        g.drawImage(img, getLocalBounds().toFloat(),
                     juce::RectanglePlacement::stretchToFit);
    }
}

void WinampSkinRenderer::drawTitleBar(juce::Graphics& g)
{
    auto id = isWindowActive ? Skin::SpriteID::TitleBarSelected : Skin::SpriteID::TitleBar;
    drawSprite(g, id, 0, 0);

    // Window buttons (top-right area of titlebar)
    drawSprite(g, Skin::SpriteID::MinimizeButton, 244, 3);
    drawSprite(g, Skin::SpriteID::ShadeButton, 254, 3);
    drawSprite(g, Skin::SpriteID::CloseButton, 264, 3);
}

void WinampSkinRenderer::drawTransportButtons(juce::Graphics& g)
{
    // Transport buttons are at y=88, x starts at 16 in Winamp layout
    drawSprite(g, Skin::SpriteID::PreviousButton, 16, 88);
    drawSprite(g, Skin::SpriteID::PlayButton,     39, 88);
    drawSprite(g, Skin::SpriteID::PauseButton,    62, 88);
    drawSprite(g, Skin::SpriteID::StopButton,     85, 88);
    drawSprite(g, Skin::SpriteID::NextButton,    108, 88);
    drawSprite(g, Skin::SpriteID::EjectButton,   136, 88);
}

void WinampSkinRenderer::drawTimeDisplay(juce::Graphics& g)
{
    // Time display area: x=48, y=26 in classic Winamp
    fontRenderer.drawTime(g, timeMinutes, timeSeconds, 48, 26, 1);
}

void WinampSkinRenderer::drawScrollingTitle(juce::Graphics& g)
{
    // Title text area: x=111, y=27, width=154, height=6 in classic Winamp
    juce::Rectangle<int> titleArea(111, 27, 154, 6);
    fontRenderer.drawScrollingText(g, titleText, titleArea, scrollOffset, 1);
}

void WinampSkinRenderer::drawMonoStereoIndicator(juce::Graphics& g)
{
    // Mono/stereo indicators: x=212, y=41 (stereo) and x=239, y=41 (mono)
    if (stereoMode)
    {
        drawSprite(g, Skin::SpriteID::StereoActive,   212, 41);
        drawSprite(g, Skin::SpriteID::MonoInactive,    239, 41);
    }
    else
    {
        drawSprite(g, Skin::SpriteID::StereoInactive,  212, 41);
        drawSprite(g, Skin::SpriteID::MonoActive,      239, 41);
    }
}

void WinampSkinRenderer::drawPlayStateIndicator(juce::Graphics& g)
{
    // Play state indicator: x=24, y=28
    Skin::SpriteID id;
    switch (playState)
    {
        case PlayState::Playing: id = Skin::SpriteID::PlayingIndicator; break;
        case PlayState::Paused:  id = Skin::SpriteID::PausedIndicator; break;
        default:                 id = Skin::SpriteID::StoppedIndicator; break;
    }
    drawSprite(g, id, 24, 28);
}

//==============================================================================
void WinampSkinRenderer::drawVisualization(juce::Graphics& g)
{
    // Visualization area in classic Winamp: x=24, y=43, w=76, h=16
    juce::Rectangle<int> visArea(24, 43, 76, 16);

    if (visMode == VisMode::Spectrum)
        drawSpectrumVis(g, visArea);
    else
        drawOscilloscopeVis(g, visArea);
}

void WinampSkinRenderer::drawSpectrumVis(juce::Graphics& g, juce::Rectangle<int> area)
{
    // 20-band spectrum analyzer using viscolor.txt palette
    const auto& colors = skinModel.visColors.colors;
    const int numBands = 20;
    float barWidth = static_cast<float>(area.getWidth()) / numBands;
    float maxHeight = static_cast<float>(area.getHeight());

    // Dark background
    g.setColour(colors[0]);  // viscolor[0] = background
    g.fillRect(area);

    for (int i = 0; i < numBands; ++i)
    {
        // Map -60..0 dB → 0..1
        float normalized = (spectrumBands[static_cast<size_t>(i)] + 60.0f) / 60.0f;
        normalized = juce::jlimit(0.0f, 1.0f, normalized);
        float barH = normalized * maxHeight;

        // Number of visible cells (Winamp uses discrete LED-style bars)
        int numCells = static_cast<int>(std::ceil(normalized * 16.0f));  // 16 cells max

        float cellHeight = maxHeight / 16.0f;
        float x = area.getX() + i * barWidth;

        for (int cell = 0; cell < numCells; ++cell)
        {
            // Map cell index to viscolor palette (0=bottom/dim → 23=top/bright)
            int colorIdx = static_cast<int>(static_cast<float>(cell) / 16.0f * 23.0f);
            colorIdx = juce::jlimit(1, 23, colorIdx);

            float cellY = area.getBottom() - (cell + 1) * cellHeight;
            g.setColour(colors[static_cast<size_t>(colorIdx)]);
            g.fillRect(x + 1.0f, cellY, barWidth - 2.0f, cellHeight - 1.0f);
        }
    }
}

void WinampSkinRenderer::drawOscilloscopeVis(juce::Graphics& g, juce::Rectangle<int> area)
{
    const auto& colors = skinModel.visColors.colors;

    // Background
    g.setColour(colors[0]);
    g.fillRect(area);

    if (oscDataSize < 2)
        return;

    // Draw waveform using viscolor palette (middle colors)
    g.setColour(colors[18]);  // Use a bright color for the oscilloscope line

    juce::Path path;
    float xStep = static_cast<float>(area.getWidth()) / static_cast<float>(oscDataSize - 1);
    float centerY = area.getCentreY();
    float halfH = area.getHeight() * 0.5f;

    for (int i = 0; i < oscDataSize; ++i)
    {
        float x = area.getX() + i * xStep;
        float y = centerY - oscData[static_cast<size_t>(i)] * halfH;
        y = juce::jlimit(static_cast<float>(area.getY()), static_cast<float>(area.getBottom()), y);

        if (i == 0)
            path.startNewSubPath(x, y);
        else
            path.lineTo(x, y);
    }

    g.strokePath(path, juce::PathStrokeType(1.0f));
}

//==============================================================================
void WinampSkinRenderer::drawSprite(juce::Graphics& g, Skin::SpriteID id, int x, int y)
{
    auto img = skinModel.getSprite(id);
    if (!img.isValid())
        return;

    g.drawImage(img,
                 juce::Rectangle<float>(static_cast<float>(x),
                                        static_cast<float>(y),
                                        static_cast<float>(img.getWidth()),
                                        static_cast<float>(img.getHeight())));
}
