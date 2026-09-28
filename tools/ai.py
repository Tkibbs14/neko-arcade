#!/usr/bin/env python3
"""Tiny OpenRouter client for Neko Arcade content generation (build-time only; the stick has no network).

Reads OPENROUTER_API_KEY from nakama-backend/.env without printing it. Pins the model and the provider
routing, disables reasoning for plain writing tasks, logs every call's cost to build/ai_spend.log.
Usage as a module:  from ai import chat;  text = chat(system, user, json_mode=True)
Usage from the shell: ai.py ping
"""
import json, os, sys, threading, time, urllib.request

_log_lock = threading.Lock()

ENV = "/mnt/c/Users/tyler/nakama-backend/.env"
LOG = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), "build", "ai_spend.log")
MODEL = "deepseek/deepseek-v4.1-flash"


def _key():
    for line in open(ENV, encoding="utf-8"):
        if line.startswith("OPENROUTER_API_KEY="):
            return line.split("=", 1)[1].strip().strip('"').strip("'")
    raise SystemExit("OPENROUTER_API_KEY not found in nakama-backend/.env")


def chat(system, user, json_mode=False, temperature=0.9, max_tokens=4000, model=MODEL, tag=""):
    body = {
        "model": model,
        "messages": [{"role": "system", "content": system}, {"role": "user", "content": user}],
        "temperature": temperature,
        "max_tokens": max_tokens,
        "reasoning": {"enabled": False},
        "usage": {"include": True},
    }
    if json_mode:
        body["response_format"] = {"type": "json_object"}
    req = urllib.request.Request("https://openrouter.ai/api/v1/chat/completions", data=json.dumps(body).encode(),
                                 headers={"Authorization": "Bearer " + _key(), "Content-Type": "application/json",
                                          "X-Title": "Neko Arcade content build"})
    for attempt in range(3):
        try:
            with urllib.request.urlopen(req, timeout=180) as r:
                j = json.load(r)
            break
        except urllib.error.HTTPError as e:
            msg = e.read().decode("utf-8", "replace")[:300]
            if e.code in (429, 502, 503) and attempt < 2:
                time.sleep(5 * (attempt + 1))
                continue
            raise SystemExit(f"OpenRouter HTTP {e.code}: {msg}")
    text = j["choices"][0]["message"].get("content") or ""
    usage = j.get("usage", {})
    os.makedirs(os.path.dirname(LOG), exist_ok=True)
    rec = json.dumps({"t": time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime()), "tag": tag, "model": j.get("model"),
                      "in": usage.get("prompt_tokens"), "out": usage.get("completion_tokens"),
                      "cost": usage.get("cost")}) + "\n"
    with _log_lock, open(LOG, "a", encoding="utf-8") as f:
        f.write(rec)
    return text


def spent():
    """Total logged cost. Tolerates lines interleaved by the parallel run before the log had a lock."""
    import re
    total = 0.0
    if os.path.exists(LOG):
        for m in re.finditer(r'"cost":\s*([0-9.eE+-]+)', open(LOG, encoding="utf-8").read()):
            total += float(m.group(1))
    return total


if __name__ == "__main__" and sys.argv[1:] == ["ping"]:
    out = chat("Reply with exactly one short word.", "Say hello.", temperature=0, max_tokens=10, tag="ping")
    print(f"reply: {out!r}; total spent so far ${spent():.5f}")
