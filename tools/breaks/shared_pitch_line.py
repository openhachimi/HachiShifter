"""The faults --smoke-shared-pitch-line has to notice.

Each is the inverse of one thing a pitch point now is: heard at its own moment
whichever note it was placed on, drawn where it is heard, seen while it is
being dragged, only ever joining notes whose points really reach into each
other -- with no automatic bridge laid over it -- never offering, taking,
adding or dragging a point where a later note's bend decides, never showing or
hiding a handle while a point is held, and never moving a point nobody moved.
"""
import io
import pathlib
import sys

SRC = pathlib.Path("C:/hachi-src/juce/src")
M = "ProjectModel.cpp"
A = "AudioEngine.cpp"
P = "PianoRollComponent.cpp"

CASES = {
    # What it was: every note sung along its own points only.
    "render_reads_only_its_own": [(A,
        """        const auto* sharedMember = sharedLines.memberFor(note.id);""",
        """        const SharedPitchLineMember* sharedMember = nullptr; // BREAK render_reads_only_its_own
        juce::ignoreUnused(sharedLines);""")],
    # The line drawn is each note's own again, whatever is sung.
    "screen_reads_only_its_own": [(P,
        """    if (const auto* member = sharedLinesFor(track).memberFor(note.id))
        return member->line->midiAt(absoluteStart + time);""",
        """    // BREAK screen_reads_only_its_own""")],
    # The line only catches up with the point once it is let go.
    "drag_not_seen_until_released": [(P,
        """    if (followDrag && dragMode == DragMode::pointPitch && !pitchStroke.empty()
        && draggedNote.isNotEmpty())
    {
        auto holdsDragged = false;""",
        """    if (false && followDrag) // BREAK drag_not_seen_until_released
    {
        auto holdsDragged = false;""")],
    # Notes a real gap apart are joined too, when one's bend runs on past the
    # other's start.  (Touching notes whose points meet on the boundary do
    # share a line now; a gap still keeps two lines apart.)
    "joined_across_a_gap": [(M,
        """        if (left.rest || right.rest || gap < -0.002 || gap > shortRestSeconds)
            return false;""",
        """        if (left.rest || right.rest || gap < -0.002) // BREAK joined_across_a_gap
            return false;""")],
    # A rest of a few tens of milliseconds -- a UST's, come in as a gap --
    # splits the line a bend is drawn across, as a real gap does.
    "short_rest_splits_the_line": [(M,
        """        if (left.rest || right.rest || gap < -0.002 || gap > shortRestSeconds)
            return false;""",
        """        if (left.rest || right.rest || gap < -0.002 || gap > 0.002) // BREAK short_rest_splits_the_line
            return false;""")],
    # A curve carries on over a rest into a note with no points of its own.
    "curve_carried_over_a_rest": [(M,
        """        if (gap > 0.002 && !(left.placed && right.placed)) return false;""",
        """        // BREAK curve_carried_over_a_rest""")],
    # The editor's automatic bridge is laid over a line two notes share.
    "bridge_over_the_line": [(A,
        """        if (previousPlan.sharesNext && nextPlan.sharesPrevious) return;""",
        """        // BREAK bridge_over_the_line""")],
    # Every point is offered, the overruled ones included.
    "overruled_points_offered": [(P,
        """    return member == nullptr || member->owns(absoluteSeconds);""",
        """    return true || member == nullptr; // BREAK overruled_points_offered""")],
    # Not drawn, but still there to be taken hold of.
    "overruled_points_taken": [(P,
        """                    if (!pitchHandleOffered(*note, hit.startSeconds + hit.clipStartSeconds
                                                       + anchors[index].timeSeconds))
                        continue;""",
        """                    // BREAK overruled_points_taken""")],
    # A point added over a tail goes to whichever line lies nearest, the one
    # no longer sung there included.
    "point_added_to_the_nearest": [(P,
        """                if (local < from || local > to) continue;""",
        """                juce::ignoreUnused(from, to); // BREAK point_added_to_the_nearest""")],
    # A point can be dragged on into the stretch a later note decides, and
    # vanishes from under the pointer.
    "drag_runs_past_the_owner": [(P,
        """            if (anchor.timeSeconds > pointDragOwnedUntil)
            {""",
        """            if (false) // BREAK drag_runs_past_the_owner
            {""")],
    # Which handles are shown follows the drag, so a point dragged across
    # another note's reach makes that note's points come and go under it.
    "handles_follow_the_drag": [(P,
        """    const auto* member = sharedLinesFor(*track, false).memberFor(note.id);""",
        """    const auto* member = sharedLinesFor(*track).memberFor(note.id); // BREAK handles_follow_the_drag""")],
    # Every point of a note held to its own span on any edit: a bend running
    # on past the note's end is pulled back to it, points nobody moved.
    "far_points_pulled_back": [(M,
        """                        const auto maximumTime = storeControlPoints
                            ? note.durationSeconds + 30.0 : note.durationSeconds;""",
        """                        const auto maximumTime = note.durationSeconds; // BREAK far_points_pulled_back""")],
    # No join: a note holds its last point's pitch right up to the next note's
    # first point and then drops straight down there.
    "step_at_the_hand_over": [(M,
        """            if (ownUntil[index] - lastPoint[index] > 1.0e-9)
                shared->pieces.push_back(std::move(join));""",
        """            juce::ignoreUnused(join); // BREAK step_at_the_hand_over""")],
    # A head reaches back as far as the note before's first point, over its
    # last one, which vanishes from under it.
    "head_passes_the_previous_end": [(P,
        """                        minimumAbsolute = std::min(
                            lastShownPitchPoint(previous->id, previous->startSeconds)
                                .value_or(previous->startSeconds),""",
        """                        minimumAbsolute = std::min( // BREAK head_passes_the_previous_end
                            previous->startSeconds,""")],
    # Two points on one vertical line: the earlier note's is overruled there,
    # and vanishes.
    "equal_time_hides_the_end": [(M,
        """                    && point.timeSeconds <= ownUntil[index] + 1.0e-9)""",
        """                    && point.timeSeconds < ownUntil[index] - 1.0e-6) // BREAK equal_time_hides_the_end"""),
        (M,
        """            member.ownTo = std::isfinite(ownUntil[index]) ? ownUntil[index]
                                                          : std::numeric_limits<double>::infinity();""",
        """            member.ownTo = std::isfinite(ownUntil[index]) ? ownUntil[index] - 1.0e-6 // BREAK equal_time_hides_the_end
                                                          : std::numeric_limits<double>::infinity();""")],
    # A tail stops a millisecond short of the next note's first point, never
    # on its line.
    "tail_stops_short": [(P,
        """                            pointDragOwnedUntil = member->takeover - positionedNote.startSeconds;""",
        """                            pointDragOwnedUntil = member->takeover - positionedNote.startSeconds
                                - minimumAnchorSeparation; // BREAK tail_stops_short""")],
    # The limit is taken as it is, even where the head already stands past
    # it: taking hold of it pulls it forward.
    "head_pulled_to_the_limit": [(P,
        """                            positionedNote.startSeconds + pitchStroke.front().timeSeconds);""",
        """                            1.0e9); // BREAK head_pulled_to_the_limit""")],
}


def load(name):
    return io.open(SRC / name, encoding="utf-8", newline="").read()


if sys.argv[1] == "list":
    print(chr(10).join(CASES))
    sys.exit(0)

if sys.argv[1] == "check":
    problems = 0
    for which, swaps in CASES.items():
        for name, good, bad in swaps:
            text = load(name)
            ok = text.count(good) == 1 and text.count(bad) == 0
            problems += 0 if ok else 1
            print(("ok  " if ok else "BAD "), which, "good", text.count(good), "bad", text.count(bad))
    print("cases with problems:", problems)
    sys.exit(1 if problems else 0)

action, names = sys.argv[1], sys.argv[2:]
for which in names:
    swaps = CASES[which] if action == "break" else list(reversed(CASES[which]))
    for name, good, bad in swaps:
        text = load(name)
        old, new = (good, bad) if action == "break" else (bad, good)
        assert text.count(old) == 1, (which, action, text.count(old), old[:60])
        io.open(SRC / name, "w", encoding="utf-8", newline="").write(text.replace(old, new, 1))
    print(action, which)
