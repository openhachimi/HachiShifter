"""Rebuild the fixtures the regression suite runs against.

They live under tools/fixtures (ignored by git) and are made from things that
are already on the machine: a UTAU voicebank and the engine.  The suite and
these scripts are in the repository because the previous copies sat in %TEMP%,
where Windows deleted them.

usage: python tools/make_fixtures.py [--force]
"""

from __future__ import annotations

import glob
import hashlib
import json
import os
import pathlib
import shutil
import subprocess
import sys

TOOLS = pathlib.Path(__file__).resolve().parent
FIXTURES = TOOLS / "fixtures"
EXE = TOOLS.parent / "build-win" / "HachiShifterNext_artefacts" / "RelWithDebInfo" / "HachiShifter Next.exe"
BANK = pathlib.Path(r"F:\UTAU\voice\New Geping UTAU Database")
REAL_USTS = r"F:\UTAU\voice"
ENGINE = pathlib.Path(r"D:\人声合成论文\utau化\build\WCSNDM.exe")
CRLF = "\r\n"
FORCE = "--force" in sys.argv


def mcp(commands):
    """Drive the editor's MCP mode, one JSON-RPC request per line."""
    lines = [json.dumps({"jsonrpc": "2.0", "id": 1, "method": "initialize", "params": {}})]
    for index, (tool, arguments) in enumerate(commands, start=2):
        lines.append(json.dumps({"jsonrpc": "2.0", "id": index, "method": "tools/call",
                                 "params": {"name": tool, "arguments": arguments}}))
    done = subprocess.run([str(EXE), "--mcp"], input="\n".join(lines) + "\n",
                          capture_output=True, text=True, encoding="utf-8", timeout=300)
    return done.stdout


def bank_aliases(limit):
    """Aliases the bank really has, so the notes can be rendered."""
    raw = (BANK / "oto.ini").read_bytes()
    for encoding in ("utf-8", "cp932", "gbk"):
        try:
            text = raw.decode(encoding)
            break
        except UnicodeDecodeError:
            continue
    aliases = []
    for line in text.splitlines():
        if "=" not in line:
            continue
        wav, rest = line.split("=", 1)
        alias = rest.split(",")[0].strip() or os.path.splitext(wav)[0]
        if alias and alias not in aliases:
            aliases.append(alias)
    return aliases[:limit]


def make_project():
    """A hundred-note UTAU project: what most of the checks open."""
    target = FIXTURES / "big100.hjpx"
    if target.exists() and not FORCE:
        print("project: kept")
        return
    aliases = bank_aliases(40) or ["a"]
    notes = []
    for index in range(100):
        # Rests, for the checks about gaps -- but only in the second half.
        # The checks that paste and ripple read notes at a named second and
        # expect an unbroken half-second grid to find them on.
        lyric = "R" if index >= 40 and index % 12 == 11 else aliases[index % len(aliases)]
        pitch = 60 + (index % 12)
        notes += [f"[#{index:04d}]", "Length=480", f"Lyric={lyric}", f"NoteNum={pitch}"]
        if index % 5 == 0:
            notes += ["PBS=-40;0", "PBW=80", "PBY="]
        if index % 17 == 3:
            notes.append("VBR=65,180,35,20,20,0,0,0")
    ust = FIXTURES / "big100.ust"
    ust.write_text(CRLF.join(["[#VERSION]", "UST Version1.2", "[#SETTING]", "Tempo=120.00",
                              "Tracks=1", "ProjectName=big100", "Mode2=True"] + notes
                             + ["[#TRACKEND]", ""]), encoding="utf-8")
    # With the bank bound: the notes then reach back for their consonants,
    # which is what the checks about touching notes and their envelopes need.
    out = mcp([("project_new", {}),
               ("import_ust", {"path": str(ust)}),
               ("project_snapshot", {}),
               ("project_save", {"path": str(target)})])
    import re
    found = re.search(r"track_[0-9a-fA-F]+", out)
    if found:
        mcp([("project_open", {"path": str(target)}),
             ("set_track", {"track_id": found.group(0), "voicebank_directory": str(BANK)}),
             ("project_save", {"path": str(target)})])
    if not target.exists():
        sys.exit("project: MCP did not save it\n" + out[-2000:])
    print(f"project: {target.name} {target.stat().st_size} bytes")


