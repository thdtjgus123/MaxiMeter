#include "WaterReflectionComponent.h"
#include <cmath>

//==============================================================================
WaterReflectionComponent::WaterReflectionComponent()
{
    startTime_ = juce::Time::getMillisecondCounterHiRes() * 0.001;
    startTimerHz(30);
}

WaterReflectionComponent::~WaterReflectionComponent()
{
    stopTimer();
}

//==============================================================================
void WaterReflectionComponent::parentHierarchyChanged() { captureAbove(); }

void WaterReflectionComponent::resized()
{
    capturedAbove_ = juce::Image();
    blurred_       = juce::Image();
    distorted_     = juce::Image();
}

void WaterReflectionComponent::timerCallback() { captureAbove(); repaint(); }

//==============================================================================
// Fast separable box blur (horizontal + vertical pass)
void WaterReflectionComponent::applyBlur(const juce::Image& src, juce::Image& dst, float radius)
{
    const int w = src.getWidth(), h = src.getHeight();
    const int r = juce::jmax(1, juce::roundToInt(radius));

    juce::Image tmp(juce::Image::ARGB, w, h, true);
    juce::Image::BitmapData srcD(src, juce::Image::BitmapData::readOnly);
    juce::Image::BitmapData tmpD(tmp, juce::Image::BitmapData::readWrite);
    juce::Image::BitmapData dstD(dst, juce::Image::BitmapData::readWrite);

    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x)
        {
            int ra=0,rr=0,rg=0,rb=0,cnt=0;
            for (int dx=-r; dx<=r; ++dx)
            {
                const juce::uint8* p = srcD.getPixelPointer(juce::jlimit(0,w-1,x+dx), y);
                ra+=p[0]; rr+=p[1]; rg+=p[2]; rb+=p[3]; ++cnt;
            }
            juce::uint8* tp = tmpD.getPixelPointer(x,y);
            tp[0]=(juce::uint8)(ra/cnt); tp[1]=(juce::uint8)(rr/cnt);
            tp[2]=(juce::uint8)(rg/cnt); tp[3]=(juce::uint8)(rb/cnt);
        }

    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x)
        {
            int ra=0,rr=0,rg=0,rb=0,cnt=0;
            for (int dy=-r; dy<=r; ++dy)
            {
                const juce::uint8* p = tmpD.getPixelPointer(x, juce::jlimit(0,h-1,y+dy));
                ra+=p[0]; rr+=p[1]; rg+=p[2]; rb+=p[3]; ++cnt;
            }
            juce::uint8* dp = dstD.getPixelPointer(x,y);
            dp[0]=(juce::uint8)(ra/cnt); dp[1]=(juce::uint8)(rr/cnt);
            dp[2]=(juce::uint8)(rg/cnt); dp[3]=(juce::uint8)(rb/cnt);
        }
}

//==============================================================================
// Colour grade: desaturate + cool blue shift + depth fade
juce::PixelARGB WaterReflectionComponent::waterColour(juce::PixelARGB in,
                                                       float depthFade,
                                                       float desat,
                                                       float depthFadeStr)
{
    float r = in.getRed()   / 255.0f;
    float g = in.getGreen() / 255.0f;
    float b = in.getBlue()  / 255.0f;
    float a = in.getAlpha() / 255.0f;

    // Desaturate
    float luma = r * 0.299f + g * 0.587f + b * 0.114f;
    float sat  = 1.0f - juce::jlimit(0.0f, 1.0f, desat);
    r = luma + (r - luma) * sat;
    g = luma + (g - luma) * sat;
    b = luma + (b - luma) * sat;

    // Cool shift
    r *= 0.80f;
    g *= 0.90f;
    b = juce::jmin(1.0f, b * 1.05f + 0.04f);

    // Depth fade (configurable strength interpolated with no-fade)
    float fade = depthFade * depthFade;  // quadratic
    fade = 1.0f - depthFadeStr + depthFadeStr * fade;
    r *= fade; g *= fade; b *= fade;
    a *= juce::jmin(1.0f, depthFade * (1.0f + depthFadeStr));

    juce::PixelARGB out;
    out.setARGB(
        (juce::uint8)(juce::jlimit(0.0f,1.0f,a)*255.0f),
        (juce::uint8)(juce::jlimit(0.0f,1.0f,r)*255.0f),
        (juce::uint8)(juce::jlimit(0.0f,1.0f,g)*255.0f),
        (juce::uint8)(juce::jlimit(0.0f,1.0f,b)*255.0f));
    return out;
}

