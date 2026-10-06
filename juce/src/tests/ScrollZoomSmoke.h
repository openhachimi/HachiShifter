#pragma once

namespace hachi
{
inline bool MainComponent::diagnosticScrollZoom(const juce::File& directory)
{
    directory.createDirectory();
    setSize(1280, 800);
    const auto track = project.addTrack("Scroll zoom", true);
    project.setTrackPitchAlgorithm(track, PitchAlgorithm::utau);
    const auto clip = project.addClip(track, 0.0, 16.0);
    for (int i = 0; i < 12; ++i)
        (void) project.addNote(clip, i * 0.8, 0.65, 60.0f + i % 4);
    for (int i = 0; i < 7; ++i)
        (void) project.addTrack("Track " + juce::String(i + 2), true);
    project.dispatchPendingMessages();
    pianoRoll.diagnosticRefresh();
    diagnosticSelectTrack(track);
    resized();
    const auto revision = project.revisionNumber();
    bool ok = true;
    const auto check = [&](const char* label, bool passed)
    { ok = ok && passed; std::cout << label << '=' << passed << '\n'; };
    const auto reset = [&]
    {
        zoomSlider.setValue(140, juce::sendNotificationSync);
        vZoomSlider.setValue(1.0, juce::sendNotificationSync);
        pianoViewport.setViewPosition(200, 500);
    };
    const auto drag = [&](ZoomScrollBar& bar, juce::Point<float> down, float delta)
    {
        const auto event = [&](juce::Point<float> at)
        {
            return juce::MouseEvent(juce::Desktop::getInstance().getMainMouseSource(), at,
                juce::ModifierKeys(juce::ModifierKeys::leftButtonModifier),
                1.0f, 0.0f, 0.0f, 0.0f, 0.0f, &bar, &bar,
                juce::Time::getCurrentTime(), down, juce::Time::getCurrentTime(), 1, true);
        };
        const auto to = down + (bar.isVertical() ? juce::Point<float>(0, delta)
                                               : juce::Point<float>(delta, 0));
        bar.mouseDown(event(down)); bar.mouseDrag(event(to)); bar.mouseUp(event(to));
    };
    for (const bool vertical : { false, true })
    {
        auto* bar = dynamic_cast<ZoomScrollBar*>(vertical ? &pianoViewport.getVerticalScrollBar()
                                                        : &pianoViewport.getHorizontalScrollBar());
        check("custom_scrollbar_installed", bar != nullptr);
        if (bar == nullptr) return false;
        const auto anchor = [&](bool leading)
        {
            if (vertical)
                return (pianoViewport.getViewPositionY() + (leading ? pianoViewport.getViewHeight() : 0))
                    / juce::jlimit(12.0, 48.0, vZoomSlider.getValue() * 22.0);
            return pianoRoll.secondsForPixel(pianoViewport.getViewPositionX()
                + (leading ? pianoViewport.getViewWidth() : 0));
        };
        for (const bool leading : { true, false })
            for (const bool inward : { true, false })
            {
                reset();
                const auto before = anchor(leading);
                drag(*bar, bar->gripCentre(leading), (leading ? 8.0f : -8.0f) * (inward ? 1.0f : -1.0f));
                const auto zoom = vertical ? vZoomSlider.getValue() : zoomSlider.getValue();
                check("endpoint_changes_zoom_in_correct_direction", inward ? zoom > (vertical ? 1.0 : 140.0)
                                                                           : zoom < (vertical ? 1.0 : 140.0));
                check("opposite_endpoint_stays_anchored", std::abs(anchor(leading) - before) < (vertical ? 0.06 : 0.02));
            }
        reset();
        const auto oldStart = bar->getCurrentRangeStart();
        drag(*bar, bar->thumbBounds().getCentre(), 12.0f);
        check("middle_still_scrolls", bar->getCurrentRangeStart() > oldStart);
        check("middle_does_not_zoom", zoomSlider.getValue() == 140 && vZoomSlider.getValue() == 1.0);
        reset();
        auto& slider = vertical ? vZoomSlider : zoomSlider;
        slider.setValue(slider.getMaximum(), juce::sendNotificationSync);
        drag(*bar, bar->gripCentre(false), -10000.0f);
        check("maximum_zoom_is_clamped", slider.getValue() == slider.getMaximum());
    }
    reset();
    pianoViewport.setViewPosition(0, 500);
    auto& horizontal = dynamic_cast<ZoomScrollBar&>(pianoViewport.getHorizontalScrollBar());
    drag(horizontal, horizontal.gripCentre(false), -8.0f);
    check("origin_does_not_jump_past_keyboard_gutter", pianoViewport.getViewPositionX() == 0);
    zoomSlider.setValue(40, juce::sendNotificationSync);
    check("grips_remain_visible_at_full_range", horizontal.isVisible() && !horizontal.autoHides());
    const auto resetUpper = [&]
    {
        timelineHorizontalZoom = 140.0;
        timelineVerticalZoom = 1.0;
        timeline.setPixelsPerSecond(140.0f);
        timeline.setRowHeight(96.0f);
        trackList.setRowHeight(96.0f);
        timelineViewport.setViewPosition(200, 180);
        trackViewport.setViewPosition(0, timelineViewport.getViewPositionY());
        lastTimelineX = timelineViewport.getViewPositionX();
        lastPianoX = pianoViewport.getViewPositionX();
        lastTimelineY = timelineViewport.getViewPositionY();
        lastTrackY = trackViewport.getViewPositionY();
    };
    for (const bool vertical : { false, true })
    {
        auto* bar = dynamic_cast<ZoomScrollBar*>(vertical ? &timelineViewport.getVerticalScrollBar()
                                                        : &timelineViewport.getHorizontalScrollBar());
        check("upper_custom_scrollbar_installed", bar != nullptr);
        if (bar == nullptr) return false;
        const auto anchor = [&](bool leading)
        {
            if (vertical)
                return (timelineViewport.getViewPositionY() + (leading ? timelineViewport.getViewHeight() : 0)
                    - timeline.getRulerHeight()) / static_cast<double>(timeline.getRowHeight());
            return timeline.secondsForPixel(timelineViewport.getViewPositionX()
                + (leading ? timelineViewport.getViewWidth() : 0));
        };
        for (const bool leading : { true, false })
            for (const bool inward : { true, false })
            {
                resetUpper();
                const auto before = anchor(leading);
                const auto lowerPosition = pianoViewport.getViewPosition();
                drag(*bar, bar->gripCentre(leading), (leading ? 8.0f : -8.0f) * (inward ? 1.0f : -1.0f));
                timerCallback();
                const auto zoom = vertical ? timelineVerticalZoom : timelineHorizontalZoom;
                check("upper_endpoint_changes_zoom_in_correct_direction", inward ? zoom > (vertical ? 1.0 : 140.0)
                                                                                 : zoom < (vertical ? 1.0 : 140.0));
                check("upper_opposite_endpoint_stays_anchored", std::abs(anchor(leading) - before) < (vertical ? 0.02 : 0.02));
                check("upper_zoom_does_not_scroll_lower", pianoViewport.getViewPosition() == lowerPosition);
                check("upper_track_headers_stay_aligned", trackViewport.getViewPositionY() == timelineViewport.getViewPositionY());
            }
        resetUpper();
        const auto oldStart = bar->getCurrentRangeStart();
        drag(*bar, bar->thumbBounds().getCentre(), 12.0f);
        check("upper_middle_still_scrolls", bar->getCurrentRangeStart() > oldStart);
        check("upper_middle_does_not_zoom", timelineHorizontalZoom == 140.0 && timelineVerticalZoom == 1.0);
        resetUpper();
        timelineViewport.onZoomBegin(vertical, false);
        timelineViewport.onZoomDrag(vertical, 1000.0);
        check("upper_maximum_zoom_is_clamped", vertical ? timelineVerticalZoom == 2.0 : timelineHorizontalZoom == 8000.0);
        timelineViewport.onZoomBegin(vertical, false);
        timelineViewport.onZoomDrag(vertical, 0.001);
        check("upper_minimum_zoom_is_clamped", vertical ? timelineVerticalZoom == 0.5 : timelineHorizontalZoom == 40.0);
    }
    for (const bool vertical : { false, true })
        for (const double scale : { 0.8, 1.2 })
        {
            resetUpper();
            timelineViewport.setViewPosition(0, 0);
            timelineViewport.onZoomBegin(vertical, false);
            timelineViewport.onZoomDrag(vertical, scale);
            check("upper_origin_stays_at_zero", timelineViewport.getViewPosition() == juce::Point<int>());
        }
    timelineViewport.setSize(2000, 1200);
    timeline.setPixelsPerSecond(40.0f);
    timeline.setRowHeight(48.0f);
    for (auto* bar : { &timelineViewport.getHorizontalScrollBar(), &timelineViewport.getVerticalScrollBar() })
        check("upper_grips_remain_visible_at_full_range", bar->isVisible() && !bar->autoHides()
            && bar->getCurrentRangeSize() >= bar->getRangeLimit().getLength()
            && !dynamic_cast<ZoomScrollBar&>(*bar).thumbBounds().isEmpty());
    resized();
    resetUpper();
    check("zoom_does_not_modify_notes", project.revisionNumber() == revision);
    reset();
    pianoViewport.setViewPosition(0, juce::jmax(0, juce::roundToInt(pianoRoll.diagnosticYForMidi(60)) - 130));
    if (auto stream = directory.getChildFile("scroll-zoom.png").createOutputStream())
        ok = juce::PNGImageFormat().writeImageToStream(createComponentSnapshot(getLocalBounds()), *stream) && ok;
    else ok = false;
    return ok;
}
}
