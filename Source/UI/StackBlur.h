#pragma once

#include <JuceHeader.h>

//==============================================================================
/// Fast stack-blur implementation for juce::Image (ARGB format).
/// Approximates a Gaussian blur in O(n) per pixel.
namespace StackBlur
{
    /// Apply an in-place stack blur to an ARGB32 image.
    /// @param img    The image to blur (must be ARGB).
    /// @param radius Blur radius in pixels (1..254).
    inline void apply(juce::Image& img, int radius)
    {
        if (radius < 1 || img.isNull()) return;
        radius = juce::jlimit(1, 254, radius);

        const int w = img.getWidth();
        const int h = img.getHeight();
        if (w < 1 || h < 1) return;

        img = img.convertedToFormat(juce::Image::ARGB);
        juce::Image::BitmapData data(img, juce::Image::BitmapData::readWrite);

        const int wm  = w - 1;
        const int hm  = h - 1;
        const int div = radius + radius + 1;

        std::vector<int> rSum(static_cast<size_t>(w * h));
        std::vector<int> gSum(static_cast<size_t>(w * h));
        std::vector<int> bSum(static_cast<size_t>(w * h));
        std::vector<int> aSum(static_cast<size_t>(w * h));

        int rOutSum, gOutSum, bOutSum, aOutSum;
        int rInSum, gInSum, bInSum, aInSum;

        std::vector<int> vMIN(static_cast<size_t>(std::max(w, h)));
        std::vector<int> vMAX(static_cast<size_t>(std::max(w, h)));

        const int divSum = (div + 1) >> 1;
        const int mulTable = divSum * divSum;
        std::vector<int> dv(static_cast<size_t>(256 * mulTable));
        for (int i = 0; i < 256 * mulTable; ++i)
            dv[static_cast<size_t>(i)] = (i / mulTable);

        // --- Horizontal pass ---
        for (int y = 0; y < h; ++y)
        {
            rInSum = gInSum = bInSum = aInSum = 0;
            rOutSum = gOutSum = bOutSum = aOutSum = 0;
            int rSumVal = 0, gSumVal = 0, bSumVal = 0, aSumVal = 0;

            for (int i = -radius; i <= radius; ++i)
            {
                int px = juce::jlimit(0, wm, i);
                auto* pixel = data.getPixelPointer(px, y);
                int weight = radius + 1 - std::abs(i);

                bSumVal += pixel[0] * weight;
                gSumVal += pixel[1] * weight;
                rSumVal += pixel[2] * weight;
                aSumVal += pixel[3] * weight;

                if (i > 0) { rInSum += pixel[2]; gInSum += pixel[1]; bInSum += pixel[0]; aInSum += pixel[3]; }
                else       { rOutSum += pixel[2]; gOutSum += pixel[1]; bOutSum += pixel[0]; aOutSum += pixel[3]; }
            }

            for (int x = 0; x < w; ++x)
            {
                auto idx = static_cast<size_t>(y * w + x);
                rSum[idx] = juce::jlimit(0, (int)dv.size() - 1, rSumVal);
                gSum[idx] = juce::jlimit(0, (int)dv.size() - 1, gSumVal);
                bSum[idx] = juce::jlimit(0, (int)dv.size() - 1, bSumVal);
                aSum[idx] = juce::jlimit(0, (int)dv.size() - 1, aSumVal);

                auto* outPixel = data.getPixelPointer(x, y);
                outPixel[0] = static_cast<uint8_t>(dv[static_cast<size_t>(bSum[idx])]);
                outPixel[1] = static_cast<uint8_t>(dv[static_cast<size_t>(gSum[idx])]);
                outPixel[2] = static_cast<uint8_t>(dv[static_cast<size_t>(rSum[idx])]);
                outPixel[3] = static_cast<uint8_t>(dv[static_cast<size_t>(aSum[idx])]);

                int p1x = juce::jlimit(0, wm, x + radius + 1);
                int p2x = juce::jlimit(0, wm, x - radius);

                auto* pIn  = data.getPixelPointer(p1x, y);
                auto* pOut = data.getPixelPointer(p2x, y);

                rSumVal += (rInSum -= pOut[2]) + (rOutSum += pIn[2]) - pOut[2];
                gSumVal += (gInSum -= pOut[1]) + (gOutSum += pIn[1]) - pOut[1];
                bSumVal += (bInSum -= pOut[0]) + (bOutSum += pIn[0]) - pOut[0];
                aSumVal += (aInSum -= pOut[3]) + (aOutSum += pIn[3]) - pOut[3];

                // Adjust in/out sums
                rInSum  += pIn[2] - pOut[2]; // already added above
                gInSum  += pIn[1] - pOut[1];
                bInSum  += pIn[0] - pOut[0];
                aInSum  += pIn[3] - pOut[3];
            }
        }

        // --- Vertical pass ---
        for (int x = 0; x < w; ++x)
        {
            rInSum = gInSum = bInSum = aInSum = 0;
            rOutSum = gOutSum = bOutSum = aOutSum = 0;
            int rSumVal = 0, gSumVal = 0, bSumVal = 0, aSumVal = 0;

            for (int i = -radius; i <= radius; ++i)
            {
                int py = juce::jlimit(0, hm, i);
                auto* pixel = data.getPixelPointer(x, py);
                int weight = radius + 1 - std::abs(i);

                bSumVal += pixel[0] * weight;
                gSumVal += pixel[1] * weight;
                rSumVal += pixel[2] * weight;
                aSumVal += pixel[3] * weight;

                if (i > 0) { rInSum += pixel[2]; gInSum += pixel[1]; bInSum += pixel[0]; aInSum += pixel[3]; }
                else       { rOutSum += pixel[2]; gOutSum += pixel[1]; bOutSum += pixel[0]; aOutSum += pixel[3]; }
            }

            for (int y = 0; y < h; ++y)
            {
                auto* outPixel = data.getPixelPointer(x, y);

                int rIdx = juce::jlimit(0, (int)dv.size() - 1, rSumVal);
                int gIdx = juce::jlimit(0, (int)dv.size() - 1, gSumVal);
                int bIdx = juce::jlimit(0, (int)dv.size() - 1, bSumVal);
                int aIdx = juce::jlimit(0, (int)dv.size() - 1, aSumVal);

                outPixel[2] = static_cast<uint8_t>(dv[static_cast<size_t>(rIdx)]);
                outPixel[1] = static_cast<uint8_t>(dv[static_cast<size_t>(gIdx)]);
                outPixel[0] = static_cast<uint8_t>(dv[static_cast<size_t>(bIdx)]);
                outPixel[3] = static_cast<uint8_t>(dv[static_cast<size_t>(aIdx)]);

                int p1y = juce::jlimit(0, hm, y + radius + 1);
                int p2y = juce::jlimit(0, hm, y - radius);

                auto* pIn  = data.getPixelPointer(x, p1y);
                auto* pOut = data.getPixelPointer(x, p2y);

                rSumVal += (rInSum -= pOut[2]) + (rOutSum += pIn[2]) - pOut[2];
                gSumVal += (gInSum -= pOut[1]) + (gOutSum += pIn[1]) - pOut[1];
                bSumVal += (bInSum -= pOut[0]) + (bOutSum += pIn[0]) - pOut[0];
                aSumVal += (aInSum -= pOut[3]) + (aOutSum += pIn[3]) - pOut[3];

                rInSum  += pIn[2] - pOut[2];
                gInSum  += pIn[1] - pOut[1];
                bInSum  += pIn[0] - pOut[0];
                aInSum  += pIn[3] - pOut[3];
            }
        }
    }