//==============================================================================
void WaterReflectionComponent::captureAbove()
{
    auto* parent = getParentComponent();
    if (!parent) return;
    const int w = getWidth(), h = getHeight();
    if (w <= 0 || h <= 0) return;

    auto myBounds = getBoundsInParent();
    juce::Rectangle<int> captureArea(myBounds.getX(), myBounds.getY() - h, w, h);
    captureArea = captureArea.getIntersection(parent->getLocalBounds());
    if (captureArea.isEmpty()) return;

    // ── Cap internal processing resolution to avoid UI-thread freeze at large sizes ──
    constexpr int kMaxProcW = 400;
    constexpr int kMaxProcH = 300;
    const float scaleX = juce::jmin(1.0f, (float)kMaxProcW / (float)w);
    const float scaleY = juce::jmin(1.0f, (float)kMaxProcH / (float)h);
    const float scale  = juce::jmin(scaleX, scaleY);
    const int   pw = juce::jmax(1, juce::roundToInt((float)w * scale));
    const int   ph = juce::jmax(1, juce::roundToInt((float)h * scale));

    // Capture at reduced scale
    auto snapshot = parent->createComponentSnapshot(captureArea, true, scale);

    // 180° rotation (flip both axes) + 90% vertical scale — at processing size
    capturedAbove_ = juce::Image(juce::Image::ARGB, pw, ph, true);
    {
        juce::Graphics gFlip(capturedAbove_);
        juce::AffineTransform rot180(-1.0f, 0.0f, (float)pw,
                                      0.0f, -0.90f, (float)ph * 0.90f);
        gFlip.drawImageTransformed(snapshot, rot180, false);
    }

    // Blur — radius scaled proportionally to processing resolution
    blurred_ = juce::Image(juce::Image::ARGB, pw, ph, true);
    applyBlur(capturedAbove_, blurred_, blurRadius_ * scale * (1.0f + intensity_ * 0.5f));

    // Perspective warp
    juce::Image perspected(juce::Image::ARGB, pw, ph, true);
    applyPerspective(blurred_, perspected);

    // Wave distortion
    distorted_ = juce::Image(juce::Image::ARGB, pw, ph, true);
    const float t = (float)(juce::Time::getMillisecondCounterHiRes() * 0.001 - startTime_);
    applyWaveDistortion(perspected, distorted_, t);
}

//==============================================================================
// Perspective warp.
// Top edge (y=0)  : perspScale = 1.0  → 1:1 mapping, fills full bounding box width.
// Bottom edge (y=h): perspScale = 1.0 + perspective_  → zoomed into centre,
//   sampling only centre (1/(1+p)) fraction of source, stretched to fill dest.
// Minimum (perspective_=0): flat rectangle. Maximum: strong inward zoom at bottom.
void WaterReflectionComponent::applyPerspective(const juce::Image& src, juce::Image& dst)
{
    const int w = src.getWidth(), h = src.getHeight();
    if (w <= 0 || h <= 0) return;

    juce::Image::BitmapData srcD(src, juce::Image::BitmapData::readOnly);
    juce::Image::BitmapData dstD(dst, juce::Image::BitmapData::readWrite);

    const float cx = (float)w * 0.5f;

    for (int y = 0; y < h; ++y)
    {
        // t = 0 at top (near/wide), 1 at bottom (far/narrow)
        const float t          = (float)y / (float)(h - 1);
        const float perspScale = 1.0f + perspective_ * t;  // 1.0 at top, (1+p) at bottom

        for (int x = 0; x < w; ++x)
        {
            // srcXf converges toward centre as perspScale grows
            const float srcXf = cx + ((float)x - cx) / perspScale;

            juce::uint8* dp = dstD.getPixelPointer(x, y);

            if (srcXf < 0.0f || srcXf >= (float)(w - 1))
            {
                dp[0] = dp[1] = dp[2] = dp[3] = 0;
                continue;
            }

            // Bilinear interpolation
            const int   sx0 = (int)srcXf;
            const int   sx1 = juce::jmin(w - 1, sx0 + 1);
            const float fx  = srcXf - (float)sx0;

            const juce::uint8* p0 = srcD.getPixelPointer(sx0, y);
            const juce::uint8* p1 = srcD.getPixelPointer(sx1, y);

            for (int c = 0; c < 4; ++c)
                dp[c] = (juce::uint8)(p0[c] * (1.0f - fx) + p1[c] * fx);
        }
    }
}

