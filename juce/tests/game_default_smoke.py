#!/usr/bin/env python3
"""Verify default portable GAME inference without model-path overrides."""
from __future__ import annotations
import hashlib
import json
import os
import pathlib
import re
import subprocess
import sys
import time
from mcp_smoke import McpClient


def main():
    binary, vocal, output = map(lambda p: pathlib.Path(p).resolve(), sys.argv[1:])
    output.mkdir(parents=True, exist_ok=True)
    for key in list(os.environ):
        if key.startswith("HACHISHIFTER_GAME") or key.startswith("HACHISHIFTER_FCPE"):
            del os.environ[key]
    os.environ.pop("HACHISHIFTER_INFERENCE", None)
    os.environ.pop("HACHISHIFTER_DEVICE", None)
    startup = None
    if os.name == "nt":
        startup = subprocess.STARTUPINFO()
        startup.dwFlags |= subprocess.STARTF_USESHOWWINDOW
        startup.wShowWindow = 0
    checks = []
    def run(name, args, timeout=240):
        started = time.monotonic()
        result = subprocess.run(args, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
            encoding="utf-8", errors="replace", startupinfo=startup, timeout=timeout,
            cwd=str(output))
        (output / (name + ".log")).write_text(result.stdout, encoding="utf-8")
        assert result.returncode == 0, result.stdout
        checks.append({"name": name, "ok": True, "seconds": round(time.monotonic()-started, 3)})
        print(name, "PASS", flush=True)
        return result.stdout
    status = run("portable-status", [str(binary), "--inspect-analysis"])
    assert "GAME+FCPE (GAME medium," in status and "game_ready=1" in status and "fcpe_ready=1" in status
    run("default-config", [str(binary), "--smoke-game-defaults", str(output)])
    config = json.loads((output / "game-defaults.json").read_text(encoding="utf-8-sig"))
    assert config["ok"] and all(c["ok"] for c in config["checks"])
    smoke = pathlib.Path(__file__).with_name("game_fcpe_smoke.py")
    run("real-model", [sys.executable, str(smoke), str(binary),
        str(binary.parent/"models/game/medium"), str(binary.parent/"models/fcpe/fcpe.onnx"), str(vocal)])
    client = McpClient(binary)
    try:
        response = client.call("analysis_status")
        assert "GAME+FCPE (GAME medium," in response and "game_variant=medium" in response, response
        checks.append({"name":"mcp-default-medium", "ok":True})
        response = client.call("analysis_status", {"game_model":"small"})
        assert "game_variant=small" in response and "active=native-hq" in response, response
        checks.append({"name":"mcp-explicit-small-and-fallback", "ok":True})
        response = client.call("analysis_status", {"game_model":"medium"})
        assert "GAME+FCPE (GAME medium," in response, response
        checks.append({"name":"mcp-explicit-medium", "ok":True})
        started = time.monotonic()
        response = client.call("import_audio", {"path":str(vocal)})
        assert "GAME+FCPE" in response, response
        snapshot = json.loads(client.call("project_snapshot"))
        notes = snapshot["tracks"][0]["clips"][0]["notes"]
        assert len(notes) > 0
        (output/"mcp-import.txt").write_text(response, encoding="utf-8")
        checks.append({"name":"mcp-vocal-default-inference", "ok":True,
            "notes":len(notes), "seconds":round(time.monotonic()-started,3)})
    finally:
        client.close()
    manifest = []
    for relative in ("models/game/medium", "models/fcpe"):
        for path in sorted((binary.parent/relative).glob("*")):
            if path.is_file():
                manifest.append({"path":str(path.relative_to(binary.parent)),"bytes":path.stat().st_size,
                    "sha256":hashlib.sha256(path.read_bytes()).hexdigest()})
    report = {"ok":True, "binary":str(binary), "exe_sha256":hashlib.sha256(binary.read_bytes()).hexdigest(),
        "variant":"medium", "backend":"GAME+FCPE", "vocal":str(vocal),
        "vocal_sha256":hashlib.sha256(vocal.read_bytes()).hexdigest(),
        "checks":checks,"configuration_checks":config["checks"],"models":manifest}
    (output/"game-default-validation.json").write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding="utf-8")
    print(json.dumps({"ok":True,"config_checks":len(config["checks"]),"groups":len(checks)},ensure_ascii=False))

if __name__ == "__main__":
    main()
