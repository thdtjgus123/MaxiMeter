#pragma once
#include <JuceHeader.h>
#include "SkinnedTitleBarLookAndFeel.h"
#include "ThemeManager.h"

class DocumentationWindow : public juce::DocumentWindow
{
public:
    DocumentationWindow()
        : juce::DocumentWindow("MaxiMeter Documentation",
                               ThemeManager::getInstance().getPalette().windowBg,
                               juce::DocumentWindow::closeButton)
    {
        setLookAndFeel(&titleBarLnf_);
        setUsingNativeTitleBar(false);
        setTitleBarHeight(32);
        auto* content = new DocContent();
        content->setSize(960, 720);
        setContentOwned(content, true);
        centreWithSize(960, 720);
        setVisible(true);
    }

    ~DocumentationWindow() override
    {
        setLookAndFeel(nullptr);
    }

    void closeButtonPressed() override { setVisible(false); }

private:
    SkinnedTitleBarLookAndFeel titleBarLnf_;

    // ======================================================================
    //  MarkdownView  -  renders parsed markdown blocks via juce::Graphics
    // ======================================================================
    class MarkdownView : public juce::Component
    {
    public:
        MarkdownView()
        {
            setMouseCursor(juce::MouseCursor::IBeamCursor);
            setWantsKeyboardFocus(true);
        }

        // --- theme helper ---
        struct DocTheme
        {
            juce::Colour bg, h1, h2, h3, para, codeBg, codeBorder, codeText, hr, bullet;
            juce::Colour selBg, searchBg;

            static DocTheme fromPalette()
            {
                auto& pal = ThemeManager::getInstance().getPalette();
                bool light = (pal.panelBg.getBrightness() > 0.5f);
                DocTheme t;
                t.bg         = pal.windowBg;
                t.para       = pal.bodyText;
                if (light)
                {
                    t.h1         = juce::Colour(0xFF1A5FB4);
                    t.h2         = juce::Colour(0xFF26A269);
                    t.h3         = juce::Colour(0xFF813D9C);
                    t.codeBg     = pal.panelBg.darker(0.05f);
                    t.codeBorder = pal.border;
                    t.codeText   = juce::Colour(0xFF2A2A2A);
                    t.hr         = pal.border;
                    t.bullet     = t.h1;
                    t.selBg      = juce::Colour(0x30448AFF);
                    t.searchBg   = juce::Colour(0x30FFC107);
                }
                else
                {
                    t.h1         = juce::Colour(0xFF7AA2F7);
                    t.h2         = juce::Colour(0xFF9ECE6A);
                    t.h3         = juce::Colour(0xFFBB9AF7);
                    t.codeBg     = juce::Colour(0xFF1A1A2E);
                    t.codeBorder = juce::Colour(0xFF2A2A44);
                    t.codeText   = juce::Colour(0xFFD4D4E8);
                    t.hr         = juce::Colour(0xFF3A3A5C);
                    t.bullet     = juce::Colour(0xFF7AA2F7);
                    t.selBg      = juce::Colour(0x30448AFF);
                    t.searchBg   = juce::Colour(0x203A7BFF);
                }
                return t;
            }
        };

        void setMarkdown(const juce::String& md)
        {
            parseBlocks(md);
            dirty = true;
            repaint();
        }

        void setHighlightTerm(const juce::String& term)
        {
            hlTerm = term.toLowerCase();
            matchIndices.clear();
            if (hlTerm.isNotEmpty())
                for (int i = 0; i < blocks.size(); ++i)
                    if (blocks[i].text.toLowerCase().contains(hlTerm))
                        matchIndices.add(i);
            repaint();
        }

        int  getNumMatches() const { return matchIndices.size(); }
        float getMatchY(int mi) const
        {
            if (mi < 0 || mi >= matchIndices.size()) return 0.0f;
            int bi = matchIndices[mi];
            return (bi < blockY.size()) ? blockY[bi] : 0.0f;
        }

        // --- selection & copy ---
        void mouseDown(const juce::MouseEvent& e) override
        {
            grabKeyboardFocus();
            selStart = selEnd = blockAtY((float)e.y);
            dragging = true;
            repaint();
        }
        void mouseDrag(const juce::MouseEvent& e) override
        {
            if (!dragging) return;
            selEnd = blockAtY((float)e.y);
            repaint();
            // auto-scroll: notify parent viewport
            if (auto* vp = findParentComponentOfClass<juce::Viewport>())
            {
                auto rel = e.getEventRelativeTo(vp);
                if (rel.y < 0)
                    vp->setViewPosition(0, juce::jmax(0, vp->getViewPositionY() - 20));
                else if (rel.y > vp->getHeight())
                    vp->setViewPosition(0, vp->getViewPositionY() + 20);
            }
        }
        void mouseUp(const juce::MouseEvent&) override { dragging = false; }

        bool keyPressed(const juce::KeyPress& k) override
        {
            if (k == juce::KeyPress('c', juce::ModifierKeys::ctrlModifier, 0))
            { copySelection(); return true; }
            if (k == juce::KeyPress('a', juce::ModifierKeys::ctrlModifier, 0))
            { selStart = 0; selEnd = blocks.size() - 1; repaint(); return true; }
            return false;
        }

        bool hasSelection() const
        {
            return selStart >= 0 && selEnd >= 0 && selStart != selEnd;
        }

        void rebuild()
        {
            float w = (float)getWidth() - pad * 2.0f;
            if (w < 10.0f) return;
            blockY.clear();
            float y = pad;
            for (int i = 0; i < blocks.size(); ++i)
            {
                blockY.add(y);
                y += blockH(blocks[i], w);
            }
            total = y + pad;
            setSize(getWidth(), juce::jmax(1, (int)total));
            dirty = false;
        }

        void resized() override { dirty = true; }

        void paint(juce::Graphics& g) override
        {
            if (dirty) rebuild();
            auto dt = DocTheme::fromPalette();
            g.fillAll(dt.bg);
            float w = (float)getWidth() - pad * 2.0f;
            auto clip = g.getClipBounds().toFloat();
            int sLo = juce::jmin(selStart, selEnd);
            int sHi = juce::jmax(selStart, selEnd);
            for (int i = 0; i < blocks.size(); ++i)
            {
                float y0 = blockY[i];
                float h0 = (i + 1 < blocks.size()) ? blockY[i + 1] - y0 : total - pad - y0;
                if (y0 + h0 < clip.getY() || y0 > clip.getBottom()) continue;
                // selection highlight
                if (i >= sLo && i <= sHi && sLo != sHi)
                {
                    g.setColour(dt.selBg);
                    g.fillRect(pad - 4, y0 - 1, w + 8, h0 + 1);
                }
                // search highlight
                if (matchIndices.contains(i))
                {
                    g.setColour(dt.searchBg);
                    g.fillRoundedRectangle(pad - 4, y0 - 2, w + 8, h0 + 2, 4.0f);
                }
                paintBlock(g, blocks[i], pad, y0, w, dt);
            }
        }