def make_wide_project():
    """Twenty long notes.  A note's regions are drawn inside it, and the
    handles answer a click within eleven pixels: at the roll's own zoom the
    boundaries of a half-second note are barely that far apart, so the checks
    that click on them need notes with room."""
    target = FIXTURES / "wide.hjpx"
    if target.exists() and not FORCE:
        print("wide project: kept")
        return
    aliases = bank_aliases(20) or ["a"]
    notes = []
    for index in range(20):
        # Two seconds each at 120, and no rests: every note has the one before
        # it to reach back into.
        notes += [f"[#{index:04d}]", "Length=1920", f"Lyric={aliases[index % len(aliases)]}",
                  f"NoteNum={60 + index % 7}"]
    ust = FIXTURES / "wide.ust"
    ust.write_text(CRLF.join(["[#VERSION]", "UST Version1.2", "[#SETTING]", "Tempo=120.00",
                              "Tracks=1", "ProjectName=wide", "Mode2=True"] + notes
                             + ["[#TRACKEND]", ""]), encoding="utf-8")
    out = mcp([("project_new", {}),
               ("import_ust", {"path": str(ust)}),
               ("project_snapshot", {}),
               ("project_save", {"path": str(target)})])
    import re
    found = re.search(r"track_[0-9a-fA-F]+", out)
    if found:
        mcp([("project_open", {"path": str(target)}),
             ("set_track", {"track_id": found.group(0), "voicebank_directory": str(BANK)}),
             ("project_save", {"path": str(target)})])
    if not target.exists():
        sys.exit("wide project: MCP did not save it\n" + out[-2000:])
    print(f"wide project: {target.name} {target.stat().st_size} bytes")


def make_sample_wav():
    """A recording under a path with no spaces: the checks that take one read
    their argument without unquoting it, so a path with spaces arrives in
    pieces."""
    target = FIXTURES / "sample.wav"
    if target.exists() and not FORCE:
        print("sample wav: kept")
        return
    source = BANK / "a-c.wav"
    if not source.exists():
        source = next(BANK.glob("*.wav"))
    shutil.copy2(source, target)
    print("sample wav: made")


def make_package():
    """A packaged copy of the editor: the exe with the engine beside it, which
    is the question --smoke-active-resampler exists to answer.  A package, not
    the build folder -- an engine left beside the build is found by every other
    check's AudioEngine too, and one that renders through WCSNDM cannot hear
    which part of a recording was read, because WCSNDM sings it at the note's
    pitch either way."""
    package = FIXTURES / "package"
    exe = package / EXE.name
    engine = package / "engines" / "WCSNDM.exe"
    engine.parent.mkdir(parents=True, exist_ok=True)
    # Always the exe that was just built: an old copy answers for an old build.
    if not exe.exists() or exe.stat().st_mtime < EXE.stat().st_mtime:
        shutil.copy2(EXE, exe)
    if not ENGINE.exists():
        print("package: no engine to copy")
        return
    if not engine.exists() or FORCE:
        shutil.copy2(ENGINE, engine)
    # An engine planted beside the build by an earlier run is still found.
    stale = EXE.parent / "engines"
    if stale.exists():
        shutil.rmtree(stale, ignore_errors=True)
        print("package: removed the engine beside the build")
    print("package: ready")


def make_mou_bank():
    """One sample, oto.ini only: the 谋 checks seed their own files beside it."""
    target = FIXTURES / "moubank"
    if target.exists() and not FORCE:
        print("mou bank: kept")
        return
    target.mkdir(parents=True, exist_ok=True)
    source = BANK / "si.wav"
    if not source.exists():
        source = next(BANK.glob("*.wav"))
    shutil.copy2(source, target / source.name)
    # Two aliases on one wav and one offset, which is ordinary in a real bank.
    # The piece the entry describes is 600 of the recording's 665 ms, so the
    # region boundaries below have somewhere to be: with a shorter piece they
    # all land on its end, and the regions come out with no width at all.
    (target / "oto.ini").write_text(
        f"{source.name}=si,0,80,-600,120,30{CRLF}{source.name}=si2,0,80,-600,120,30{CRLF}",
        encoding="utf-8")
    # The four regions, in the shape a real 谋 bank writes them: <wav>=
    # <classes>,<oto_offset>,<b1>,<b2>,<b3>, all three boundaries always
    # written.  b1 is the consonant's end, and it is before the 120 ms
    # preutterance -- that is what puts it in the lead-in rather than in the
    # note -- while the rest are far enough apart to be clicked one at a time.
    (target / "otomou.ini").write_text(
        f"{source.name}=CVVV,0,40,250,450{CRLF}", encoding="utf-8")
    print("mou bank: made")