    /// Simple multi-pass box blur (3 passes ≈ Gaussian).
    /// Easier to reason about, good fallback.
    inline void applyBoxBlur(juce::Image& img, int radius)
    {
        if (radius < 1 || img.isNull()) return;
        radius = juce::jlimit(1, 120, radius);

        img = img.convertedToFormat(juce::Image::ARGB);
        const int w = img.getWidth();
        const int h = img.getHeight();
        if (w < 1 || h < 1) return;

        juce::Image temp(juce::Image::ARGB, w, h, true);

        for (int pass = 0; pass < 3; ++pass)
        {
            juce::Image::BitmapData src(img, juce::Image::BitmapData::readOnly);
            juce::Image::BitmapData dst(temp, juce::Image::BitmapData::writeOnly);

            // Horizontal pass
            for (int y = 0; y < h; ++y)
            {
                int rAcc = 0, gAcc = 0, bAcc = 0, aAcc = 0;
                int count = 0;

                // Seed accumulator
                for (int kx = 0; kx <= radius; ++kx)
                {
                    auto* p = src.getPixelPointer(juce::jlimit(0, w - 1, kx), y);
                    bAcc += p[0]; gAcc += p[1]; rAcc += p[2]; aAcc += p[3];
                    ++count;
                }

                for (int x = 0; x < w; ++x)
                {
                    auto* out = dst.getPixelPointer(x, y);
                    out[0] = static_cast<uint8_t>(bAcc / count);
                    out[1] = static_cast<uint8_t>(gAcc / count);
                    out[2] = static_cast<uint8_t>(rAcc / count);
                    out[3] = static_cast<uint8_t>(aAcc / count);

                    // Add right pixel, remove left pixel
                    int addX = juce::jlimit(0, w - 1, x + radius + 1);
                    int remX = juce::jlimit(0, w - 1, x - radius);

                    auto* pAdd = src.getPixelPointer(addX, y);
                    auto* pRem = src.getPixelPointer(remX, y);

                    if (x + radius + 1 <= w - 1) { bAcc += pAdd[0]; gAcc += pAdd[1]; rAcc += pAdd[2]; aAcc += pAdd[3]; ++count; }
                    if (x - radius >= 0)          { bAcc -= pRem[0]; gAcc -= pRem[1]; rAcc -= pRem[2]; aAcc -= pRem[3]; --count; }

                    if (count < 1) count = 1;
                }
            }

            // Vertical pass: temp → img
            {
                juce::Image::BitmapData src2(temp, juce::Image::BitmapData::readOnly);
                juce::Image::BitmapData dst2(img, juce::Image::BitmapData::writeOnly);

                for (int x = 0; x < w; ++x)
                {
                    int rAcc = 0, gAcc = 0, bAcc = 0, aAcc = 0;
                    int count = 0;

                    for (int ky = 0; ky <= radius; ++ky)
                    {
                        auto* p = src2.getPixelPointer(x, juce::jlimit(0, h - 1, ky));
                        bAcc += p[0]; gAcc += p[1]; rAcc += p[2]; aAcc += p[3];
                        ++count;
                    }

                    for (int y = 0; y < h; ++y)
                    {
                        auto* out = dst2.getPixelPointer(x, y);
                        out[0] = static_cast<uint8_t>(bAcc / count);
                        out[1] = static_cast<uint8_t>(gAcc / count);
                        out[2] = static_cast<uint8_t>(rAcc / count);
                        out[3] = static_cast<uint8_t>(aAcc / count);

                        int addY = juce::jlimit(0, h - 1, y + radius + 1);
                        int remY = juce::jlimit(0, h - 1, y - radius);

                        auto* pAdd = src2.getPixelPointer(x, addY);
                        auto* pRem = src2.getPixelPointer(x, remY);

                        if (y + radius + 1 <= h - 1) { bAcc += pAdd[0]; gAcc += pAdd[1]; rAcc += pAdd[2]; aAcc += pAdd[3]; ++count; }
                        if (y - radius >= 0)          { bAcc -= pRem[0]; gAcc -= pRem[1]; rAcc -= pRem[2]; aAcc -= pRem[3]; --count; }

                        if (count < 1) count = 1;
                    }
                }
            }
        }
    }
}
