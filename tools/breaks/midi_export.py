"""The faults --smoke-midi-export has to notice.

Each is the inverse of one thing the MIDI writer does: counting in beats and
not in seconds, carrying the tempo map, the lyrics and how loud each note is,
ending every note, giving way where two of one pitch overlap, starting no
earlier than the song does, keeping each track apart, and refusing a project
with nothing in it.
"""
import io
import pathlib
import sys

SRC = pathlib.Path("C:/hachi-src/juce/src")
M = "ProjectModel.cpp"

CASES = {
    # Seconds at the opening tempo: right until the song changes speed.
    "seconds_not_beats": [(M,
        """            data.quarterPositionForSeconds(seconds) * ticksPerQuarter));""",
        """            seconds * data.bpm / 60.0 * ticksPerQuarter)); // BREAK seconds_not_beats""")],
    # The speed is written once and never changes again.
    "no_tempo_changes": [(M,
        """    for (const auto& change : data.tempoChanges)
        speeds.emplace_back(juce::jmax(0.0, change.quarterPosition) * ticksPerQuarter,
                            change.bpm);""",
        """    // BREAK no_tempo_changes""")],
    # Every speed written, including the one a later event at the same tick
    # replaces: a reader that takes the first plays the whole song at a speed
    # nothing is sung at.
    "two_speeds_on_one_tick": [(M,
        """        if (index + 1 < speeds.size()
            && std::abs(speeds[index + 1].first - speeds[index].first) < 1.0e-9)
            continue;""",
        """        // BREAK two_speeds_on_one_tick""")],
    # The song goes out with no words in it.
    "no_lyrics": [(M,
        """            if (note.lyric.isNotEmpty())
                sequence.addEvent(juce::MidiMessage::textMetaEvent(5, note.lyric),
                                  note.start);""",
        """            // BREAK no_lyrics""")],
    # Every note starts and none of them ends.
    "no_note_offs": [(M,
        """            sequence.addEvent(juce::MidiMessage::noteOff(1, note.number), note.end);""",
        """            // BREAK no_note_offs""")],
    # Two of one pitch are left overlapping, and the first off ends both.
    "overlap_kept": [(M,
        """                notes[index].end = juce::jmax(notes[index].start + 1, notes[later].start);""",
        """                juce::ignoreUnused(later); // BREAK overlap_kept""")],
    # A note before the timeline's start is written at a negative tick.
    "negative_ticks": [(M,
        """        return juce::jmax(0, juce::roundToInt(
            data.quarterPositionForSeconds(seconds) * ticksPerQuarter));""",
        """        return juce::roundToInt( // BREAK negative_ticks
            data.quarterPositionForSeconds(seconds) * ticksPerQuarter);""")],
    # Every note the same loudness, whatever it was written at.
    "velocity_flat": [(M,
        """                written.velocity = juce::jlimit(1, 127,
                    juce::roundToInt(note.gain * 100.0f));""",
        """                written.velocity = 100; // BREAK velocity_flat""")],
    # One track for the whole song: the parts are no longer apart.
    "one_track_for_all": [(M,
        """        sequence.updateMatchedPairs();
        midi.addTrack(sequence);""",
        """        sequence.updateMatchedPairs();
        if (&track == &data.tracks.front()) midi.addTrack(sequence); // BREAK one_track_for_all""")],
    # A project with no notes is written out as a file anyway.
    "empty_is_written": [(M,
        """    if (notesWritten == 0)
    {
        error = "this project has no notes to write";
        return false;
    }""",
        """    // BREAK empty_is_written""")],
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
