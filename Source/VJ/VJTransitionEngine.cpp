#include "VJTransitionEngine.h"

//==============================================================================
void VJTransitionEngine::startTransition(Type type, juce::Image fromImage, juce::Image toImage, int durationMs)
{
    currentType_ = type;
    fromImage_   = fromImage;
    toImage_     = toImage;

    if (durationMs <= 0 || type == Type::Cut)
    {
        fromImage_ = toImage_;
        progress_  = 1.0f;
        active_    = false;
        return;
    }

    durationMs_ = durationMs;
    startMs_    = juce::Time::getMillisecondCounter();
    progress_   = 0.0f;
    active_     = true;
}

//==============================================================================
bool VJTransitionEngine::tick()
{
    if (!active_)
        return false;

    const auto elapsed = static_cast<float>(juce::Time::getMillisecondCounter() - startMs_);
    progress_ = juce::jlimit(0.0f, 1.0f, elapsed / static_cast<float>(durationMs_));

    if (progress_ >= 1.0f)
    {
        fromImage_ = toImage_;
        active_    = false;
    }
    return active_;
}

//==============================================================================
void VJTransitionEngine::paint(juce::Graphics& g, juce::Rectangle<int> bounds) const
{
    // Always draw the from-image first as the base ---------------------------
    if (fromImage_.isValid())
        g.drawImage(fromImage_, bounds.toFloat());

    if (!active_)
        return;

    const float t = progress_;

    switch (currentType_)
    {
        //----------------------------------------------------------------------
        case Type::Cut:
        {
            if (toImage_.isValid())
                g.drawImage(toImage_, bounds.toFloat());
            break;
        }

        //----------------------------------------------------------------------
        case Type::Crossfade:
        {
            if (toImage_.isValid())
            {
                g.setOpacity(t);
                g.drawImage(toImage_, bounds.toFloat());
                g.setOpacity(1.0f);
            }
            break;
        }

        //----------------------------------------------------------------------
        case Type::WipeLeft:
        {
            if (toImage_.isValid())
            {
                const int w       = bounds.getWidth();
                const int revealW = juce::roundToInt(t * w);
                const auto clip   = bounds.withTrimmedRight(w - revealW);
                const auto srcClip = juce::Rectangle<float>(0, 0,
                    (float)revealW * toImage_.getWidth()  / w,
                    (float)toImage_.getHeight());
                g.drawImage(toImage_, clip.toFloat(), juce::RectanglePlacement::stretchToFit);
            }
            break;
        }

        //----------------------------------------------------------------------
        case Type::WipeRight:
        {
            if (toImage_.isValid())
            {
                const int w       = bounds.getWidth();
                const int revealW = juce::roundToInt(t * w);
                const auto clip   = bounds.withTrimmedLeft(w - revealW);
                g.drawImage(toImage_, clip.toFloat(), juce::RectanglePlacement::stretchToFit);
            }
            break;
        }

        //----------------------------------------------------------------------
        case Type::WipeUp:
        {
            if (toImage_.isValid())
            {
                const int h       = bounds.getHeight();
                const int revealH = juce::roundToInt(t * h);
                const auto clip   = bounds.withTrimmedBottom(h - revealH);
                g.drawImage(toImage_, clip.toFloat(), juce::RectanglePlacement::stretchToFit);
            }
            break;
        }

        //----------------------------------------------------------------------
        case Type::WipeDown:
        {
            if (toImage_.isValid())
            {
                const int h       = bounds.getHeight();
                const int revealH = juce::roundToInt(t * h);
                const auto clip   = bounds.withTrimmedTop(h - revealH);
                g.drawImage(toImage_, clip.toFloat(), juce::RectanglePlacement::stretchToFit);
            }
            break;
        }

        //----------------------------------------------------------------------
        case Type::ZoomBlend:
        {
            if (fromImage_.isValid())
            {
                // Scale up the from-image while fading it out
                const float scale  = 1.0f + t * 0.3f;
                const float cx     = bounds.getCentreX();
                const float cy     = bounds.getCentreY();
                const float newW   = bounds.getWidth()  * scale;
                const float newH   = bounds.getHeight() * scale;
                const juce::Rectangle<float> scaled (cx - newW * 0.5f, cy - newH * 0.5f, newW, newH);
                g.setOpacity(1.0f - t);
                g.drawImage(fromImage_, scaled);
                g.setOpacity(1.0f);
            }
            if (toImage_.isValid())
            {
                g.setOpacity(t);
                g.drawImage(toImage_, bounds.toFloat());
                g.setOpacity(1.0f);
            }
            break;
        }

        default:
            break;
    }
}
