#pragma once

#include <JuceHeader.h>

#if JUCE_WINDOWS
 #ifndef NOMINMAX
  #define NOMINMAX
 #endif
 #ifndef WIN32_LEAN_AND_MEAN
  #define WIN32_LEAN_AND_MEAN
 #endif
 #include <Windows.h>
 #ifdef min
  #undef min
 #endif
 #ifdef max
  #undef max
 #endif
#endif

//==============================================================================
/// A wrapper around juce::ColourSelector that adds an eyedropper button.
/// When the eyedropper is active, a full-screen overlay captures the next
/// mouse click and samples the pixel colour under the cursor.
class ColourPickerWithEyedropper : public juce::Component
{
public:
    ColourPickerWithEyedropper()
    {
        selector_ = std::make_unique<juce::ColourSelector>(
            juce::ColourSelector::showColourAtTop
            | juce::ColourSelector::showSliders
            | juce::ColourSelector::showAlphaChannel
            | juce::ColourSelector::showColourspace);
        addAndMakeVisible(*selector_);

        eyedropperBtn_.setButtonText(juce::String::charToString(0x1F441));  // eye emoji
        eyedropperBtn_.setTooltip("Pick colour from screen");
        eyedropperBtn_.setColour(juce::TextButton::buttonColourId, juce::Colour(0xFF2A2A40));
        eyedropperBtn_.setColour(juce::TextButton::textColourOffId, juce::Colours::white);
        addAndMakeVisible(eyedropperBtn_);

        hexEditor_.setFont(juce::Font(juce::FontOptions(13.0f)));
        hexEditor_.setJustification(juce::Justification::centred);
        hexEditor_.setInputRestrictions(9, "0123456789abcdefABCDEF#");
        hexEditor_.setColour(juce::TextEditor::backgroundColourId, juce::Colour(0xFF2A2A40));
        hexEditor_.setColour(juce::TextEditor::textColourId, juce::Colours::white);
        hexEditor_.setColour(juce::TextEditor::outlineColourId, juce::Colour(0xFF555570));
        addAndMakeVisible(hexEditor_);

        eyedropperBtn_.onClick = [this]() { startEyedropper(); };

        hexEditor_.onReturnKey = [this]()
        {
            auto text = hexEditor_.getText().trimCharactersAtStart("#");
            juce::Colour c;
            if (text.length() == 6)
                c = juce::Colour((uint32_t)text.getHexValue32()).withAlpha((uint8_t)255);
            else if (text.length() == 8)
                c = juce::Colour((uint32_t)text.getHexValue32());
            else
                return;
            selector_->setCurrentColour(c);
        };

        // Poll selector colour to keep hex field in sync
        startTimerHz(10);
    }

    ~ColourPickerWithEyedropper() override { stopTimer(); }

    /// Callback fires whenever the colour changes (slider, hex input, or eyedropper).
    std::function<void(juce::Colour)> onColourChanged;

    void setCurrentColour(juce::Colour c)
    {
        selector_->setCurrentColour(c);
        updateHexField(c);
    }

    juce::Colour getCurrentColour() const { return selector_->getCurrentColour(); }
    juce::ColourSelector& getSelector() { return *selector_; }

    void resized() override
    {
        auto area = getLocalBounds();
        auto bottom = area.removeFromBottom(30);
        bottom.reduce(4, 2);
        eyedropperBtn_.setBounds(bottom.removeFromLeft(36));
        bottom.removeFromLeft(4);
        hexEditor_.setBounds(bottom);
        selector_->setBounds(area);
    }

    bool isShowing() const { return Component::isShowing(); }

private:
    std::unique_ptr<juce::ColourSelector> selector_;
    juce::TextButton  eyedropperBtn_;
    juce::TextEditor  hexEditor_;
    juce::Colour      lastSyncedColour_;

