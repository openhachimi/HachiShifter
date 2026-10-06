"""Read an exported MIDI with nothing of the editor's, and hold it to the UST.

--smoke-midi-export reads the file back through JUCE, which is the same code
that wrote it; this parses the bytes itself -- chunks, variable-length delta
times, running status -- so the two agree about the file for reasons that are
not shared.  Every note's start tick, pitch and lyric is compared against the
UST the song came from.

    python tools/verify_midi.py song.mid song.ust

The MIDI is made by driving the editor's MCP mode:
    project_new, import_ust {path}, export_midi {path}
"""
import pathlib
import re
import struct
import sys


def varlen(data, index):
    value = 0
    while True:
        byte = data[index]
        index += 1
        value = (value << 7) | (byte & 0x7F)
        if not byte & 0x80:
            return value, index


def read_midi(path):
    data = pathlib.Path(path).read_bytes()
    assert data[:4] == b"MThd", "not a MIDI file"
    length, fmt, tracks, division = struct.unpack(">IHHH", data[4:14])
    index = 8 + length
    parsed = []
    for _ in range(tracks):
        assert data[index:index + 4] == b"MTrk", "track chunk expected"
        size = struct.unpack(">I", data[index + 4:index + 8])[0]
        end = index + 8 + size
        cursor = index + 8
        tick = 0
        status = None
        events = []
        while cursor < end:
            delta, cursor = varlen(data, cursor)
            tick += delta
            byte = data[cursor]
            if byte & 0x80:
                status = byte
                cursor += 1
            if status == 0xFF:                       # meta
                kind = data[cursor]
                cursor += 1
                size2, cursor = varlen(data, cursor)
                payload = data[cursor:cursor + size2]
                cursor += size2
                events.append((tick, "meta", kind, payload))
            elif status in (0xF0, 0xF7):             # sysex
                size2, cursor = varlen(data, cursor)
                cursor += size2
            else:
                kind = status & 0xF0
                size2 = 1 if kind in (0xC0, 0xD0) else 2
                payload = data[cursor:cursor + size2]
                cursor += size2
                events.append((tick, "midi", status, payload))
        parsed.append(events)
        index = end
    return fmt, division, parsed


def ust_notes(path):
    raw = pathlib.Path(path).read_bytes()
    try:
        text = raw.decode("utf-8")
    except UnicodeDecodeError:
        text = raw.decode("gbk", "replace")
    tick, notes = 0, []
    for block in text.split("[#")[1:]:
        fields = {}
        for line in block.splitlines()[1:]:
            if "=" in line:
                key, value = line.split("=", 1)
                fields[key.strip()] = value.strip()
        if "Length" not in fields:
            continue
        length = int(float(fields["Length"]))
        lyric = fields.get("Lyric", "").strip()
        if lyric in ("R", "r", "") or length <= 0:
            tick += length
            continue
        notes.append((tick, length, int(fields["NoteNum"]), lyric))
        tick += length
    return notes


midi_path, ust_path = sys.argv[1], sys.argv[2]
fmt, division, tracks = read_midi(midi_path)
print("format", fmt, "| division", division, "| tracks", len(tracks))

tempi = [(tick, 60e6 / int.from_bytes(payload, "big"))
         for tick, kind, code, payload in tracks[0] if kind == "meta" and code == 0x51]
print("tempo events:", [(t, round(b, 3)) for t, b in tempi][:4])

song = tracks[1]
ons = [(tick, payload[0], payload[1]) for tick, kind, status, payload in song
       if kind == "midi" and status & 0xF0 == 0x90 and payload[1] > 0]
offs = [(tick, payload[0]) for tick, kind, status, payload in song
        if kind == "midi" and (status & 0xF0 == 0x80
                               or (status & 0xF0 == 0x90 and payload[1] == 0))]
lyrics = [(tick, payload.decode("utf-8", "replace")) for tick, kind, code, payload in song
          if kind == "meta" and code == 0x05]
names = [payload.decode("utf-8", "replace") for tick, kind, code, payload in song
         if kind == "meta" and code == 0x03]
print("note ons", len(ons), "| offs", len(offs), "| lyrics", len(lyrics), "| track name", names)

expected = ust_notes(ust_path)
print("UST sounding notes:", len(expected))

problems = []
if len(ons) != len(expected):
    problems.append("note count %d vs %d" % (len(ons), len(expected)))
for index, ((tick, number, velocity), (want_tick, want_len, want_num, want_lyric)) \
        in enumerate(zip(ons, expected)):
    if tick != want_tick:
        problems.append("note %d starts at %d, the UST says %d" % (index, tick, want_tick))
    if number != want_num:
        problems.append("note %d is %d, the UST says %d" % (index, number, want_num))
    if index < len(lyrics) and lyrics[index][1] != want_lyric:
        problems.append("note %d sings %r, the UST says %r"
                        % (index, lyrics[index][1], want_lyric))
    if index < len(lyrics) and lyrics[index][0] != tick:
        problems.append("lyric %d is not on its note" % index)

# Every note ends, and none of one pitch is on twice at once.
sounding = {}
for tick, kind, status, payload in song:
    if kind != "midi":
        continue
    if status & 0xF0 == 0x90 and payload[1] > 0:
        if sounding.get(payload[0], 0):
            problems.append("note %d sounds twice at tick %d" % (payload[0], tick))
        sounding[payload[0]] = sounding.get(payload[0], 0) + 1
    elif status & 0xF0 in (0x80, 0x90):
        sounding[payload[0]] = sounding.get(payload[0], 0) - 1
left = {number: count for number, count in sounding.items() if count}
if left:
    problems.append("left sounding: %s" % left)

print("problems:", len(problems))
for problem in problems[:10]:
    print("  ", problem)
