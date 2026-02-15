#pragma once

/**
 * @file PluginRenderReplayer.h
 * @brief Replays render commands from the Python bridge through juce::Graphics.
 *
 * After calling PythonPluginBridge::renderInstance(), pass the resulting
 * command list to PluginRenderReplayer::replay() inside your Component::paint().
 */

#include <JuceHeader.h>
#include "PythonPluginBridge.h"
#include <vector>

namespace PluginRenderReplayer
{
    /**
     * Replay a list of render commands through a juce::Graphics context.
     *
     * Call this inside the paint() method of a CustomPluginComponent:
     *
     * @code
     * void paint(juce::Graphics& g) override {
     *     auto cmds = bridge.renderInstance(instanceId, getWidth(), getHeight(), audioJson);
     *     PluginRenderReplayer::replay(g, cmds);
     * }
     * @endcode
     */
    inline void replay(juce::Graphics& g,
                        const std::vector<PluginRender::RenderCommand>& commands)
    {
        for (auto& cmd : commands)
        {
            auto& p = cmd.params;

            switch (cmd.type)
            {
                case PluginRender::CmdType::Clear:
                {
                    auto col = juce::Colour(static_cast<juce::uint32>((int64_t)p["color"]));
                    g.fillAll(col);
                    break;
                }

                case PluginRender::CmdType::FillRect:
                {
                    g.setColour(juce::Colour(static_cast<juce::uint32>((int64_t)p["color"])));
                    g.fillRect((float)p["x"], (float)p["y"], (float)p["w"], (float)p["h"]);
                    break;
                }

                case PluginRender::CmdType::StrokeRect:
                {
                    g.setColour(juce::Colour(static_cast<juce::uint32>((int64_t)p["color"])));
                    g.drawRect(juce::Rectangle<float>((float)p["x"], (float)p["y"],
                                                       (float)p["w"], (float)p["h"]),
                               (float)p["thickness"]);
                    break;
                }

                case PluginRender::CmdType::FillRoundedRect:
                {
                    g.setColour(juce::Colour(static_cast<juce::uint32>((int64_t)p["color"])));
                    g.fillRoundedRectangle((float)p["x"], (float)p["y"],
                                           (float)p["w"], (float)p["h"],
                                           (float)p["radius"]);
                    break;
                }

                case PluginRender::CmdType::StrokeRoundedRect:
                {
                    g.setColour(juce::Colour(static_cast<juce::uint32>((int64_t)p["color"])));
                    g.drawRoundedRectangle((float)p["x"], (float)p["y"],
                                           (float)p["w"], (float)p["h"],
                                           (float)p["radius"],
                                           (float)p["thickness"]);
                    break;
                }

                case PluginRender::CmdType::FillEllipse:
                {
                    g.setColour(juce::Colour(static_cast<juce::uint32>((int64_t)p["color"])));
                    float cx = (float)p["cx"], cy = (float)p["cy"];
                    float rx = (float)p["rx"], ry = (float)p["ry"];
                    g.fillEllipse(cx - rx, cy - ry, rx * 2, ry * 2);
                    break;
                }

                case PluginRender::CmdType::StrokeEllipse:
                {
                    g.setColour(juce::Colour(static_cast<juce::uint32>((int64_t)p["color"])));
                    float cx = (float)p["cx"], cy = (float)p["cy"];
                    float rx = (float)p["rx"], ry = (float)p["ry"];
                    g.drawEllipse(cx - rx, cy - ry, rx * 2, ry * 2, (float)p["thickness"]);
                    break;
                }

                case PluginRender::CmdType::DrawLine:
                {
                    g.setColour(juce::Colour(static_cast<juce::uint32>((int64_t)p["color"])));
                    g.drawLine((float)p["x1"], (float)p["y1"],
                               (float)p["x2"], (float)p["y2"],
                               (float)p["thickness"]);
                    break;
                }

                case PluginRender::CmdType::DrawPath:
                {
                    auto* pts = p["points"].getArray();
                    if (pts && pts->size() > 1)
                    {
                        juce::Path path;
                        auto first = (*pts)[0];
                        path.startNewSubPath((float)first[0], (float)first[1]);
                        for (int i = 1; i < pts->size(); ++i)
                        {
                            auto pt = (*pts)[i];
                            path.lineTo((float)pt[0], (float)pt[1]);
                        }
                        if ((bool)p["closed"])
                            path.closeSubPath();

                        g.setColour(juce::Colour(static_cast<juce::uint32>((int64_t)p["color"])));
                        g.strokePath(path, juce::PathStrokeType((float)p["thickness"]));
                    }
                    break;
                }

                case PluginRender::CmdType::FillPath:
                {
                    auto* pts = p["points"].getArray();
                    if (pts && pts->size() > 2)
                    {
                        juce::Path path;
                        auto first = (*pts)[0];
                        path.startNewSubPath((float)first[0], (float)first[1]);
                        for (int i = 1; i < pts->size(); ++i)
                        {
                            auto pt = (*pts)[i];
                            path.lineTo((float)pt[0], (float)pt[1]);
                        }
                        path.closeSubPath();

                        g.setColour(juce::Colour(static_cast<juce::uint32>((int64_t)p["color"])));
                        g.fillPath(path);
                    }
                    break;
                }

                case PluginRender::CmdType::DrawText:
                {
                    auto fontInfo = p["font"];
                    juce::Font font(juce::FontOptions(
                        (float)fontInfo["size"]));
                    // Bold/italic would require FontOptions style flags

                    g.setColour(juce::Colour(static_cast<juce::uint32>((int64_t)p["color"])));
                    g.setFont(font);

                    auto alignStr = p["align"].toString();
                    auto justification = juce::Justification::centredLeft;
                    if (alignStr == "center") justification = juce::Justification::centred;
                    else if (alignStr == "right") justification = juce::Justification::centredRight;

                    float tw = (float)p["w"];
                    float th = (float)p["h"];
                    if (tw <= 0) tw = 10000;
                    if (th <= 0) th = (float)fontInfo["size"] + 4;

                    g.drawText(p["text"].toString(),
                               juce::Rectangle<float>((float)p["x"], (float)p["y"], tw, th),
                               justification, false);
                    break;
                }

                case PluginRender::CmdType::SetClip:
                {
                    g.saveState();
                    g.reduceClipRegion(juce::Rectangle<int>(
                        (int)(float)p["x"], (int)(float)p["y"],
                        (int)(float)p["w"], (int)(float)p["h"]));
                    break;
                }

                case PluginRender::CmdType::ResetClip:
                {
                    g.restoreState();
                    break;
                }

                case PluginRender::CmdType::SetOpacity:
                {
                    g.setOpacity((float)p["opacity"]);
                    break;
                }

                case PluginRender::CmdType::DrawArc:
                {
                    g.setColour(juce::Colour(static_cast<juce::uint32>((int64_t)p["color"])));
                    float cx = (float)p["cx"], cy = (float)p["cy"];
                    float r  = (float)p["radius"];
                    juce::Path arc;
                    arc.addArc(cx - r, cy - r, r * 2, r * 2,
                               (float)p["start"], (float)p["end"], true);
                    g.strokePath(arc, juce::PathStrokeType((float)p["thickness"]));
                    break;
                }

                case PluginRender::CmdType::FillArc:
                {
                    g.setColour(juce::Colour(static_cast<juce::uint32>((int64_t)p["color"])));
                    float cx = (float)p["cx"], cy = (float)p["cy"];
                    float r  = (float)p["radius"];
                    juce::Path arc;
                    arc.addArc(cx - r, cy - r, r * 2, r * 2,
                               (float)p["start"], (float)p["end"], true);
                    arc.lineTo(cx, cy);
                    arc.closeSubPath();
                    g.fillPath(arc);
                    break;
                }

                //── Extended commands (v2) ──────────────────────────────────

                case PluginRender::CmdType::FillCircle:
                {
                    g.setColour(juce::Colour(static_cast<juce::uint32>((int64_t)p["color"])));
                    float cx = (float)p["cx"], cy = (float)p["cy"];
                    float r  = (float)p["radius"];
                    g.fillEllipse(cx - r, cy - r, r * 2, r * 2);
                    break;
                }

                case PluginRender::CmdType::StrokeCircle:
                {
                    g.setColour(juce::Colour(static_cast<juce::uint32>((int64_t)p["color"])));
                    float cx = (float)p["cx"], cy = (float)p["cy"];
                    float r  = (float)p["radius"];
                    g.drawEllipse(cx - r, cy - r, r * 2, r * 2, (float)p["thickness"]);
                    break;
                }

                case PluginRender::CmdType::DrawBezier:
                {
                    g.setColour(juce::Colour(static_cast<juce::uint32>((int64_t)p["color"])));
                    juce::Path bezier;
                    bezier.startNewSubPath((float)p["x1"], (float)p["y1"]);
                    bezier.cubicTo(
                        (float)p["cx1"], (float)p["cy1"],
                        (float)p["cx2"], (float)p["cy2"],
                        (float)p["x2"],  (float)p["y2"]);
                    g.strokePath(bezier, juce::PathStrokeType((float)p["thickness"]));
                    break;
                }

                case PluginRender::CmdType::DrawPolyline:
                {
                    auto* pts = p["points"].getArray();
                    if (pts && pts->size() > 1)
                    {
                        juce::Path path;
                        auto first = (*pts)[0];
                        path.startNewSubPath((float)first[0], (float)first[1]);
                        for (int i = 1; i < pts->size(); ++i)
                        {
                            auto pt = (*pts)[i];
                            path.lineTo((float)pt[0], (float)pt[1]);
                        }
                        g.setColour(juce::Colour(static_cast<juce::uint32>((int64_t)p["color"])));
                        g.strokePath(path, juce::PathStrokeType((float)p["thickness"]));
                    }
                    break;
                }

                case PluginRender::CmdType::SaveState:
                {
                    g.saveState();
                    break;
                }

                case PluginRender::CmdType::RestoreState:
                {
                    g.restoreState();
                    break;
                }

                //── Image, gradient, transform, blend (v2 extended) ────────

                case PluginRender::CmdType::DrawImage:
                {
                    // Load image from base64 data ("src") or image key ("key")
                    auto src = p["src"].toString();
                    if (src.isEmpty())
                        src = p["key"].toString();  // Python sends "key" for registered images
                    if (src.isNotEmpty())
                    {
                        juce::MemoryBlock mb;
                        if (mb.fromBase64Encoding(src))
                        {
                            auto img = juce::ImageFileFormat::loadFrom(mb.getData(), mb.getSize());
                            if (img.isValid())
                                g.drawImage(img,
                                    juce::Rectangle<float>((float)p["x"], (float)p["y"],
                                                            (float)p["w"], (float)p["h"]),
                                    juce::RectanglePlacement::stretchToFit);
                        }
                    }
                    break;
                }

                case PluginRender::CmdType::FillGradientRect:
                {
                    float x = (float)p["x"], y = (float)p["y"];
                    float w = (float)p["w"], h = (float)p["h"];

                    if (auto* gradObj = p["gradient"].getDynamicObject())
                    {
                        float gx1 = (float)gradObj->getProperty("x1");
                        float gy1 = (float)gradObj->getProperty("y1");
                        float gx2 = (float)gradObj->getProperty("x2");
                        float gy2 = (float)gradObj->getProperty("y2");
                        bool radial = (bool)gradObj->getProperty("radial");

                        juce::ColourGradient gradient(juce::Colour(), gx1, gy1,
                                                      juce::Colour(), gx2, gy2, radial);
                        gradient.clearColours();

                        if (auto* stops = gradObj->getProperty("stops").getArray())
                        {
                            for (auto& stopVar : *stops)
                            {
                                if (auto* stopArr = stopVar.getArray())
                                {
                                    if (stopArr->size() >= 2)
                                    {
                                        double pos = (double)(*stopArr)[0];
                                        auto col = juce::Colour(
                                            static_cast<juce::uint32>((int64_t)(*stopArr)[1]));
                                        gradient.addColour(pos, col);
                                    }
                                }
                            }
                        }

                        g.setGradientFill(gradient);
                    }
                    else
                    {
                        // Legacy fallback: color1/color2/direction
                        auto c1 = juce::Colour(static_cast<juce::uint32>((int64_t)p["color1"]));
                        auto c2 = juce::Colour(static_cast<juce::uint32>((int64_t)p["color2"]));
                        bool vertical = p["direction"].toString() == "vertical";
                        juce::ColourGradient gradient(
                            c1, x, y,
                            c2, vertical ? x : x + w, vertical ? y + h : y,
                            false);
                        g.setGradientFill(gradient);
                    }

                    g.fillRect(x, y, w, h);
                    break;
                }

                case PluginRender::CmdType::PushTransform:
                {
                    g.saveState();
                    auto origin = g.getClipBounds().toFloat().getCentre();

                    // Parse translate: Python sends as [x,y] tuple or translate_x/translate_y scalars
                    float tx = 0.0f, ty = 0.0f;
                    if (auto* transArr = p["translate"].getArray())
                    {
                        if (transArr->size() >= 2)
                        {
                            tx = (float)(*transArr)[0];
                            ty = (float)(*transArr)[1];
                        }
                    }
                    else
                    {
                        tx = (float)p["translate_x"];
                        ty = (float)p["translate_y"];
                    }

                    float rot = (float)p["rotate"];

                    // Parse scale: Python sends as [sx,sy] tuple or scale_x/scale_y scalars
                    float sx = 1.0f, sy = 1.0f;
                    if (auto* scaleArr = p["scale"].getArray())
                    {
                        if (scaleArr->size() >= 2)
                        {
                            sx = (float)(*scaleArr)[0];
                            sy = (float)(*scaleArr)[1];
                        }
                    }
                    else
                    {
                        sx = p.hasProperty("scale_x") ? (float)p["scale_x"] : 1.0f;
                        sy = p.hasProperty("scale_y") ? (float)p["scale_y"] : 1.0f;
                    }

                    auto transform = juce::AffineTransform::translation(tx, ty)
                        .rotated(rot, origin.x + tx, origin.y + ty)
                        .scaled(sx, sy, origin.x + tx, origin.y + ty);
                    g.addTransform(transform);
                    break;
                }

                case PluginRender::CmdType::PopTransform:
                {
                    g.restoreState();
                    break;
                }

                case PluginRender::CmdType::SetBlendMode:
                {
                    // JUCE's Graphics doesn't support blend modes directly,
                    // but we can approximate Additive with setOpacity.
                    // Full blend mode support requires OpenGL (handled in CustomPluginComponent).
                    break;
                }

                //── GPU shader commands (v3) ── handled by OpenGL in CustomPluginComponent
                case PluginRender::CmdType::DrawShader:
                case PluginRender::CmdType::DrawCustomShader:
                    break; // Intentionally no-op: shaders run on GPU, not via juce::Graphics

                default:
                    break;
            }
        }
    }
}
