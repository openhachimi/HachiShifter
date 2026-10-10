#include "Theme.h"

namespace hachi
{
juce::Colour Palette::background  { 0xff353535 };
juce::Colour Palette::base        { 0xff2d2d2d };
juce::Colour Palette::panel       { 0xff2a2a2a };
juce::Colour Palette::panelRaised { 0xff404040 };
juce::Colour Palette::button      { 0xff3d3d3d };
juce::Colour Palette::buttonHover { 0xff484848 };
juce::Colour Palette::border      { 0xff505050 };
juce::Colour Palette::graphBackground { 0xff232323 };
juce::Colour Palette::clipBackground  { 0xff343434 };
juce::Colour Palette::grid        { 0xff373737 };
juce::Colour Palette::beatGrid    { 0xff4a4a4a };
juce::Colour Palette::accent      { 0xff7f69ca };
juce::Colour Palette::accentLight { 0xffcbcbfa };
// Shared UTAU/Melodyne editor palette.  Yellow is reserved for warnings and
// exceptional handles; ordinary notes use the blue-green treatment from the
// migrated UTAU editor.
juce::Colour Palette::noteFill    { 0xff269b9b };
juce::Colour Palette::noteLight   { 0xff8be0d5 };
juce::Colour Palette::noteEdge    { 0xff4fc3b5 };
juce::Colour Palette::pitchLine   { 0xfff4f4f4 };
juce::Colour Palette::playhead    { 0xfff05a5a };
juce::Colour Palette::text        { 0xffd0d0d0 };
juce::Colour Palette::textMuted   { 0xff909090 };
juce::Colour Palette::scrollThumb { 0xff555555 };

juce::Colour Palette::trackColour(std::size_t index)
{
    static const juce::Colour colours[] {
        juce::Colour(0xff7f69ca), juce::Colour(0xffcbcbfa), juce::Colour(0xfff4c000),
        juce::Colour(0xff9b8bdd), juce::Colour(0xffdedcff), juce::Colour(0xffffd94f)
    };
    return colours[index % 6];
}

void Palette::applyTheme(const juce::String& theme, juce::Colour accentColour,
                         juce::Colour accentLightColour, juce::Colour noteColour)
{
    const auto light = theme == "light";
    background = light ? juce::Colour(0xffeeeeee) : juce::Colour(0xff353535);
    base = light ? juce::Colour(0xfffafafa) : juce::Colour(0xff2d2d2d);
    panel = light ? juce::Colour(0xfff3f3f3) : juce::Colour(0xff2a2a2a);
    panelRaised = light ? juce::Colour(0xffe2e2e2) : juce::Colour(0xff404040);
    button = light ? juce::Colour(0xffdedede) : juce::Colour(0xff3d3d3d);
    buttonHover = light ? juce::Colour(0xffd2d2d2) : juce::Colour(0xff484848);
    border = light ? juce::Colour(0xffbcbcbc) : juce::Colour(0xff505050);
    graphBackground = light ? juce::Colour(0xfff8f8f8) : juce::Colour(0xff232323);
    clipBackground = light ? juce::Colour(0xffe9e9e9) : juce::Colour(0xff343434);
    grid = light ? juce::Colour(0xffdddddd) : juce::Colour(0xff373737);
    beatGrid = light ? juce::Colour(0xffc9c9c9) : juce::Colour(0xff4a4a4a);
    text = light ? juce::Colour(0xff282828) : juce::Colour(0xffd0d0d0);
    textMuted = light ? juce::Colour(0xff6f6f6f) : juce::Colour(0xff909090);
    scrollThumb = light ? juce::Colour(0xffa9a9a9) : juce::Colour(0xff555555);
    pitchLine = light ? juce::Colour(0xff242424) : juce::Colour(0xfff4f4f4);
    accent = accentColour;
    accentLight = accentLightColour;
    // Keep note colours tied to the shared editor style.  Older preference
    // files may contain the retired yellow note colour, so do not reapply it.
    juce::ignoreUnused(noteColour);
    noteFill = light ? juce::Colour(0xff43aaa0) : juce::Colour(0xff269b9b);
    noteLight = light ? juce::Colour(0xff16736e) : juce::Colour(0xff8be0d5);
    noteEdge = light ? juce::Colour(0xff2c817b) : juce::Colour(0xff4fc3b5);
}

HachiLookAndFeel::HachiLookAndFeel()
{
    refreshColours();
}

void HachiLookAndFeel::refreshColours()
{
    setColour(juce::ResizableWindow::backgroundColourId, Palette::background);
    setColour(juce::Label::textColourId, Palette::text);
    setColour(juce::TextButton::textColourOffId, Palette::text);
    // Text drawn on a toggled-on (accent) button must contrast with the accent,
    // not follow the panel colour: in light mode panel is near-white and washed
    // out against a light accent.  contrasting() picks black or white per accent.
    setColour(juce::TextButton::textColourOnId, Palette::accent.contrasting(0.85f));
    setColour(juce::ComboBox::backgroundColourId, Palette::base);
    setColour(juce::ComboBox::textColourId, Palette::text);
    setColour(juce::ComboBox::outlineColourId, Palette::border);
    setColour(juce::ComboBox::arrowColourId, Palette::text);
    // Text editors were left on the JUCE default (white field, black text),
    // which reads as an out-of-theme box in either mode.  Tie them to the
    // Palette so they follow dark/light like everything else.
    setColour(juce::TextEditor::backgroundColourId, Palette::base);
    setColour(juce::TextEditor::textColourId, Palette::text);
    setColour(juce::TextEditor::highlightedTextColourId, Palette::text);
    setColour(juce::TextEditor::highlightColourId, Palette::accent.withAlpha(0.35f));
    setColour(juce::TextEditor::outlineColourId, Palette::border);
    setColour(juce::TextEditor::focusedOutlineColourId, Palette::accent);
    setColour(juce::CaretComponent::caretColourId, Palette::text);
    setColour(juce::PopupMenu::backgroundColourId, Palette::panelRaised);
    setColour(juce::PopupMenu::textColourId, Palette::text);
    setColour(juce::PopupMenu::highlightedBackgroundColourId, Palette::accent);
    // Highlighted menu text must contrast with the accent highlight, not the
    // panel (near-white in light mode → invisible on a light accent).
    setColour(juce::PopupMenu::highlightedTextColourId, Palette::accent.contrasting(0.85f));
    setColour(juce::ScrollBar::backgroundColourId, Palette::base);
    setColour(juce::ScrollBar::trackColourId, Palette::base);
    setColour(juce::ScrollBar::thumbColourId, Palette::scrollThumb);
}

void HachiLookAndFeel::drawButtonBackground(juce::Graphics& g, juce::Button& button,
                                            const juce::Colour&, bool highlighted, bool down)
{
    auto colour = button.getToggleState() ? Palette::accent : Palette::button;
    if (highlighted) colour = button.getToggleState() ? Palette::accent.brighter(0.12f)
                                                       : Palette::buttonHover;
    if (down) colour = colour.darker(0.15f);
    g.setColour(colour);
    g.fillRoundedRectangle(button.getLocalBounds().toFloat().reduced(1.0f), 4.0f);
}

void HachiLookAndFeel::drawButtonText(juce::Graphics& g, juce::TextButton& button, bool, bool)
{
    const auto id = button.getComponentID();
    if (button.getButtonText() == "+" || button.getButtonText() == "-")
    {
        // Draw zoom symbols geometrically so narrow buttons never ellipsize.
        const auto bounds = button.getLocalBounds().toFloat();
        const auto centre = bounds.getCentre();
        const auto radius = juce::jmin(6.0f, juce::jmin(bounds.getWidth(), bounds.getHeight()) * 0.25f);
        g.setColour(Palette::text.withMultipliedAlpha(button.isEnabled() ? 1.0f : 0.4f));
        g.drawLine(centre.x - radius, centre.y, centre.x + radius, centre.y, 1.8f);
        if (button.getButtonText() == "+")
            g.drawLine(centre.x, centre.y - radius, centre.x, centre.y + radius, 1.8f);
        return;
    }
    if (!id.startsWith("icon."))
    {
        LookAndFeel_V4::drawButtonText(g, button, false, false);
        return;
    }

    auto bounds = button.getLocalBounds().toFloat().reduced(6.0f);
    // Original SVG artwork on a shared 24px grid. Cache parsing; tint a copy
    // so active/disabled buttons cannot change another button's cached image.
    static const auto toolIcons = []
    {
        std::array<std::unique_ptr<juce::Drawable>, 7> icons;
        const std::array<const char*, 7> shapes {{
            "<path d='M5 3 L5 19 L9 15 L13 22 L16 20 L12 13 L19 13 Z'/>",
            "<path d='M4 20 L5 15 L16 4 Q18 2 20 4 Q22 6 20 8 L9 19 Z M5 15 L9 19 M14 6 L18 10 M4 20 L8 19'/>",
            "<path d='M5 19 L19 5'/><rect x='2' y='16' width='5' height='5'/><rect x='17' y='2' width='5' height='5'/>",
            "<path d='M4 18 C9 18 11 6 20 6'/><circle cx='4' cy='18' r='2'/><circle cx='12' cy='12' r='2'/><circle cx='20' cy='6' r='2'/>",
            "<path d='M14 3 A6 6 0 0 0 11 12 L3 20 L6 23 L14 15 A6 6 0 0 0 21 7 L17 11 L13 7 L17 3 Z'/>",
            "<path d='M9 15 L15 9 M8 12 L5 15 A3 3 0 0 0 9 19 L12 16 M12 8 L15 5 A3 3 0 0 1 19 9 L16 12'/>",
            "<path d='M7 3 V17 H21 M3 7 H17 V21 M10 10 H14 V14'/>"
        }};
        for (std::size_t index = 0; index < icons.size(); ++index)
        {
            const auto svg = juce::String("<svg xmlns='http://www.w3.org/2000/svg' width='24' height='24' viewBox='0 0 24 24'><g fill='none' stroke='#000000' stroke-width='1.8' stroke-linecap='round' stroke-linejoin='round'>")
                + shapes[index] + "</g></svg>";
            if (const auto xml = juce::parseXML(svg))
                icons[index] = juce::Drawable::createFromSVG(*xml);
        }
        return icons;
    }();
    const std::array<const char*, 7> toolIds {{ "icon.pointer", "icon.draw",
        "icon.line", "icon.points", "icon.wrench", "icon.connect", "icon.trim" }};
    for (std::size_t index = 0; index < toolIds.size(); ++index)
        if (id == toolIds[index] && toolIcons[index])
        {
            auto icon = toolIcons[index]->createCopy();
            icon->replaceColour(juce::Colours::black,
                (button.getToggleState() ? Palette::accent.contrasting(0.85f) : Palette::text).withMultipliedAlpha(button.isEnabled() ? 1.f : .35f));
            icon->drawWithin(g, button.getLocalBounds().toFloat().reduced(4.0f),
                juce::RectanglePlacement::centred, button.isEnabled() ? 1.0f : 0.4f);
            return;
        }
    g.setColour(button.getToggleState() ? Palette::accent.contrasting(0.85f) : Palette::text);
    juce::Path path;
    if (id == "icon.play")
        path.addTriangle(bounds.getX() + 2.0f, bounds.getY(), bounds.getRight(), bounds.getCentreY(),
                         bounds.getX() + 2.0f, bounds.getBottom());
    else if (id == "icon.pause")
    {
        const auto barWidth = juce::jmax(2.0f, bounds.getWidth() * 0.28f);
        path.addRoundedRectangle(bounds.getX() + 1.0f, bounds.getY(), barWidth, bounds.getHeight(), 1.0f);
        path.addRoundedRectangle(bounds.getRight() - barWidth - 1.0f, bounds.getY(),
                                 barWidth, bounds.getHeight(), 1.0f);
    }
    else if (id == "icon.stop")
        path.addRectangle(bounds.reduced(2.0f));
    else if (id == "icon.open")
    {
        path.addRoundedRectangle(bounds.withTrimmedTop(4.0f), 2.0f);
        path.addRectangle(bounds.getX() + 2.0f, bounds.getY() + 1.0f, bounds.getWidth() * 0.42f, 5.0f);
    }
    else if (id == "icon.save")
    {
        path.addRoundedRectangle(bounds, 1.5f);
        path.addRectangle(bounds.reduced(3.0f).withHeight(5.0f));
        path.addRectangle(bounds.reduced(4.0f).withTrimmedTop(9.0f));
    }
    else if (id == "icon.pointer")
    {
        // Cursor arrow with a straight top edge and angular tip.
        path.startNewSubPath(bounds.getX() + 1.5f, bounds.getY() + 1.5f);
        path.lineTo(bounds.getX() + 1.5f, bounds.getBottom() - 4.5f);
        path.lineTo(bounds.getX() + 6.0f, bounds.getBottom() - 9.0f);
        path.lineTo(bounds.getX() + 9.0f, bounds.getBottom() - 1.5f);
        path.lineTo(bounds.getX() + 11.5f, bounds.getBottom() - 4.0f);
        path.lineTo(bounds.getX() + 8.5f, bounds.getBottom() - 11.0f);
        path.lineTo(bounds.getRight() - 2.5f, bounds.getBottom() - 9.5f);
        path.closeSubPath();
    }
    else if (id == "icon.draw")
    {
        // Pencil: diagonal body, pointed tip, visible lead.
        path.startNewSubPath(bounds.getX() + 1.5f, bounds.getBottom() - 3.0f);
        path.lineTo(bounds.getX() + 2.5f, bounds.getBottom() - 6.0f);
        path.lineTo(bounds.getRight() - 2.5f, bounds.getY() + 2.0f);
        path.lineTo(bounds.getRight() - 4.5f, bounds.getY() + 0.5f);
        path.lineTo(bounds.getX() + 5.5f, bounds.getBottom() - 7.5f);
        path.lineTo(bounds.getX() + 3.0f, bounds.getBottom() - 5.0f);
        path.lineTo(bounds.getX() + 1.5f, bounds.getBottom() - 3.0f);
        path.closeSubPath();
    }
    else if (id == "icon.line")
    {
        // Diagonal line with small start/end caps like a straight-line tool.
        path.startNewSubPath(bounds.getX() + 1.0f, bounds.getBottom() - 1.0f);
        path.lineTo(bounds.getRight() - 1.0f, bounds.getY() + 1.0f);
    }
    else if (id == "icon.points")
    {
        path.startNewSubPath(bounds.getX() + 1.0f, bounds.getBottom() - 2.0f);
        path.lineTo(bounds.getCentreX(), bounds.getCentreY() + 1.0f);
        path.lineTo(bounds.getRight() - 1.0f, bounds.getY() + 2.0f);
        g.strokePath(path, juce::PathStrokeType(1.6f, juce::PathStrokeType::curved));
        g.fillEllipse(bounds.getX() - 1.0f, bounds.getBottom() - 4.0f, 4.0f, 4.0f);
        g.fillEllipse(bounds.getCentreX() - 2.0f, bounds.getCentreY() - 1.0f, 4.0f, 4.0f);
        g.fillEllipse(bounds.getRight() - 3.0f, bounds.getY(), 4.0f, 4.0f);
        return;
    }
    else if (id == "icon.wrench")
    {
        // Open-end wrench: C-shaped jaw and shaft with an angled end.
        path.startNewSubPath(bounds.getX() + 2.0f, bounds.getCentreY() - 3.5f);
        path.lineTo(bounds.getX() + 2.0f, bounds.getY() + 2.0f);
        path.lineTo(bounds.getX() + 6.5f, bounds.getY() + 2.0f);
        path.lineTo(bounds.getX() + 6.5f, bounds.getCentreY() - 4.0f);
        path.closeSubPath();
        path.startNewSubPath(bounds.getX() + 2.0f, bounds.getCentreY() + 3.5f);
        path.lineTo(bounds.getX() + 2.0f, bounds.getBottom() - 2.0f);
        path.lineTo(bounds.getX() + 6.5f, bounds.getBottom() - 2.0f);
        path.lineTo(bounds.getX() + 6.5f, bounds.getCentreY() + 4.0f);
        path.closeSubPath();
        path.startNewSubPath(bounds.getX() + 6.0f, bounds.getCentreY() + 4.0f);
        path.lineTo(bounds.getRight() - 1.0f, bounds.getCentreY() - 2.0f);
    }
    else if (id == "icon.collapse" || id == "icon.expand")
    {
        // A chevron pointing the way the arrangement will go.
        const auto rise = id == "icon.collapse" ? -3.0f : 3.0f;
        path.startNewSubPath(bounds.getX() + 1.0f, bounds.getCentreY() - rise);
        path.lineTo(bounds.getCentreX(), bounds.getCentreY() + rise);
        path.lineTo(bounds.getRight() - 1.0f, bounds.getCentreY() - rise);
    }
    else if (id == "icon.connect")
    {
        path.addEllipse(bounds.getX(), bounds.getCentreY() - 3.0f, 6.0f, 6.0f);
        path.addEllipse(bounds.getRight() - 6.0f, bounds.getCentreY() - 3.0f, 6.0f, 6.0f);
        path.startNewSubPath(bounds.getX() + 5.0f, bounds.getCentreY());
        path.lineTo(bounds.getRight() - 5.0f, bounds.getCentreY());
    }
    else
    {
        g.setFont(10.0f);
        g.drawText(id == "icon.audio" ? "A+" : "M+", button.getLocalBounds(), juce::Justification::centred);
        return;
    }
    if (id == "icon.line" || id == "icon.wrench" || id == "icon.connect"
        || id == "icon.collapse" || id == "icon.expand")
        g.strokePath(path, juce::PathStrokeType(2.0f, juce::PathStrokeType::curved,
                                                juce::PathStrokeType::rounded));
    else
        g.fillPath(path);
}

void HachiLookAndFeel::drawComboBox(juce::Graphics& g, int width, int height, bool,
                                    int, int, int, int, juce::ComboBox&)
{
    auto bounds = juce::Rectangle<float>(0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height));
    g.setColour(Palette::base);
    g.fillRoundedRectangle(bounds.reduced(1.0f), 4.0f);
    g.setColour(Palette::border);
    g.drawRoundedRectangle(bounds.reduced(1.0f), 4.0f, 1.0f);
    drawDropdownArrow(g, { 0, 0, width, height });
}

void HachiLookAndFeel::drawDropdownArrow(juce::Graphics& g, juce::Rectangle<int> bounds)
{
    g.setColour(Palette::accentLight);
    const auto x = static_cast<float>(bounds.getRight() - 14);
    const auto y = static_cast<float>(bounds.getCentreY());
    juce::Path arrow;
    arrow.startNewSubPath(x - 4.0f, y - 2.0f);
    arrow.lineTo(x, y + 2.0f);
    arrow.lineTo(x + 4.0f, y - 2.0f);
    g.strokePath(arrow, juce::PathStrokeType(1.5f));
}
}
