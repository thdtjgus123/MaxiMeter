#pragma once

#include <JuceHeader.h>

//==============================================================================
/// A saved canvas state with a thumbnail and display name.
struct VJScene
{
    juce::String name;
    juce::var    canvasState;   ///< Serialised via ProjectSerializer::itemToVar
    juce::Image  thumbnail;     ///< 160x90 preview image
    juce::Uuid   id;

    VJScene() : id(juce::Uuid()) {}

    juce::var toVar() const
    {
        auto* obj = new juce::DynamicObject();
        obj->setProperty("id",   id.toString());
        obj->setProperty("name", name);
        obj->setProperty("state", canvasState);
        // Thumbnail serialisation as base64 PNG
        if (thumbnail.isValid())
        {
            juce::MemoryOutputStream png;
            juce::PNGImageFormat fmt;
            fmt.writeImageToStream(thumbnail, png);
            obj->setProperty("thumbnail",
                juce::Base64::toBase64(png.getData(), png.getDataSize()));
        }
        return juce::var(obj);
    }

    static VJScene fromVar(const juce::var& v)
    {
        VJScene s;
        if (auto* obj = v.getDynamicObject())
        {
            if (obj->hasProperty("id"))
                s.id = juce::Uuid(obj->getProperty("id").toString());
            s.name        = obj->getProperty("name").toString();
            s.canvasState = obj->getProperty("state");
            if (obj->hasProperty("thumbnail"))
            {
                juce::MemoryBlock mb;
                juce::MemoryOutputStream mos(mb, false);
                juce::Base64::convertFromBase64(mos, obj->getProperty("thumbnail").toString());
                mos.flush();
                s.thumbnail = juce::ImageFileFormat::loadFrom(mb.getData(), mb.getSize());
            }
        }
        return s;
    }
};