//==============================================================================
void WaterReflectionComponent::applyWaveDistortion(const juce::Image& src,
                                                    juce::Image& dst,
                                                    float timeSeconds)
{
    const int w = src.getWidth(), h = src.getHeight();
    if (w <= 0 || h <= 0) return;

    const float maxDx = intensity_ * (float)w * 0.018f;
    const float maxDy = intensity_ * (float)h * 0.008f;
    const float ws    = waveScale_;  // frequency multiplier
    const float t     = timeSeconds * speed_;

    juce::Image::BitmapData srcData(src, juce::Image::BitmapData::readOnly);
    juce::Image::BitmapData dstData(dst, juce::Image::BitmapData::readWrite);

    for (int y = 0; y < h; ++y)
    {
        const float depthFade = 1.0f - (float)y / (float)h;
        const float distortStr = 1.0f - depthFade;
        const float fy = (float)y;

        const float wave1 = std::sin(fy * 0.035f*ws + t*0.9f) * 0.55f
                          + std::sin(fy * 0.018f*ws + t*0.55f + 1.1f) * 0.30f
                          + std::sin(fy * 0.065f*ws + t*1.4f  + 2.3f) * 0.15f;

        const float baseDx = wave1 * maxDx * (0.3f + distortStr * 0.7f);
        const float baseDy = std::sin(fy * 0.025f*ws + t*0.7f + 0.5f) * maxDy * distortStr;

        for (int x = 0; x < w; ++x)
        {
            const float fx = (float)x;
            float localDx = baseDx
                + std::sin(fx*0.04f*ws + fy*0.02f*ws + t*1.2f) * maxDx * 0.25f * distortStr;
            float localDy = baseDy
                + std::sin(fx*0.055f*ws + t*0.85f + 0.9f) * maxDy * 0.3f * distortStr;

            int srcX = juce::jlimit(0, w-1, juce::roundToInt(fx + localDx));
            int srcY = juce::jlimit(0, h-1, juce::roundToInt(fy + localDy));

            const juce::uint8* sp = srcData.getPixelPointer(srcX, srcY);
            juce::PixelARGB px;
            px.setARGB(sp[0], sp[1], sp[2], sp[3]);

            auto graded = waterColour(px, depthFade, desaturation_, depthFade_);

            juce::uint8* dp = dstData.getPixelPointer(x, y);
            dp[0]=graded.getAlpha(); dp[1]=graded.getRed();
            dp[2]=graded.getGreen(); dp[3]=graded.getBlue();
        }
    }
}

//==============================================================================
void WaterReflectionComponent::paint(juce::Graphics& g)
{
    const int w = getWidth(), h = getHeight();
    const float fw = (float)w, fh = (float)h;

    juce::Colour bg = hasCustomBg() ? meterBg_ : juce::Colour(0xFF0d1520);
    g.fillAll(bg);

    // Distorted reflection
    if (distorted_.isValid() && distorted_.getWidth() > 0)
    {
        g.setOpacity(reflectOpacity_);
        g.drawImage(distorted_, 0, 0, w, h,
                    0, 0, distorted_.getWidth(), distorted_.getHeight());
        g.setOpacity(1.0f);
    }

    // Depth gradient
    {
        juce::Colour darkWater = hasCustomBg() ? meterBg_.darker(0.5f)
                                               : juce::Colour(0xFF0d1a28);
        juce::ColourGradient depthGrad(juce::Colour(0x00000000), 0.0f, 0.0f,
                                        darkWater.withAlpha(0.85f),  0.0f, fh, false);
        depthGrad.addColour(0.45, juce::Colour(0x22000000));
        depthGrad.addColour(0.72, darkWater.withAlpha(0.55f));
        g.setGradientFill(depthGrad);
        g.fillRect(0, 0, w, h);
    }

    // Surface mist (width spans full, fades down ~28% of height)
    if (mistOpacity_ > 0.01f)
    {
        juce::Colour mistCol = tintColour_.interpolatedWith(juce::Colour(0xFFB8D4E8), 0.6f)
                                           .withAlpha(mistOpacity_ * 0.85f);
        juce::ColourGradient mist(mistCol, 0.0f, 0.0f,
                                   juce::Colour(0x00000000), 0.0f, fh * 0.30f, false);
        g.setGradientFill(mist);
        g.fillRect(0, 0, w, (int)(fh * 0.30f));
    }

    // Water tint overlay
    if (tintColour_.getAlpha() > 0)
    {
        g.setColour(tintColour_);
        g.fillRect(0, 0, w, h);
    }

    // Specular shimmer lines
    if (shimmerCount_ > 0)
    {
        const float t    = (float)(juce::Time::getMillisecondCounterHiRes() * 0.001 - startTime_) * speed_;
        const float maxY = fh * 0.6f;

        for (int i = 0; i < shimmerCount_; ++i)
        {
            float lineY = std::fmod(
                fh * (0.03f + i * (0.60f / (float)shimmerCount_))
                + std::sin(t * 0.4f + i * 1.7f) * fh * 0.035f, maxY);
            float alpha = 0.06f + 0.05f * std::sin(t * 0.7f + i * 2.1f);
            float lineW = fw * (0.25f + 0.35f * std::abs(std::sin(t * 0.3f + i)));
            float lineX = (fw - lineW) * 0.5f + std::sin(t * 0.5f + i * 0.9f) * fw * 0.08f;
            g.setColour(juce::Colours::white.withAlpha(juce::jlimit(0.0f, 0.18f, alpha)));
            g.fillRoundedRectangle(lineX, lineY, lineW, 0.7f, 0.35f);
        }
    }

    // Hairline surface edge
    g.setColour(juce::Colours::white.withAlpha(0.22f));
    g.fillRect(0, 0, w, 1);
    g.setColour(juce::Colours::white.withAlpha(0.07f));
    g.fillRect(0, 1, w, 1);
}
