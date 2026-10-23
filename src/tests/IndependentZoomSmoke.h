#pragma once

namespace hachi
{
inline bool MainComponent::diagnosticIndependentZoom()
{
    setSize(1280, 800);
    const auto track = project.addTrack("Independent zoom", true);
    project.setTrackPitchAlgorithm(track, PitchAlgorithm::utau);
    const auto clip = project.addClip(track, 0.0, 24.0);
    (void) project.addNote(clip, 2.0, 1.0, 60.0f);
    project.dispatchPendingMessages();
    pianoRoll.diagnosticRefresh();
    diagnosticSelectTrack(track);
    resized();
    pianoViewport.setViewPosition(100, 700);
    timelineViewport.setViewPosition(200, 0);
    lastPianoX = pianoViewport.getViewPositionX();
    lastTimelineX = timelineViewport.getViewPositionX();
    const auto revision = project.revisionNumber();
    bool ok = true;
    const auto check = [&](const char* name, bool passed)
    { ok = ok && passed; std::cout << name << '=' << passed << '\n'; };
    const auto lower = [&]
    {
        return std::array<double, 6> { zoomSlider.getValue(), vZoomSlider.getValue(),
            static_cast<double>(pianoRoll.getWidth()), static_cast<double>(pianoRoll.getHeight()),
            static_cast<double>(pianoViewport.getViewPositionX()), static_cast<double>(pianoViewport.getViewPositionY()) };
    };
    const auto upper = [&]
    {
        return std::array<double, 6> { timelineHorizontalZoom, timelineVerticalZoom,
            static_cast<double>(timeline.getWidth()), static_cast<double>(timeline.getRowHeight()),
            static_cast<double>(timelineViewport.getViewPositionX()), static_cast<double>(timelineViewport.getViewPositionY()) };
    };
    const auto wheel = [&](EditorViewport& view, bool horizontal, float delta)
    {
        const juce::Point<float> at(150, 80);
        const auto mods = juce::ModifierKeys(juce::ModifierKeys::ctrlModifier
            | (horizontal ? juce::ModifierKeys::shiftModifier : 0));
        const juce::MouseEvent event(juce::Desktop::getInstance().getMainMouseSource(), at, mods,
            0, 0, 0, 0, 0, &view, &view, juce::Time::getCurrentTime(), at,
            juce::Time::getCurrentTime(), 0, false);
        juce::MouseWheelDetails details {};
        details.deltaY = delta;
        view.mouseWheelMove(event, details);
        timerCallback(); // Catch delayed scroll synchronization undoing the isolation.
    };
    for (const bool horizontal : { true, false })
        for (const float delta : { 0.2f, -0.2f })
        {
            const auto pianoBefore = lower(), timelineBefore = upper();
            wheel(timelineViewport, horizontal, delta);
            check("upper_wheel_leaves_lower_scale_and_position_unchanged", lower() == pianoBefore);
            check("upper_wheel_changes_upper_scale", upper()[horizontal ? 0 : 1] != timelineBefore[horizontal ? 0 : 1]);
            const auto upperBefore = upper(), lowerBefore = lower();
            wheel(pianoViewport, horizontal, delta);
            check("lower_wheel_leaves_upper_scale_and_position_unchanged", upper() == upperBefore);
            check("lower_wheel_changes_lower_scale", lower()[horizontal ? 0 : 1] != lowerBefore[horizontal ? 0 : 1]);
        }
    for (const bool horizontal : { true, false })
    {
        const auto before = lower(), timelineBefore = upper();
        wheel(trackViewport, horizontal, 0.2f);
        check("track_headers_zoom_only_upper_pane", lower() == before
            && upper()[horizontal ? 0 : 1] != timelineBefore[horizontal ? 0 : 1]);
    }
    for (const bool vertical : { false, true })
    {
        const auto lowerBefore = lower(), upperBefore = upper();
        timelineViewport.onZoomBegin(vertical, false);
        timelineViewport.onZoomDrag(vertical, 1.15);
        timerCallback();
        check("upper_grips_leave_lower_unchanged", lower() == lowerBefore);
        check("upper_grips_change_upper_scale", upper()[vertical ? 1 : 0] != upperBefore[vertical ? 1 : 0]);
        const auto before = upper();
        pianoViewport.onZoomBegin(vertical, false);
        pianoViewport.onZoomDrag(vertical, 1.15);
        timerCallback();
        check("lower_grips_leave_upper_unchanged", upper() == before);
    }
    check("zoom_does_not_change_project_data", project.revisionNumber() == revision);
    return ok;
}
}