    private:
        struct Block
        {
            enum Type { H1, H2, H3, PARA, CODE, BULLET, HR, SPACER };
            Type type;
            juce::String text;
        };

        juce::Array<Block> blocks;
        juce::Array<float> blockY;
        juce::Array<int>   matchIndices;
        juce::String       hlTerm;
        float total = 0, pad = 24.0f;
        bool  dirty = true;
        int   selStart = -1, selEnd = -1;
        bool  dragging = false;

        int blockAtY(float y) const
        {
            for (int i = blocks.size() - 1; i >= 0; --i)
                if (i < blockY.size() && y >= blockY[i]) return i;
            return 0;
        }

        void copySelection() const
        {
            int lo = juce::jmin(selStart, selEnd);
            int hi = juce::jmax(selStart, selEnd);
            if (lo < 0) lo = 0;
            if (hi < 0 || hi >= blocks.size()) hi = blocks.size() - 1;
            juce::String txt;
            for (int i = lo; i <= hi; ++i)
            {
                auto& b = blocks[i];
                if (b.type == Block::HR) txt << "---\n";
                else if (b.type == Block::SPACER) txt << "\n";
                else if (b.type == Block::BULLET) txt << "  - " << b.text << "\n";
                else txt << b.text << "\n";
            }
            juce::SystemClipboard::copyTextToClipboard(txt);
        }

        // --- fonts (non-deprecated JUCE 8 API) ---
        static juce::Font fH1() { return juce::Font(juce::FontOptions(26.0f, juce::Font::bold)); }
        static juce::Font fH2() { return juce::Font(juce::FontOptions(21.0f, juce::Font::bold)); }
        static juce::Font fH3() { return juce::Font(juce::FontOptions(17.0f, juce::Font::bold)); }
        static juce::Font fP()  { return juce::Font(juce::FontOptions(14.5f)); }
        static juce::Font fC()  { return juce::Font(juce::FontOptions("Consolas", 13.5f, juce::Font::plain)); }

        // --- parse ---
        void parseBlocks(const juce::String& md)
        {
            blocks.clear();
            auto lines = juce::StringArray::fromLines(md);
            bool inCode = false;
            juce::String code;
            for (auto& ln : lines)
            {
                auto t = ln.trimStart();
                if (t.startsWith("```"))
                {
                    if (inCode) { blocks.add({ Block::CODE, code.trimEnd() }); code.clear(); }
                    inCode = !inCode;
                    continue;
                }
                if (inCode) { code << ln << "\n"; continue; }
                if (t.isEmpty())        { blocks.add({ Block::SPACER, {} }); continue; }
                if (t.startsWith("### "))  blocks.add({ Block::H3, t.substring(4) });
                else if (t.startsWith("## "))  blocks.add({ Block::H2, t.substring(3) });
                else if (t.startsWith("# "))   blocks.add({ Block::H1, t.substring(2) });
                else if (t.startsWith("---") || t.startsWith("***")) blocks.add({ Block::HR, {} });
                else if (t.startsWith("- ") || t.startsWith("* "))   blocks.add({ Block::BULLET, t.substring(2) });
                else blocks.add({ Block::PARA, ln });
            }
            if (inCode && code.isNotEmpty())
                blocks.add({ Block::CODE, code.trimEnd() });
        }

        // --- measure ---
        static float txH(const juce::String& s, const juce::Font& f, float w)
        {
            if (s.isEmpty()) return f.getHeight();
            juce::AttributedString as;
            as.append(s, f, juce::Colours::white);
            as.setWordWrap(juce::AttributedString::byWord);
            juce::TextLayout tl;
            tl.createLayout(as, juce::jmax(10.0f, w));
            return tl.getHeight();
        }

        float blockH(const Block& b, float w) const
        {
            switch (b.type)
            {
                case Block::H1:     return txH(b.text, fH1(), w) + 20.0f;
                case Block::H2:     return txH(b.text, fH2(), w) + 16.0f;
                case Block::H3:     return txH(b.text, fH3(), w) + 12.0f;
                case Block::PARA:   return txH(b.text, fP(),  w) + 6.0f;
                case Block::BULLET: return txH(b.text, fP(),  w - 20.0f) + 5.0f;
                case Block::CODE:   return txH(b.text, fC(),  w - 28.0f) + 20.0f;
                case Block::HR:     return 16.0f;
                case Block::SPACER: return 10.0f;
                default:            return 8.0f;
            }
        }

        // --- draw helpers ---
        static void drawTx(juce::Graphics& g, const juce::String& s,
                           const juce::Font& f, juce::Colour c, float x, float y, float w)
        {
            juce::AttributedString as;
            as.append(s, f, c);
            as.setWordWrap(juce::AttributedString::byWord);
            juce::TextLayout tl;
            tl.createLayout(as, juce::jmax(10.0f, w));
            tl.draw(g, { x, y, w, tl.getHeight() });
        }

        void paintBlock(juce::Graphics& g, const Block& b, float x, float y, float w, const DocTheme& dt) const
        {
            switch (b.type)
            {
                case Block::H1:
                    drawTx(g, b.text, fH1(), dt.h1, x, y + 6, w);
                    g.setColour(dt.hr);
                    g.drawHorizontalLine((int)(y + txH(b.text, fH1(), w) + 12), x, x + w);
                    break;
                case Block::H2:
                    drawTx(g, b.text, fH2(), dt.h2, x, y + 4, w);
                    break;
                case Block::H3:
                    drawTx(g, b.text, fH3(), dt.h3, x, y + 2, w);
                    break;
                case Block::PARA:
                    drawTx(g, b.text, fP(), dt.para, x, y + 2, w);
                    break;
                case Block::BULLET:
                    g.setColour(dt.bullet);
                    g.fillEllipse(x + 4, y + 7, 5, 5);
                    drawTx(g, b.text, fP(), dt.para, x + 20, y + 2, w - 20);
                    break;
                case Block::CODE:
                {
                    float ch = txH(b.text, fC(), w - 28) + 16;
                    g.setColour(dt.codeBg);
                    g.fillRoundedRectangle(x, y + 2, w, ch, 6.0f);
                    g.setColour(dt.codeBorder);
                    g.drawRoundedRectangle(x, y + 2, w, ch, 6.0f, 1.0f);
                    drawTx(g, b.text, fC(), dt.codeText, x + 14, y + 10, w - 28);
                    break;
                }
                case Block::HR:
                    g.setColour(dt.hr);
                    g.drawHorizontalLine((int)(y + 8), x, x + w);
                    break;
                case Block::SPACER: break;
            }
        }
    };