    //==========================================================================
    /// Full-screen transparent overlay that captures click for eyedropper.
    /// Uses a native top-level window with a low-alpha fill so Windows
    /// still delivers mouse events.
    class EyedropperOverlay : public juce::TopLevelWindow,
                              public juce::Timer
    {
    public:
        std::function<void(juce::Colour)> onPicked;
        std::function<void()> onCancel;

        EyedropperOverlay()
            : juce::TopLevelWindow("Eyedropper", true)
        {
            setOpaque(false);
            setAlwaysOnTop(true);
            setDropShadowEnabled(false);
            setUsingNativeTitleBar(false);
            setMouseCursor(juce::MouseCursor::CrosshairCursor);
            setWantsKeyboardFocus(true);
        }

        void launch()
        {
            auto& displays = juce::Desktop::getInstance().getDisplays();
            juce::Rectangle<int> totalBounds;
            for (int i = 0; i < displays.displays.size(); ++i)
                totalBounds = totalBounds.getUnion(displays.displays[i].totalArea);

            // Grab a screenshot BEFORE showing overlay
            captureScreen(totalBounds);
            screenOrigin_ = totalBounds.getPosition();

            setBounds(totalBounds);
            setVisible(true);
            toFront(true);
            grabKeyboardFocus();
            startTimerHz(30);

#if JUCE_WINDOWS
            // Make the window click-through transparent but still receive input
            if (auto* peer = getPeer())
            {
                if (auto hwnd = (HWND) peer->getNativeHandle())
                {
                    // Remove WS_EX_TRANSPARENT so we receive mouse events
                    auto exStyle = GetWindowLong(hwnd, GWL_EXSTYLE);
                    exStyle |= WS_EX_TOOLWINDOW;     // hide from taskbar
                    exStyle &= ~WS_EX_TRANSPARENT;    // ensure we get clicks
                    SetWindowLong(hwnd, GWL_EXSTYLE, exStyle);
                    SetWindowPos(hwnd, HWND_TOPMOST, 0, 0, 0, 0,
                                 SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
                }
            }
#endif
        }

        void paint(juce::Graphics& g) override
        {
            // Draw the captured screenshot so user sees the desktop
            if (screenshot_.isValid())
                g.drawImageAt(screenshot_, 0, 0);
            else
                g.fillAll(juce::Colour(0x80000000));

            // Dark tint overlay
            g.setColour(juce::Colour(0x18000000));
            g.fillAll();

            // Draw magnifier preview near cursor
            if (previewValid_)
            {
                const int magSize = 140;
                // Position magnifier above cursor, or below if too close to top
                int mx = previewX_ - magSize / 2;
                int my = previewY_ - magSize - 30;
                if (my < 10) my = previewY_ + 30;
                mx = juce::jlimit(4, getWidth() - magSize - 4, mx);

                auto magBounds = juce::Rectangle<int>(mx, my, magSize, magSize);

                // Zoomed pixel grid (7x7 pixels centered on cursor)
                const int gridN = 7;
                const int cellSize = (magSize - 20) / gridN;
                auto gridArea = magBounds.reduced(10).removeFromTop(cellSize * gridN);

                // Background
                g.setColour(juce::Colour(0xEE1A1A2E));
                g.fillRoundedRectangle(magBounds.toFloat(), 8.0f);
                g.setColour(juce::Colours::white.withAlpha(0.5f));
                g.drawRoundedRectangle(magBounds.toFloat(), 8.0f, 1.5f);

                // Draw zoomed pixels
                if (screenshot_.isValid())
                {
                    int scrX = previewX_;
                    int scrY = previewY_;
                    for (int dy = 0; dy < gridN; ++dy)
                    {
                        for (int dx = 0; dx < gridN; ++dx)
                        {
                            int sx = scrX + dx - gridN / 2;
                            int sy = scrY + dy - gridN / 2;
                            juce::Colour pc = (sx >= 0 && sy >= 0 &&
                                               sx < screenshot_.getWidth() &&
                                               sy < screenshot_.getHeight())
                                ? screenshot_.getPixelAt(sx, sy)
                                : juce::Colours::black;
                            g.setColour(pc);
                            g.fillRect(gridArea.getX() + dx * cellSize,
                                       gridArea.getY() + dy * cellSize,
                                       cellSize, cellSize);
                        }
                    }
                    // Highlight center cell
                    g.setColour(juce::Colours::white);
                    g.drawRect(gridArea.getX() + (gridN / 2) * cellSize,
                               gridArea.getY() + (gridN / 2) * cellSize,
                               cellSize, cellSize, 2);
                }

                // Colour swatch + hex below the grid
                auto swatchArea = magBounds.reduced(10).removeFromBottom(22);
                g.setColour(previewColour_);
                g.fillRect(swatchArea.removeFromLeft(22));
                g.setColour(juce::Colours::white);
                g.setFont(juce::Font(juce::FontOptions(12.0f)));
                swatchArea.removeFromLeft(4);
                g.drawText("#" + previewColour_.toDisplayString(false),
                           swatchArea, juce::Justification::centredLeft);
            }
        }

        void mouseMove(const juce::MouseEvent& e) override
        {
            updatePreview(e.getPosition());
        }

        void mouseDrag(const juce::MouseEvent& e) override
        {
            updatePreview(e.getPosition());
        }

        void mouseDown(const juce::MouseEvent& e) override
        {
            if (e.mods.isLeftButtonDown())
            {
                auto colour = getColourAt(e.getPosition());
                stopTimer();
                if (onPicked) onPicked(colour);
            }
            else
            {
                stopTimer();
                if (onCancel) onCancel();
            }
        }

        bool keyPressed(const juce::KeyPress& key) override
        {
            if (key == juce::KeyPress::escapeKey)
            {
                stopTimer();
                if (onCancel) onCancel();
                return true;
            }
            return false;
        }

        void timerCallback() override
        {
            // Update preview from global mouse pos
            auto mousePos = juce::Desktop::getMousePosition() - screenOrigin_;
            if (mousePos != juce::Point<int>(previewX_, previewY_))
            {
                updatePreview(mousePos);
            }
        }

    private:
        juce::Image screenshot_;
        juce::Point<int> screenOrigin_;
        bool previewValid_ = false;
        int previewX_ = 0, previewY_ = 0;
        juce::Colour previewColour_;

        void captureScreen(juce::Rectangle<int> bounds)
        {
            int w = bounds.getWidth();
            int h = bounds.getHeight();
            screenshot_ = juce::Image(juce::Image::ARGB, w, h, true);

#if JUCE_WINDOWS
            // Step 1: GDI capture of the desktop (non-JUCE content)
            HDC hdcScreen = GetDC(nullptr);
            if (hdcScreen)
            {
                HDC hdcMem = CreateCompatibleDC(hdcScreen);
                HBITMAP hBmp = CreateCompatibleBitmap(hdcScreen, w, h);
                SelectObject(hdcMem, hBmp);
                BitBlt(hdcMem, 0, 0, w, h,
                       hdcScreen, bounds.getX(), bounds.getY(), SRCCOPY);

                juce::Image::BitmapData bmpData(screenshot_, juce::Image::BitmapData::writeOnly);

                BITMAPINFOHEADER bmi = {};
                bmi.biSize = sizeof(bmi);
                bmi.biWidth = w;
                bmi.biHeight = -h;
                bmi.biPlanes = 1;
                bmi.biBitCount = 32;
                bmi.biCompression = BI_RGB;

                std::vector<uint8_t> pixels(w * h * 4);
                GetDIBits(hdcMem, hBmp, 0, h, pixels.data(),
                          reinterpret_cast<BITMAPINFO*>(&bmi), DIB_RGB_COLORS);

                for (int y = 0; y < h; ++y)
                    for (int x = 0; x < w; ++x)
                    {
                        int idx = (y * w + x) * 4;
                        bmpData.setPixelColour(x, y,
                            juce::Colour(pixels[idx + 2], pixels[idx + 1], pixels[idx]));
                    }

                DeleteObject(hBmp);
                DeleteDC(hdcMem);
                ReleaseDC(nullptr, hdcScreen);
            }
#endif

            // Step 2: Overlay JUCE-rendered windows on top (captures what
            //         GDI/BitBlt may miss with Direct2D / software renderer)
            juce::Graphics g(screenshot_);
            auto& desktop = juce::Desktop::getInstance();
            for (int i = 0; i < desktop.getNumComponents(); ++i)
            {
                auto* comp = desktop.getComponent(i);
                if (comp == this) continue;               // skip ourselves
                if (!comp->isVisible()) continue;

                auto compScreen = comp->getScreenBounds();
                auto localArea  = comp->getLocalBounds();
                if (localArea.isEmpty()) continue;

                auto snapshot = comp->createComponentSnapshot(localArea);
                g.drawImageAt(snapshot,
                              compScreen.getX() - bounds.getX(),
                              compScreen.getY() - bounds.getY());
            }
        }

        void updatePreview(juce::Point<int> localPos)
        {
            previewValid_ = true;
            previewX_ = localPos.x;
            previewY_ = localPos.y;
            previewColour_ = getColourAt(localPos);
            repaint();
        }

        juce::Colour getColourAt(juce::Point<int> localPos) const
        {
            if (screenshot_.isValid() &&
                localPos.x >= 0 && localPos.y >= 0 &&
                localPos.x < screenshot_.getWidth() &&
                localPos.y < screenshot_.getHeight())
            {
                return screenshot_.getPixelAt(localPos.x, localPos.y);
            }
            return juce::Colours::black;
        }
    };

    std::unique_ptr<EyedropperOverlay> overlay_;

    void startEyedropper()
    {
        overlay_ = std::make_unique<EyedropperOverlay>();
        overlay_->onPicked = [this](juce::Colour c)
        {
            selector_->setCurrentColour(c);
            updateHexField(c);
            if (onColourChanged) onColourChanged(c);
            overlay_.reset();
        };
        overlay_->onCancel = [this]()
        {
            overlay_.reset();
        };
        overlay_->launch();
    }

    void updateHexField(juce::Colour c)
    {
        hexEditor_.setText("#" + c.toDisplayString(true), false);
        lastSyncedColour_ = c;
    }

    // Timer for sync
    void timerCallback()
    {
        auto c = selector_->getCurrentColour();
        if (c != lastSyncedColour_)
        {
            updateHexField(c);
            if (onColourChanged) onColourChanged(c);
        }
    }

    // Need to inherit Timer
    void startTimerHz(int hz) { timer_.owner = this; timer_.startTimerHz(hz); }
    void stopTimer() { timer_.stopTimer(); }

    struct InternalTimer : public juce::Timer
    {
        ColourPickerWithEyedropper* owner = nullptr;
        void timerCallback() override { if (owner) owner->timerCallback(); }
    };
    InternalTimer timer_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ColourPickerWithEyedropper)
};
