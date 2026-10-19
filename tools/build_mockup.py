#!/usr/bin/env python3
"""Builds docs/mockups/Hairball.html (full page) and docs/mockups/Hairball.fragment.html (for the artifact tool)
from hairball.src.html + engine.js + params.json. Run after changing any of them."""
import json, pathlib
d = pathlib.Path(__file__).resolve().parent.parent / "docs" / "mockups"
src = (d / "hairball.src.html").read_text()
eng = (d / "engine.js").read_text().replace("</script", "<\\/script")
params = json.dumps(json.loads((d / "params.json").read_text()), separators=(",", ":"))
frag = src.replace("/*ENGINE*/", eng, 1).replace("/*PARAMS*/", params, 1)
(d / "Hairball.fragment.html").write_text(frag)
head = '<!doctype html>\n<html lang="en"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1,viewport-fit=cover"></head><body>\n'
(d / "Hairball.html").write_text(head + frag + "\n</body></html>\n")
print("built", len(frag) // 1024, "KB")
