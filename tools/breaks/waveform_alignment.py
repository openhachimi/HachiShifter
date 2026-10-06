"""The faults --smoke-waveform-alignment has to notice.

Each is the inverse of one thing a note's picture now is: shaped by the very
gain the mix puts on its samples -- envelope, fade in, and the fade out where
the next note takes over -- and, in the faint layer behind the envelope lane,
by all of that but the envelope's shape, with the envelope still holding its
end value past its last point -- and that the roll draws a note by the
envelope on screen, not by the one a render of a selection shaped it with.
"""
import io
import pathlib
import sys

SRC = pathlib.Path("C:/hachi-src/juce/src")
R = "backend/UtauRenderer.cpp"
P = "PianoRollComponent.cpp"

CASES = {
    # What it was: the envelope alone.  The piece runs on past the hand-over at
    # full height, drawn where the line has already ended and nothing is heard.
    "picture_without_the_fades": [(R,
        """                              [&gain, sampleAt](double localSeconds)
                              {
                                  return gain.at(sampleAt(localSeconds));
                              },""",
        """                              [&envelope](double localSeconds) // BREAK picture_without_the_fades
                              {
                                  return amplitudeGainAt(envelope, localSeconds);
                              },""")],
    # The faint layer leaves the fades out as well as the envelope, so the
    # envelope lane shows the whole piece past the hand-over.
    "ghost_without_the_fades": [(R,
        """                                  return gain.at(sampleAt(localSeconds), !inside);""",
        """                                  return inside ? 1.0f // BREAK ghost_without_the_fades
                                                : gain.at(sampleAt(localSeconds), true);""")],
    # The faint layer leaves the envelope out past its last point too, where it
    # holds silence that no point in the lane can move.
    "ghost_ignores_the_envelope_end": [(R,
        """                                  const auto inside = envelope.size() < 2
                                      || (localSeconds >= envelope.front().timeSeconds
                                          && localSeconds <= envelope.back().timeSeconds);""",
        """                                  const auto inside = true; // BREAK ghost_ignores_the_envelope_end
                                  juce::ignoreUnused(envelope);""")],
    # The picture goes back to the audio as this render shaped it, which at
    # the end of a selection sings on past the line: the note it hands over
    # to in the song was not rendered with it.
    "picture_as_rendered": [(P,
        """    const auto value = high ? waveform.unshapedMaxima[bucket]
                            : waveform.unshapedMinima[bucket];
    const auto seconds = (static_cast<double>(bucket) + 0.5) / 1000.0
        - waveform.leadInSeconds;
    const auto db = amplitudeDbAt(envelope, seconds);
    return db <= -59.9f ? 0.0f : value * std::pow(10.0f, db / 20.0f);""",
        """    juce::ignoreUnused(envelope); // BREAK picture_as_rendered
    return high ? waveform.maxima[bucket] : waveform.minima[bucket];""")],
    # The faint layer shows the whole piece, past the envelope's ends as well.
    "lane_ghost_past_the_ends": [(P,
        """    if (withinEnvelope(waveform, envelope, bucket))
        return std::max(waveform.unshapedMaxima[bucket], -waveform.unshapedMinima[bucket]);""",
        """    if (true) // BREAK lane_ghost_past_the_ends
        return std::max(waveform.unshapedMaxima[bucket], -waveform.unshapedMinima[bucket]);""")],
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
