#!/usr/bin/env python3
"""Embed build/web/nekoarcade.wasm (base64) into web/page.html, two ways:
  build/web/neko-arcade.html            standalone page (open it straight from disk)
  build/web/artifact/neko-arcade.html   the same page without the document skeleton, for publishing"""
import base64, os, time

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
wasm = open(os.path.join(ROOT, "build", "web", "nekoarcade.wasm"), "rb").read()
page = open(os.path.join(ROOT, "web", "page.html"), encoding="utf-8").read()
if "__WASM_B64__" not in page or "<main>" not in page:
    raise SystemExit("mkweb: placeholder or <main> missing in web/page.html")
page = page.replace("__WASM_B64__", base64.b64encode(wasm).decode())
page = page.replace("__BUILD__", time.strftime("%Y-%m-%d %H:%M"))
head, body = page.split("<main>", 1)
standalone = ('<!doctype html>\n<html lang="en">\n<head>\n<meta charset="utf-8">\n'
              '<meta name="viewport" content="width=device-width, initial-scale=1, viewport-fit=cover">\n'
              + head + "</head>\n<body>\n<main>" + body + "</body>\n</html>\n")
os.makedirs(os.path.join(ROOT, "build", "web", "artifact"), exist_ok=True)
open(os.path.join(ROOT, "build", "web", "neko-arcade.html"), "w", encoding="utf-8", newline="\n").write(standalone)
open(os.path.join(ROOT, "build", "web", "artifact", "neko-arcade.html"), "w", encoding="utf-8", newline="\n").write(page)
print(f"mkweb: build/web/neko-arcade.html ({len(standalone) // 1024} KB) + artifact/neko-arcade.html")
