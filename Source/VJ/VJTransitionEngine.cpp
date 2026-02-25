#include "VJTransitionEngine.h"

static const juce::String emptyString;

//==============================================================================
void VJTransitionEngine::setCustomGLSL(const juce::String& source)
{
    if (!glslTransition_)
        glslTransition_ = std::make_unique<VJGLSLTransition>();
    glslTransition_->setSource(source);
}

const juce::String& VJTransitionEngine::getGLSLSource() const
{
    if (glslTransition_)
        return glslTransition_->getSource();
    return emptyString;
}

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
    // If the transition is not running just draw the last target image.
    if (!active_)
    {
        // After a transition ends fromImage_ == toImage_.
        if (fromImage_.isValid())
            g.drawImage(fromImage_, bounds.toFloat());
        return;
    }

    const float t = progress_;

    switch (currentType_)
    {
        //----------------------------------------------------------------------
        case Type::Cut:
        {
            if (toImage_.isValid())
                g.drawImage(toImage_, bounds.toFloat());
            else if (fromImage_.isValid())
                g.drawImage(fromImage_, bounds.toFloat());
            break;
        }

        //----------------------------------------------------------------------
        case Type::Crossfade:
        {
            if (fromImage_.isValid())
                g.drawImage(fromImage_, bounds.toFloat());
            if (toImage_.isValid())
            {
                g.setOpacity(t);
                g.drawImage(toImage_, bounds.toFloat());
                g.setOpacity(1.0f);
            }
            break;
        }

        //----------------------------------------------------------------------
        // Wipe transitions: clip the Graphics context and draw the full
        // destination image so it is NOT stretched into the partial region.
        //----------------------------------------------------------------------
        case Type::WipeLeft:
        {
            if (fromImage_.isValid())
                g.drawImage(fromImage_, bounds.toFloat());
            if (toImage_.isValid())
            {
                const int w       = bounds.getWidth();
                const int revealW = juce::roundToInt(t * w);
                juce::Graphics::ScopedSaveState ss(g);
                g.reduceClipRegion(bounds.withWidth(revealW));
                g.drawImage(toImage_, bounds.toFloat());
            }
            break;
        }

        //----------------------------------------------------------------------
        case Type::WipeRight:
        {
            if (fromImage_.isValid())
                g.drawImage(fromImage_, bounds.toFloat());
            if (toImage_.isValid())
            {
                const int w       = bounds.getWidth();
                const int revealW = juce::roundToInt(t * w);
                juce::Graphics::ScopedSaveState ss(g);
                g.reduceClipRegion(bounds.withTrimmedLeft(w - revealW));
                g.drawImage(toImage_, bounds.toFloat());
            }
            break;
        }

        //----------------------------------------------------------------------
        case Type::WipeUp:
        {
            if (fromImage_.isValid())
                g.drawImage(fromImage_, bounds.toFloat());
            if (toImage_.isValid())
            {
                const int h       = bounds.getHeight();
                const int revealH = juce::roundToInt(t * h);
                juce::Graphics::ScopedSaveState ss(g);
                g.reduceClipRegion(bounds.withHeight(revealH));
                g.drawImage(toImage_, bounds.toFloat());
            }
            break;
        }

        //----------------------------------------------------------------------
        case Type::WipeDown:
        {
            if (fromImage_.isValid())
                g.drawImage(fromImage_, bounds.toFloat());
            if (toImage_.isValid())
            {
                const int h       = bounds.getHeight();
                const int revealH = juce::roundToInt(t * h);
                juce::Graphics::ScopedSaveState ss(g);
                g.reduceClipRegion(bounds.withTrimmedTop(h - revealH));
                g.drawImage(toImage_, bounds.toFloat());
            }
            break;
        }

        //----------------------------------------------------------------------
        case Type::ZoomBlend:
        {
            // Zoom the from-image outward while fading it out, then fade in
            // the to-image at normal scale.  No base-draw here — both layers
            // are self-contained so we avoid the double-render flicker.
            if (fromImage_.isValid())
            {
                const float scale = 1.0f + t * 0.3f;
                const float cx    = static_cast<float>(bounds.getCentreX());
                const float cy    = static_cast<float>(bounds.getCentreY());
                const float newW  = static_cast<float>(bounds.getWidth())  * scale;
                const float newH  = static_cast<float>(bounds.getHeight()) * scale;
                const juce::Rectangle<float> scaled(cx - newW * 0.5f,
                                                     cy - newH * 0.5f,
                                                     newW, newH);
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

        //----------------------------------------------------------------------
        case Type::CustomGLSL:
        {
            if (glslTransition_ && glslTransition_->isValid())
            {
                juce::Image dest(juce::Image::ARGB,
                                 bounds.getWidth(), bounds.getHeight(), true);
                glslTransition_->renderCPU(dest, fromImage_, toImage_, t);
                g.drawImageAt(dest, bounds.getX(), bounds.getY());
            }
            else
            {
                // Fallback: simple crossfade when GLSL is invalid / none loaded.
                if (fromImage_.isValid())
                    g.drawImage(fromImage_, bounds.toFloat());
                if (toImage_.isValid())
                {
                    g.setOpacity(t);
                    g.drawImage(toImage_, bounds.toFloat());
                    g.setOpacity(1.0f);
                }
            }
            break;
        }

        default:
            break;
    }
}
