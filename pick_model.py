#!/usr/bin/env python3
"""List available Gemini models, pick one, test all 3 nodes, and update orchestrator-config.yaml."""
import json, os, urllib.request
from pathlib import Path
import litellm, yaml

litellm.suppress_debug_info = True

req = urllib.request.Request(
    "https://generativelanguage.googleapis.com/v1beta/models?pageSize=100",
    headers={"x-goog-api-key": os.environ["GEMINI_API_KEY"]},
)
with urllib.request.urlopen(req, timeout=10) as resp:
    data = json.load(resp)

models = [
    m["name"].removeprefix("models/")
    for m in data.get("models", [])
    if "generateContent" in m.get("supportedGenerationMethods", [])
]

for i, name in enumerate(models, 1):
    print(f"{i:2d}. {name}")

chosen = f"gemini/{models[int(input('\nPick model number: ')) - 1]}"
print(f"\nTesting {chosen} + PC + MAC...\n")

tests = [
    ("PC ", f"ollama_chat/{os.environ['PC_OLLAMA_MODEL']}", os.environ["PC_OLLAMA_URL"]),
    ("MAC", f"ollama_chat/{os.environ['MAC_OLLAMA_MODEL']}", os.environ["MAC_OLLAMA_URL"]),
    ("GEM", chosen, None),
]

for tag, model, base in tests:
    kw = {"model": model, "messages": [{"role": "user", "content": "Reply OK"}], "max_tokens": 256, "num_retries": 2}
    if base:
        kw["api_base"] = base
        kw["extra_body"] = {"think": False}
    try:
        r = litellm.completion(**kw)
        text = (r.choices[0].message.content or "").strip()
        print(f"[{tag}] {r.model} -> {text or '(empty content)'}")
    except Exception as e:
        print(f"[{tag}] {model} FAILED -> {type(e).__name__}: {e}")

cfg_path = Path("orchestrator-config.yaml")
if cfg_path.exists() and input(f"\nSave '{chosen}' to orchestrator-config.yaml? [y/N]: ").lower() == "y":
    cfg = yaml.safe_load(cfg_path.read_text())
    tiers = cfg.get("model_tiers", {})
    if "tier_cloud_engineer" in tiers:
        tiers["tier_cloud_engineer"]["primary"]["model"] = chosen
        cfg_path.write_text(yaml.safe_dump(cfg, sort_keys=False))
        print(f"[OK] Updated tier_cloud_engineer -> {chosen}")