    // ======================================================================
    //  DocContent  -  search bar + viewport holding MarkdownView
    // ======================================================================
    class DocContent : public juce::Component,
                      private ThemeManager::Listener
    {
    public:
        DocContent()
        {
            applyThemeColours();
            sb.setTextToShowWhenEmpty("Search (Ctrl+F)", juce::Colours::grey);
            sb.onTextChange = [this] { doSearch(); };
            addAndMakeVisible(sb);

            prev.setButtonText("<");  next.setButtonText(">");
            prev.onClick = [this] { nav(-1); };
            next.onClick = [this] { nav(+1); };
            addAndMakeVisible(prev); addAndMakeVisible(next);

            ml.setColour(juce::Label::textColourId, ThemeManager::getInstance().getPalette().dimText);
            ml.setJustificationType(juce::Justification::centredRight);
            addAndMakeVisible(ml);

            vp.setViewedComponent(&mv, false);
            vp.setScrollBarsShown(true, false);
            addAndMakeVisible(vp);

            mv.setMarkdown(buildContent());
            ThemeManager::getInstance().addListener(this);
        }

        ~DocContent() override
        {
            ThemeManager::getInstance().removeListener(this);
        }

        void paint(juce::Graphics& g) override
        {
            g.fillAll(ThemeManager::getInstance().getPalette().panelBg);
        }

        void resized() override
        {
            auto a = getLocalBounds().reduced(4);
            auto top = a.removeFromTop(28);
            next.setBounds(top.removeFromRight(28).reduced(2));
            prev.setBounds(top.removeFromRight(28).reduced(2));
            ml.setBounds(top.removeFromRight(100));
            sb.setBounds(top);
            a.removeFromTop(4);
            vp.setBounds(a);
            mv.setSize(a.getWidth() - vp.getScrollBarThickness(), 10);
            mv.rebuild();
        }

        bool keyPressed(const juce::KeyPress& k) override
        {
            if (k == juce::KeyPress('f', juce::ModifierKeys::ctrlModifier, 0))
            { sb.grabKeyboardFocus(); sb.selectAll(); return true; }
            if (k == juce::KeyPress::escapeKey)
            { sb.clear(); ml.setText("", juce::dontSendNotification); return true; }
            if (k == juce::KeyPress::F3Key)
            { nav(k.getModifiers().isShiftDown() ? -1 : 1); return true; }
            return false;
        }

    private:
        juce::TextEditor sb;
        juce::TextButton prev, next;
        juce::Label ml;
        juce::Viewport vp;
        MarkdownView mv;
        int mi = -1;

        void themeChanged(AppTheme) override
        {
            applyThemeColours();
            mv.repaint();
            repaint();
        }

        void applyThemeColours()
        {
            auto& pal = ThemeManager::getInstance().getPalette();
            bool light = (pal.panelBg.getBrightness() > 0.5f);
            sb.setColour(juce::TextEditor::backgroundColourId,     light ? pal.panelBg.darker(0.05f) : juce::Colour(0xFF252540));
            sb.setColour(juce::TextEditor::textColourId,           pal.bodyText);
            sb.setColour(juce::TextEditor::outlineColourId,        pal.border);
            sb.setColour(juce::TextEditor::focusedOutlineColourId, pal.accent);
            ml.setColour(juce::Label::textColourId, pal.dimText);
        }

        void doSearch()
        {
            auto q = sb.getText().trim();
            mv.setHighlightTerm(q);
            mi = -1;
            int n = mv.getNumMatches();
            if (q.isEmpty()) { ml.setText("", juce::dontSendNotification); return; }
            if (n == 0) { ml.setText("0 results", juce::dontSendNotification); return; }
            mi = 0; scrollTo();
            ml.setText(juce::String(n) + " found", juce::dontSendNotification);
        }
        void nav(int d)
        {
            int n = mv.getNumMatches(); if (n == 0) return;
            mi = (mi + d + n) % n; scrollTo();
            ml.setText(juce::String(mi + 1) + "/" + juce::String(n), juce::dontSendNotification);
        }
        void scrollTo()
        {
            float y = mv.getMatchY(mi);
            vp.setViewPosition(0, juce::jmax(0, (int)y - 50));
        }

        // ==================================================================
        //  Full Documentation Content (Markdown)
        // ==================================================================
        static juce::String buildContent()
        {
            return juce::String(
            "# MaxiMeter Custom Component Framework\n"
            "Version 0.2.0  |  February 2026\n"
            "\n---\n\n"

            "## Table of Contents\n"
            "- Overview\n"
            "- Quick Start\n"
            "- Architecture\n"
            "- BaseComponent API\n"
            "- RenderContext (Drawing API)\n"
            "- GPU Shader System\n"
            "- SharedMemory Audio Transport\n"
            "- AudioData (Audio Analysis)\n"
            "- Properties System\n"
            "- Manifest\n"
            "- Color & Gradient\n"
            "- Examples\n"
            "- Error Recovery & Auto-Restart\n"
            "- Import & Deployment\n"
            "- Hot Reload\n"
            "- Troubleshooting\n"
            "- FAQ\n"
            "\n---\n\n"

            // ------ 1. Overview ------
            "## 1. Overview\n\n"
            "The MaxiMeter Custom Component Framework lets you create your own audio\n"
            "visualisation components using Python. Custom components appear alongside\n"
            "built-in meters in the TOOLBOX panel and can be dragged onto the canvas\n"
            "just like any other component.\n\n"
            "### Key Features\n\n"
            "- Write visualisations in **pure Python** (no C++ needed)\n"
            "- Full access to real-time audio analysis data (FFT, RMS, LUFS)\n"
            "- Rich drawing API: rectangles, ellipses, paths, text, gradients\n"
            "- Declarative property system (auto-generates Property Panel UI)\n"
            "- Hot-reload: edit code and see changes instantly\n"
            "- State persistence: component state saved with project files\n"
            "- Zero external dependencies (pure Python 3.10+)\n"
            "\n---\n\n"

            // ------ 2. Quick Start ------
            "## 2. Quick Start\n\n"
            "- Create a new .py file in CustomComponents/plugins/\n"
            "- Import the framework classes\n"
            "- Define a class inheriting BaseComponent\n"
            "- Implement get_manifest() and on_render()\n"
            "- Restart MaxiMeter -- your component appears in TOOLBOX!\n\n"
            "### Minimal Example\n\n"
            "```\n"
            "from CustomComponents import (\n"
            "    BaseComponent, Manifest, Category,\n"
            "    RenderContext, AudioData, Color\n"
            ")\n"
            "\n"
            "class MyMeter(BaseComponent):\n"
            "    @staticmethod\n"
            "    def get_manifest():\n"
            "        return Manifest(\n"
            "            id=\"com.myname.my_meter\",\n"
            "            name=\"My Meter\",\n"
            "            category=Category.METER,\n"
            "            default_size=(300, 200),\n"
            "        )\n"
            "\n"
            "    def on_render(self, ctx, audio):\n"
            "        ctx.clear(Color.black())\n"
            "        level = audio.left.rms_linear\n"
            "        w = ctx.width * level\n"
            "        ctx.fill_rect(0, 0, w, ctx.height, Color(0, 200, 100))\n"
            "```\n"
            "\n---\n\n"

            // ------ 3. Architecture ------
            "## 3. Architecture\n\n"
            "The framework uses a subprocess-based architecture with two\n"
            "data transports:\n\n"
            "```\n"
            "C++ Host (JUCE / OpenGL)\n"
            "  |--- SharedMemory (12KB mmap) ---> Python Runtime\n"
            "  |--- Win32 Pipe (JSON fallback) -> Python Runtime\n"
            "  <--- Render Commands (JSON) ------- Python Runtime\n"
            "```\n\n"
            "### Audio Data Transport\n\n"
            "- SharedMemory (preferred): ~5 us per frame, zero-copy binary\n"
            "  transfer via a 12KB mmap region (MaxiMeter_AudioSHM).\n"
            "- JSON over Pipe (fallback): ~500 us per frame, used when SHM\n"
            "  is unavailable. Transparent to plugin code.\n\n"
            "### Rendering Pipeline\n\n"
            "- GPU shader commands (particles, fractals, bloom, etc.) are\n"
            "  executed via JUCE OpenGL. The host auto-detects GPU capability:\n"
            "  - OpenGL 4.3+: compute shader path with SSBO\n"
            "  - OpenGL 3.3:  fragment shader fallback\n\n"
            "### Flow Per Frame\n\n"
            "- C++ host writes audio data to SharedMemory (or sends JSON)\n"
            "- Python bridge reads audio data (~5 us via SHM)\n"
            "- Python calls the component's on_render(ctx, audio)\n"
            "- RenderContext records draw commands into a command buffer\n"
            "- Host receives the command list and replays through\n"
            "  juce::Graphics (2D) or OpenGL (shader commands)\n\n"
            "### Error Recovery\n\n"
            "- If the Python process crashes, the host auto-restarts it\n"
            "  (up to 5 retries). See section 13 for details.\n"
            "\n---\n\n"

            // ------ 4. BaseComponent ------
            "## 4. BaseComponent API\n\n"
            "BaseComponent is the abstract base class all custom components must\n"
            "inherit from.\n\n"
            "### Lifecycle Methods\n\n"
            "```\n"
            "__init__()              Constructor (do minimal work)\n"
            "on_init()               Called once after creation; load resources here\n"
            "on_render(ctx, audio)   Called every frame (~60 fps)\n"
            "on_resize(w, h)         Called when the component is resized on canvas\n"
            "on_property_changed()   Called when a property value changes\n"
            "on_destroy()            Cleanup when removed from canvas\n"
            "```\n\n"
            "### Required Overrides\n\n"
            "```\n"
            "@staticmethod\n"
            "def get_manifest() -> Manifest\n"
            "\n"
            "def on_render(self, ctx: RenderContext, audio: AudioData)\n"
            "```\n\n"
            "### Optional Overrides\n\n"
            "- on_init(), on_resize(w, h), on_property_changed(key, val)\n"
            "- on_mouse_down(x, y, btn), on_mouse_move(x, y), on_mouse_up(x, y, btn)\n"
            "- on_destroy(), get_properties() -> List[Property]\n\n"
            "### State Persistence\n\n"
            "```\n"
            "self.state.set(key, value)      Store persistent state\n"
            "self.state.get(key, default)    Read persistent state\n"
            "# State is saved/restored with project files automatically\n"
            "```\n"
            "\n---\n\n"

            // ------ 5. RenderContext ------
            "## 5. RenderContext (Drawing API)\n\n"
            "RenderContext is the drawing surface passed to on_render().\n"
            "Coordinates: (0,0) = top-left, (ctx.width, ctx.height) = bottom-right.\n\n"
            "### Properties\n\n"
            "```\n"
            "ctx.width       Current component width in pixels\n"
            "ctx.height      Current component height in pixels\n"
            "```\n\n"
            "### Background\n\n"
            "```\n"
            "ctx.clear(color)\n"
            "```\n\n"
            "### Rectangles\n\n"
            "```\n"
            "ctx.fill_rect(x, y, w, h, color)\n"
            "ctx.stroke_rect(x, y, w, h, color, thickness=1.0)\n"
            "ctx.fill_rounded_rect(x, y, w, h, radius, color)\n"
            "ctx.stroke_rounded_rect(x, y, w, h, radius, color, thickness)\n"
            "```\n\n"
            "### Ellipses\n\n"
            "```\n"
            "ctx.fill_ellipse(cx, cy, rx, ry, color)\n"
            "ctx.stroke_ellipse(cx, cy, rx, ry, color, thickness=1.0)\n"
            "```\n\n"
            "### Lines\n\n"
            "```\n"
            "ctx.draw_line(x1, y1, x2, y2, color, thickness=1.0)\n"
            "```\n\n"
            "### Paths\n\n"
            "```\n"
            "ctx.draw_path(points, color, thickness, closed=False)\n"
            "ctx.fill_path(points, color)\n"
            "# points = [(x1,y1), (x2,y2), ...]\n"
            "```\n\n"
            "### Arcs\n\n"
            "```\n"
            "ctx.draw_arc(cx, cy, radius, start_angle, end_angle, color, thickness)\n"
            "ctx.fill_arc(cx, cy, radius, start_angle, end_angle, color)\n"
            "# Angles in radians. 0 = right, positive = clockwise.\n"
            "```\n\n"
            "### Text\n\n"
            "```\n"
            "ctx.draw_text(text, x, y, w, h, color, font, align)\n"
            "# font  = Font(family=\"Arial\", size=14, bold=False, italic=False)\n"
            "# align = TextAlign.LEFT | CENTER | RIGHT\n"
            "```\n\n"
            "### Images\n\n"
            "```\n"
            "ctx.draw_image(key, x, y, w, h, opacity=1.0)\n"
            "```\n\n"
            "### Gradients\n\n"
            "```\n"
            "ctx.fill_gradient_rect(x, y, w, h, gradient)\n"
            "# Gradient.linear(x1, y1, x2, y2, [GradientStop(...)])\n"
            "# Gradient.radial(cx, cy, radius, [GradientStop(...)])\n"
            "```\n\n"
            "### State Stack\n\n"
            "```\n"
            "ctx.set_clip(x, y, w, h)           Clip drawing to rectangle\n"
            "ctx.reset_clip()                    Remove clipping\n"
            "ctx.push_transform(tx, ty, rot, sx, sy)\n"
            "ctx.pop_transform()\n"
            "ctx.set_opacity(0.0 ~ 1.0)\n"
            "ctx.set_blend_mode(BlendMode.NORMAL | ADDITIVE | MULTIPLY | SCREEN)\n"
            "```\n\n"
            "### GPU Shader Commands\n\n"
            "```\n"
            "ctx.draw_shader(shader_id, uniforms={}, **kwargs)\n"
            "# Execute a GLSL shader pass. See section 6 for built-in shaders.\n"
            "```\n\n"
            "### Shader Convenience Helpers\n\n"
            "```\n"
            "ctx.particles(count=1000, speed=1.0, size=3.0, color=(r,g,b))\n"
            "ctx.waveform_3d(rotate_x=0.3, depth=20.0, line_width=2.0)\n"
            "ctx.spectrum_3d(rotate_x=0.4, bar_width=0.8, height=1.5)\n"
            "ctx.plasma(speed=1.0, scale=1.0, complexity=3.0)\n"
            "ctx.voronoi(cells=8.0, speed=1.0)\n"
            "ctx.mandelbrot(zoom=1.0, center_x=-0.5, max_iter=256)\n"
            "ctx.julia_set(zoom=1.0, c_real=-0.7, c_imag=0.27, animate=True)\n"
            "ctx.bloom(intensity=1.5, threshold=0.6, radius=4.0)\n"
            "ctx.blur(radius=5.0, direction=(1.0, 1.0))\n"
            "ctx.glitch(intensity=0.5, block_size=0.03, chromatic=0.01)\n"
            "ctx.chromatic_aberration(intensity=0.005, angle=0.0)\n"
            "```\n"
            "\n---\n\n"

            // ------ 6. GPU Shader System ------
            "## 6. GPU Shader System\n\n"
            "MaxiMeter includes 12 built-in GLSL shaders accessible from Python.\n\n"
            "### Built-in Shaders\n\n"
            "| Shader ID   | Type         | Description                     |\n"
            "|-------------|--------------|----------------------------------|\n"
            "| particles   | Visualizer   | Audio-reactive particle system   |\n"
            "| waveform3d  | Visualizer   | 3-D waveform with depth rows     |\n"
            "| spectrum3d  | Visualizer   | 3-D frequency bar graph          |\n"
            "| plasma      | Visualizer   | Animated plasma pattern          |\n"
            "| voronoi     | Visualizer   | Voronoi cell pattern             |\n"
            "| mandelbrot  | Visualizer   | Mandelbrot fractal               |\n"
            "| julia       | Visualizer   | Julia-set fractal                |\n"
            "| bloom       | Post-process | Glow/bloom effect                |\n"
            "| blur        | Post-process | Gaussian blur                    |\n"
            "| glitch      | Post-process | Digital glitch effect            |\n"
            "| chromatic   | Post-process | Chromatic aberration             |\n\n"
            "### Compute Shader Particles (OpenGL 4.3+)\n\n"
            "When the host detects OpenGL 4.3 or higher, the 'particles' shader\n"
            "uses a compute shader backed by an SSBO for persistent state:\n\n"
            "| Feature         | Compute (GL 4.3+) | Fragment (GL 3.3)    |\n"
            "|-----------------|--------------------|-----------------------|\n"
            "| Max particles   | 100,000            | 500                   |\n"
            "| State           | Persistent (SSBO)  | Stateless (per-frame) |\n"
            "| Physics         | Velocity+gravity   | Position approx.      |\n"
            "| Workgroup size  | 256 threads        | N/A                   |\n\n"
            "Path selection is fully automatic. Your Python code just calls\n"
            "ctx.particles(count=50000) and the host picks the best backend.\n\n"
            "### Using draw_shader() Directly\n\n"
            "For full control you can pass raw uniforms:\n\n"
            "```\n"
            "ctx.draw_shader('particles',\n"
            "    u_count=10000, u_speed=2.0, u_size=4.0,\n"
            "    u_colorR=1.0, u_colorG=0.4, u_colorB=0.1)\n"
            "```\n\n"
            "### Custom GLSL Shaders\n\n"
            "You can write your own GLSL shaders and pass the source code as\n"
            "a string.  The host compiles and caches them automatically.\n\n"
            "**Fragment-only** shaders use `#version 330 core`.\n"
            "**Compute + Fragment** shaders both use `#version 430 core`.\n\n"
            "#### Host-provided uniforms (fragment)\n\n"
            "```\n"
            "u_time        (float)     Elapsed time in seconds\n"
            "u_resolution  (vec2)      Viewport width, height\n"
            "u_frame       (int)       Frame counter\n"
            "u_audioData   (sampler2D) Row 0=spectrum, Row 1=waveform\n"
            "u_spectrumSize(int)       Number of spectrum bins\n"
            "u_waveformSize(int)       Number of waveform samples\n"
            "u_texture     (sampler2D) Back-buffer (previous 2D draws)\n"
            "```\n\n"
            "#### Host-provided uniforms (compute)\n\n"
            "```\n"
            "u_time        (float)     Elapsed time in seconds\n"
            "u_deltaTime   (float)     Seconds since last frame\n"
            "u_resolution  (vec2)      Viewport width, height\n"
            "u_audioData   (sampler2D) Row 0=spectrum, Row 1=waveform\n"
            "```\n\n"
            "#### Example: Fragment-only\n\n"
            "```\n"
            "MY_SHADER = '''\n"
            "#version 330 core\n"
            "out vec4 fragColor;\n"
            "in vec2 vTexCoord;\n"
            "uniform float u_time;\n"
            "uniform vec2  u_resolution;\n"
            "uniform sampler2D u_audioData;\n"
            "uniform float u_intensity;\n"
            "\n"
            "void main() {\n"
            "    vec2 uv = vTexCoord;\n"
            "    float bin = texture(u_audioData,\n"
            "        vec2(uv.x, 0.0)).r;\n"
            "    fragColor = vec4(bin * u_intensity,\n"
            "        bin * 0.5, uv.y, 1.0);\n"
            "}\n"
            "'''\n"
            "\n"
            "def on_render(self, ctx, audio):\n"
            "    ctx.draw_custom_shader(\n"
            "        MY_SHADER,\n"
            "        cache_key='my_audio_viz',\n"
            "        u_intensity=2.0,\n"
            "    )\n"
            "```\n\n"
            "#### Example: Compute + Fragment\n\n"
            "```\n"
            "N = 1000\n"
            "ctx.draw_custom_shader(\n"
            "    FRAG_SRC,\n"
            "    compute_source=COMPUTE_SRC,\n"
            "    buffer_size=N * 32,\n"
            "    num_groups=((N+255)//256, 1, 1),\n"
            "    cache_key='my_particles',\n"
            "    u_count=float(N),\n"
            ")\n"
            "```\n\n"
            "The compute shader gets an SSBO at binding 0 that persists\n"
            "across frames — perfect for particle systems, physics, etc.\n\n"
            "#### Rules\n\n"
            "- Fragment-only: #version 330 core\n"
            "- Compute+Fragment: #version 430 core for both\n"
            "- Output to 'out vec4 fragColor'\n"
            "- Use vTexCoord (vec2, 0-1) for UV coordinates\n"
            "- Provide a stable cache_key to avoid recompilation\n"
            "- Custom uniforms are limited to float type\n"
            "\n---\n\n"

            // ------ 7. SharedMemory Audio Transport ------
            "## 7. SharedMemory Audio Transport\n\n"
            "Audio data is written by the C++ host into a 12 KB memory-mapped\n"
            "region (MaxiMeter_AudioSHM) every frame and read by Python with\n"
            "near-zero overhead.\n\n"
            "### Performance Comparison\n\n"
            "| Transport     | Latency   | Method                |\n"
            "|---------------|-----------|-----------------------|\n"
            "| SharedMemory  | ~5 us     | mmap binary read      |\n"
            "| JSON over Pipe| ~500 us   | serialize/deserialize |\n\n"
            "SharedMemory is used automatically when available — no plugin\n"
            "code changes needed.\n\n"
            "### Memory Layout (v1)\n\n"
            "```\n"
            "Offset  Size       Field\n"
            "------  ---------  ----------------------------\n"
            "0       4 bytes    Magic (0x4D584D41 = 'MXMA')\n"
            "4       4 bytes    Version (1)\n"
            "8       4 bytes    Frame counter\n"
            "16-75   60 bytes   Global scalars (sample rate,\n"
            "                   LUFS, BPM, correlation, etc.)\n"
            "76-235  160 bytes  Per-channel data x 8\n"
            "236+    N*4 bytes  Spectrum (fft_size/2+1 floats)\n"
            "        M*4 bytes  Waveform (1024 floats)\n"
            "Total:  ~12 KB\n"
            "```\n\n"
            "### Direct Access from Python\n\n"
            "```\n"
            "from CustomComponents.shared_memory import AudioSharedMemoryReader\n"
            "\n"
            "reader = AudioSharedMemoryReader()\n"
            "if reader.open():\n"
            "    data = reader.read()\n"
            "    print(data['lufs_momentary'])\n"
            "    reader.close()\n"
            "```\n"
            "\n---\n\n"

            // ------ 8. AudioData ------
            "## 8. AudioData (Audio Analysis)\n\n"
            "AudioData is a read-only snapshot of the current audio state,\n"
            "passed to on_render() every frame.\n\n"
            "### Stream Info\n\n"
            "```\n"
            "audio.sample_rate          Current sample rate (e.g. 44100)\n"
            "audio.num_channels         Number of active channels\n"
            "audio.is_playing           True if audio is playing\n"
            "audio.position_seconds     Playback position in seconds\n"
            "audio.duration_seconds     Total duration in seconds\n"
            "```\n\n"
            "### Levels (Per Channel)\n\n"
            "Access via audio.left, audio.right, or audio.channels[i]:\n\n"
            "```\n"
            ".rms             RMS level in dB\n"
            ".peak            Peak level in dB\n"
            ".true_peak       True peak in dB\n"
            ".rms_linear      RMS level normalised 0..1\n"
            ".peak_linear     Peak level normalised 0..1\n"
            "```\n\n"
            "### Spectrum (FFT)\n\n"
            "```\n"
            "audio.spectrum           List of magnitudes in dB\n"
            "audio.spectrum_linear    List of magnitudes normalised 0..1\n"
            "audio.fft_size           FFT bin count\n"
            "audio.freq_to_bin(hz)    Convert frequency to bin index\n"
            "audio.magnitude_at(hz)   Get magnitude at a frequency\n"
            "audio.band_magnitude(lo_hz, hi_hz)  Average over a band\n"
            "```\n\n"
            "### Waveform\n\n"
            "```\n"
            "audio.waveform    Mono-mixed sample block (~1024 samples)\n"
            "```\n\n"
            "### Stereo Analysis\n\n"
            "```\n"
            "audio.correlation    Stereo correlation (-1 to +1)\n"
            "audio.stereo_angle   Stereo balance angle (-90 to +90)\n"
            "```\n\n"
            "### Loudness (EBU R128)\n\n"
            "```\n"
            "audio.lufs_momentary      Momentary loudness (LUFS)\n"
            "audio.lufs_short_term     Short-term loudness (LUFS)\n"
            "audio.lufs_integrated     Integrated loudness (LUFS)\n"
            "audio.loudness_range      Loudness range (LU)\n"
            "```\n\n"
            "### Beat Detection\n\n"
            "```\n"
            "audio.bpm            Detected tempo in BPM\n"
            "audio.beat_phase     Phase within current beat (0..1)\n"
            "```\n"
            "\n---\n\n"

            // ------ 9. Properties ------
            "## 9. Properties System\n\n"
            "Override get_properties() to expose settings in the Property Panel.\n"
            "The UI is auto-generated from the property definitions.\n\n"
            "### Available Types\n\n"
            "INT, FLOAT, BOOL, STRING, COLOR, ENUM, RANGE, FILE_PATH, POINT, SIZE\n\n"
            "### Example\n\n"
            "```\n"
            "def get_properties(self):\n"
            "    return [\n"
            "        Property(\n"
            "            \"color\", \"Bar Colour\", PropertyType.COLOR,\n"
            "            default=Color(0x3A, 0x7B, 0xFF),\n"
            "            group=\"Appearance\"\n"
            "        ),\n"
            "        Property(\n"
            "            \"num_bars\", \"Number of Bars\", PropertyType.INT,\n"
            "            default=32, min_value=4, max_value=128,\n"
            "            group=\"Display\"\n"
            "        ),\n"
            "        Property(\n"
            "            \"style\", \"Bar Style\", PropertyType.ENUM,\n"
            "            default=\"filled\",\n"
            "            choices=[(\"filled\",\"Filled\"),(\"outline\",\"Outline\")],\n"
            "            group=\"Display\"\n"
            "        ),\n"
            "    ]\n"
            "```\n\n"
            "### Reading / Writing\n\n"
            "```\n"
            "value = self.get_property(\"color\", Color.white())   # read\n"
            "self.set_property(\"color\", new_color)                # write\n"
            "```\n\n"
            "### Reacting to Changes\n\n"
            "```\n"
            "def on_property_changed(self, key, value):\n"
            "    if key == \"num_bars\":\n"
            "        self._rebuild_bars(value)\n"
            "```\n"
            "\n---\n\n"

            // ------ 10. Manifest ------
            "## 10. Manifest\n\n"
            "Every component must provide a Manifest via get_manifest().\n\n"
            "### Fields\n\n"
            "```\n"
            "id             (required) Reverse-domain ID, e.g. \"com.author.name\"\n"
            "name           (required) Display name shown in TOOLBOX\n"
            "version        Semantic version string (default \"1.0.0\")\n"
            "author         Author name\n"
            "description    Tooltip / description text\n"
            "category       METER | ANALYZER | SCOPE | STEREO |\n"
            "               DECORATION | UTILITY | OTHER\n"
            "default_size   (width, height) tuple\n"
            "min_size       Minimum resize dimensions\n"
            "max_size       Maximum resize dimensions\n"
            "tags           Tuple of searchable keyword strings\n"
            "```\n"
            "\n---\n\n"

            // ------ 11. Color & Gradient ------
            "## 11. Color & Gradient\n\n"
            "### Color\n\n"
            "```\n"
            "Color(r, g, b, a=255)\n"
            "Color.from_hex(0xFF3A7BFF)\n"
            "Color.from_hex_str(\"#3A7BFF\")\n"
            "\n"
            "color.with_alpha(0.5)\n"
            "color.lerp(other_color, t)     # blend between two colours\n"
            "```\n\n"
            "### Predefined Colors\n\n"
            "white, black, transparent, red, green, blue, yellow, cyan, magenta\n\n"
            "### Gradient\n\n"
            "```\n"
            "Gradient.linear(x1, y1, x2, y2, [\n"
            "    GradientStop(0.0, Color(255, 0, 0)),\n"
            "    GradientStop(1.0, Color(0, 0, 255)),\n"
            "])\n"
            "\n"
            "Gradient.radial(cx, cy, radius, [\n"
            "    GradientStop(0.0, Color.white()),\n"
            "    GradientStop(1.0, Color.transparent()),\n"
            "])\n"
            "```\n"
            "\n---\n\n"

            // ------ 12. Examples ------
            "## 12. Examples\n\n"

            "### 12-A. Simple VU Meter\n\n"
            "```\n"
            "class SimpleVUMeter(BaseComponent):\n"
            "    @staticmethod\n"
            "    def get_manifest():\n"
            "        return Manifest(\n"
            "            id=\"com.example.simple_vu\",\n"
            "            name=\"Simple VU Meter\",\n"
            "            category=Category.METER,\n"
            "            default_size=(200, 300),\n"
            "        )\n"
            "\n"
            "    def get_properties(self):\n"
            "        return [\n"
            "            Property(\"color\", \"Bar Colour\", PropertyType.COLOR,\n"
            "                     default=Color(0x3A, 0x7B, 0xFF)),\n"
            "            Property(\"smoothing\", \"Smoothing\", PropertyType.RANGE,\n"
            "                     default=0.15, min_value=0.01, max_value=0.5),\n"
            "        ]\n"
            "\n"
            "    def on_init(self):\n"
            "        self.state.set(\"smooth_l\", 0.0)\n"
            "        self.state.set(\"smooth_r\", 0.0)\n"
            "\n"
            "    def on_render(self, ctx, audio):\n"
            "        ctx.clear(Color(0x12, 0x12, 0x1A))\n"
            "        col = self.get_property(\"color\", Color(0x3A, 0x7B, 0xFF))\n"
            "        sm  = self.get_property(\"smoothing\", 0.15)\n"
            "        sl  = self.state.get(\"smooth_l\", 0.0)\n"
            "        sr  = self.state.get(\"smooth_r\", 0.0)\n"
            "        sl += (audio.left.rms_linear  - sl) * sm\n"
            "        sr += (audio.right.rms_linear - sr) * sm\n"
            "        self.state.set(\"smooth_l\", sl)\n"
            "        self.state.set(\"smooth_r\", sr)\n"
            "        w, h = ctx.width, ctx.height\n"
            "        bw = w * 0.35\n"
            "        for i, lv in enumerate([sl, sr]):\n"
            "            x = w * 0.15 + i * (bw + w * 0.1)\n"
            "            fh = h * 0.9 * lv\n"
            "            ctx.fill_rounded_rect(x, h*0.95 - fh, bw, fh, 3, col)\n"
            "```\n\n"

            "### 12-B. Circular Spectrum Analyser\n\n"
            "```\n"
            "import math\n"
            "\n"
            "class CircularSpectrum(BaseComponent):\n"
            "    @staticmethod\n"
            "    def get_manifest():\n"
            "        return Manifest(\n"
            "            id=\"com.example.circular_spectrum\",\n"
            "            name=\"Circular Spectrum\",\n"
            "            category=Category.ANALYZER,\n"
            "            default_size=(300, 300),\n"
            "        )\n"
            "\n"
            "    def on_init(self):\n"
            "        self._smooth = [0.0] * 64\n"
            "\n"
            "    def on_render(self, ctx, audio):\n"
            "        ctx.clear(Color(0x0A, 0x0A, 0x14))\n"
            "        cx, cy = ctx.width / 2, ctx.height / 2\n"
            "        radius = min(cx, cy) - 10\n"
            "        inner  = radius * 0.3\n"
            "        spec   = audio.spectrum_linear or []\n"
            "        for i in range(64):\n"
            "            idx = int(len(spec) * i / 64) if spec else 0\n"
            "            val = spec[idx] if idx < len(spec) else 0\n"
            "            self._smooth[i] += (val - self._smooth[i]) * 0.2\n"
            "            angle = (i / 64) * 2 * math.pi - math.pi / 2\n"
            "            length = (radius - inner) * self._smooth[i]\n"
            "            x1 = cx + inner * math.cos(angle)\n"
            "            y1 = cy + inner * math.sin(angle)\n"
            "            x2 = cx + (inner + length) * math.cos(angle)\n"
            "            y2 = cy + (inner + length) * math.sin(angle)\n"
            "            t = i / 63.0\n"
            "            col = Color(0, 200, 255).lerp(Color(255, 68, 136), t)\n"
            "            ctx.draw_line(x1, y1, x2, y2, col, 2.5)\n"
            "```\n\n"

            "### 12-C. Beat-Reactive Background\n\n"
            "```\n"
            "class BeatPulse(BaseComponent):\n"
            "    @staticmethod\n"
            "    def get_manifest():\n"
            "        return Manifest(\n"
            "            id=\"com.example.beat_pulse\",\n"
            "            name=\"Beat Pulse BG\",\n"
            "            category=Category.DECORATION,\n"
            "            default_size=(400, 400),\n"
            "        )\n"
            "\n"
            "    def on_render(self, ctx, audio):\n"
            "        phase = audio.beat_phase\n"
            "        pulse = (1.0 - phase) * 0.5\n"
            "        level = max(audio.left.rms_linear, audio.right.rms_linear)\n"
            "        pulse = max(pulse, level * 0.5)\n"
            "        bg  = Color(0x0A, 0x0A, 0x14)\n"
            "        col = Color(0xFF, 0x00, 0x88)\n"
            "        ctx.clear(bg.lerp(col, pulse * 0.4))\n"
            "        cx, cy = ctx.width / 2, ctx.height / 2\n"
            "        r = min(cx, cy) * (0.3 + pulse * 0.5)\n"
            "        ctx.fill_ellipse(cx, cy, r, r, col.with_alpha(pulse * 0.6))\n"
            "```\n\n"

            "### 12-D. GPU Particle Visualizer\n\n"
            "```\n"
            "class ParticleDemo(BaseComponent):\n"
            "    @staticmethod\n"
            "    def get_manifest():\n"
            "        return Manifest(\n"
            "            id='com.example.particle_demo',\n"
            "            name='Particle Demo',\n"
            "            category=Category.VISUALIZER,\n"
            "            default_size=(400, 300),\n"
            "        )\n"
            "\n"
            "    def get_properties(self):\n"
            "        return [\n"
            "            Property('count', 'Particle Count', PropertyType.INT,\n"
            "                     default=5000, min_val=100, max_val=100000),\n"
            "            Property('speed', 'Speed', PropertyType.FLOAT,\n"
            "                     default=1.0, min_val=0.1, max_val=5.0),\n"
            "        ]\n"
            "\n"
            "    def on_render(self, ctx, audio):\n"
            "        ctx.clear(Color(5, 5, 16))\n"
            "        # Compute shader on GL 4.3+, fragment fallback on GL 3.3\n"
            "        ctx.particles(\n"
            "            count=self.get_property('count', 5000),\n"
            "            speed=self.get_property('speed', 1.0),\n"
            "            size=2.5,\n"
            "            color=(0.3, 0.8, 1.0),\n"
            "        )\n"
            "        # Add bloom glow on top\n"
            "        ctx.bloom(intensity=1.5, threshold=0.4)\n"
            "```\n\n"

            "### 12-E. Shader Post-Processing Chain\n\n"
            "```\n"
            "def on_render(self, ctx, audio):\n"
            "    ctx.clear(Color.black())\n"
            "    # ... draw your base content ...\n"
            "    \n"
            "    # Stack post-processing effects:\n"
            "    ctx.bloom(intensity=2.0, threshold=0.5)\n"
            "    ctx.chromatic_aberration(intensity=0.008)\n"
            "    ctx.glitch(intensity=audio.left.rms_linear * 0.3)\n"
            "```\n"
            "\n---\n\n"

            // ------ 13. Import & Deployment ------
            "## 13. Import & Deployment\n\n"
            "### Method 1 - Manual Copy\n\n"
            "Copy your .py file into CustomComponents/plugins/ and restart MaxiMeter.\n\n"
            "### Method 2 - Menu Import\n\n"
            "- File > Import Component... > select your .py file\n"
            "- The file is auto-copied to the plugins directory\n"
            "- Restart to activate the new component\n\n"
            "### Distribution\n\n"
            "Share the single .py file. No packaging or compilation needed.\n"
            "\n---\n\n"

            // ------ 14. Hot Reload ------
            "## 14. Hot Reload\n\n"
            "- Edit and save your .py file\n"
            "- The framework detects changes and reloads the module automatically\n"
            "- Existing canvas instances are NOT auto-migrated (remove and re-add)\n"
            "\n---\n\n"

            // ------ 15. Error Recovery & Auto-Restart ------
            "## 15. Error Recovery & Auto-Restart\n\n"
            "The C++ host monitors the Python bridge process and recovers\n"
            "automatically from crashes.\n\n"
            "### How It Works\n\n"
            "- If the Python process exits unexpectedly (crash, exception),\n"
            "  the host detects the broken pipe and triggers a restart.\n"
            "- Up to 5 consecutive restarts are attempted.\n"
            "- 200 ms cool-down between each attempt.\n"
            "- The retry counter resets after a successful IPC round-trip,\n"
            "  so occasional crashes won't exhaust the budget.\n"
            "- Intentional stop() calls block the restart mechanism.\n\n"
            "### What Plugin Authors Should Know\n\n"
            "- No action required: recovery is fully automatic.\n"
            "- Fix persistent crashes in your on_render() code to avoid\n"
            "  hitting the 5-retry limit.\n"
            "- Guard against None/empty arrays: audio.spectrum_linear\n"
            "  may be empty on the first frame after a restart.\n"
            "\n---\n\n"

            // ------ 16. Troubleshooting ------
            "## 16. Troubleshooting\n\n"
            "### Component Does Not Appear in TOOLBOX\n\n"
            "- Verify the file is in CustomComponents/plugins/\n"
            "- Check that get_manifest() returns a valid Manifest object\n"
            "- Look at the log output for Python import errors\n\n"
            "### Runtime Error in on_render()\n\n"
            "- Check stderr / log for Python tracebacks\n"
            "- Always handle empty spectrum / waveform arrays gracefully\n"
            "- Avoid division by zero when audio.rms_linear is 0\n\n"
            "### Particles Capped at 500\n\n"
            "- This means your GPU only supports OpenGL 3.3 (fragment fallback).\n"
            "- This is expected behaviour, not a bug.\n"
            "- To use 100k particles you need a GPU with OpenGL 4.3+.\n\n"
            "### No Audio Data (All Zeros)\n\n"
            "- Check that SharedMemory region (MaxiMeter_AudioSHM) is created.\n"
            "- Verify pipe connection between host and Python is active.\n"
            "- Check that audio is actually playing in the host.\n\n"
            "### Plugin Keeps Restarting\n\n"
            "- A crash loop: the host tries up to 5 restarts then stops.\n"
            "- Fix the exception in your on_render() code.\n"
            "- Check the log for Python tracebacks.\n\n"
            "### Performance Issues\n\n"
            "- Keep draw calls under ~500 per frame for smooth 60 fps\n"
            "- Use smoothing variables instead of raw values\n"
            "- Avoid creating new objects inside on_render() each frame\n"
            "- For particles, use the compute shader path (GL 4.3+)\n"
            "\n---\n\n"

            // ------ 17. FAQ ------
            "## 17. FAQ\n\n"
            "### Can I use NumPy or other third-party packages?\n\n"
            "Yes, if installed in the Python environment. However, distribution\n"
            "becomes harder since users will need those packages too.\n\n"
            "### What Python version is required?\n\n"
            "Python 3.10 or later.\n\n"
            "### Can I access the filesystem?\n\n"
            "Yes, but avoid blocking I/O inside on_render(). Load files in on_init().\n\n"
            "### How many draw commands can I make per frame?\n\n"
            "Under 500 is perfectly fine at 60 fps. 1000+ may need optimisation.\n"
            "Consider reducing the number of FFT bins or simplifying geometry.\n\n"
            "### How many particles can I use?\n\n"
            "On OpenGL 4.3+ GPUs: up to 100,000 via compute shader.\n"
            "On OpenGL 3.3 GPUs:  up to 500 via fragment shader fallback.\n"
            "The host selects the best path automatically.\n\n"
            "### How does SharedMemory improve performance?\n\n"
            "SharedMemory transfers audio data in ~5 us per frame via binary\n"
            "mmap, compared to ~500 us with JSON serialisation over pipes.\n"
            "This is ~100x faster and is used automatically when available.\n\n"
            "### What happens if my plugin crashes?\n\n"
            "The host auto-restarts the Python process (up to 5 retries).\n"
            "Fix the crash, and the counter resets on the next successful frame.\n\n"
            "### Can components be interactive?\n\n"
            "Yes. Override on_mouse_down, on_mouse_move, and on_mouse_up.\n"
            "Double-click a component on the canvas to enable interaction mode.\n\n"
            "### Is component state saved with the project?\n\n"
            "Yes. All properties and self.state data are automatically serialised\n"
            "and restored when the project file is saved and reopened.\n\n"

            "---\n\n"
            "### Keyboard Shortcuts\n\n"
            "```\n"
            "Ctrl+F        Focus the search bar\n"
            "F3            Jump to next match\n"
            "Shift+F3      Jump to previous match\n"
            "Escape        Clear search\n"
            "```\n"
            "\n---\n\n"
            "End of Documentation\n"
            );
        }
    };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DocumentationWindow)
};