def make_ust_encoding():
    """Fixtures for --smoke-ust-encoding: one file per encoding, plus the text
    each must decode to.  Encoded by Python's codecs, so the editor's Windows
    decoding is held to a second implementation."""
    out = FIXTURES / "ust-encoding"
    if out.exists() and not FORCE and any(out.glob("*.ust")):
        print("ust encoding: kept")
        return
    if out.exists():
        shutil.rmtree(out)
    out.mkdir(parents=True)

    def ust(lyrics, project="enc", voice="", outfile=""):
        lines = ["[#VERSION]", "UST Version1.2", "[#SETTING]", "Tempo=120.00",
                 "Tracks=1", "ProjectName=" + project]
        if voice:
            lines.append("VoiceDir=" + voice)
        if outfile:
            lines.append("OutFile=" + outfile)
        for index, lyric in enumerate(lyrics):
            lines += [f"[#{index:04d}]", "Length=480", "Lyric=" + lyric, "NoteNum=60"]
        lines.append("[#TRACKEND]")
        return CRLF.join(lines) + CRLF

    def write(name, text, encoding, bom=False):
        data = text.encode(encoding)
        if bom:
            data = bytes([0xEF, 0xBB, 0xBF]) + data
        (out / (name + ".ust")).write_bytes(data)
        (out / (name + ".utf8")).write_bytes(text.encode("utf-8"))

    write("sjis-kana", ust(["あ", "か", "さ", "R", "きゃ", "ん", "ー", "を"],
                           project="テスト", voice="%VOICE%重音テト"), "cp932")
    write("sjis-romaji-header", ust(["a", "ka", "sa", "R"], project="新規プロジェクト"), "cp932")
    # No kana: held only to a Japanese machine, where it reads as it did.
    write("sjis-kanji", ust(["愛", "夢", "歌", "空", "心"]), "cp932")
    write("sjis-single-kana", ust(["あ"]), "cp932")
    write("sjis-vcv", ust(["- あ", "a か", "a さ", "a R"], voice="%VOICE%波音リツ"), "cp932")
    write("gbk-kana", ust(["あ", "い", "う"], project="马仔很皮"), "gbk")
    write("gbk-single-kana", ust(["あ"]), "gbk")
    write("gbk-hanzi", ust(["啊", "我", "爱", "你", "中", "的"]), "gbk")
    write("gbk-pinyin-header", ust(["a", "ba"], outfile="哔哩哔哩の米娜桑.wav"), "gbk")
    # Taiwan and Korea, held only to their own machines.  No kana in Big5:
    # Windows' code page 950 maps them to private use where Python's codec has
    # real kana, so the two could never agree on the text.
    write("big5-hanzi", ust(["愛", "我", "你", "的", "中", "文", "歌"], project="測試"), "cp950")
    write("euckr-hangul", ust(["아", "가", "사", "랑", "해"], project="테스트"), "cp949")
    write("utf8-japanese", ust(["あ", "愛"], project="テスト"), "utf-8")
    write("utf8-bom-chinese", ust(["我", "あ"], project="猫中毒"), "utf-8", bom=True)

    # A real song carried over to Shift-JIS: the lyrics survive, and header
    # lines naming things in simplified Chinese, which Shift-JIS has no
    # characters for, are left out.
    source = pathlib.Path(REAL_USTS) / "jiege3" / "低八度.ust"
    if source.exists():
        text = source.read_bytes().decode("gbk")
        kept = [line for line in text.split(CRLF) if _encodable(line, "cp932")]
        write("sjis-e2e-dibadu", CRLF.join(kept), "cp932")

    # Every real UST on the disk, once each, exactly as found.
    seen, count = set(), 0
    for path in sorted(glob.glob(REAL_USTS + "/**/*.ust", recursive=True)):
        data = pathlib.Path(path).read_bytes()
        digest = hashlib.md5(data).hexdigest()
        if digest in seen:
            continue
        seen.add(digest)
        try:
            data.decode("utf-8")
            kind, reference = "utf8", data.decode("utf-8")
        except UnicodeDecodeError:
            kind, reference = "gbk", data.decode("gbk")
        name = f"{kind}-e2e-real{count:02d}"
        (out / (name + ".ust")).write_bytes(data)
        (out / (name + ".utf8")).write_bytes(reference.encode("utf-8"))
        count += 1
    print(f"ust encoding: {len(list(out.glob('*.ust')))} files ({count} real)")


def _encodable(text, encoding):
    try:
        text.encode(encoding)
        return True
    except UnicodeEncodeError:
        return False


def main():
    FIXTURES.mkdir(parents=True, exist_ok=True)
    if not EXE.exists():
        sys.exit(f"build the editor first: {EXE}")
    make_project()
    make_wide_project()
    make_sample_wav()
    make_package()
    make_mou_bank()
    make_ust_encoding()
    print("fixtures in", FIXTURES)


if __name__ == "__main__":
    main()
