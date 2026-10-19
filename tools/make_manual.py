#!/usr/bin/env python3
"""Builds the PDF manual of Hairball: docs/manual.txt (intro, quick start, recommendations, install) + the interface tour from
docs/tour (made by tools/shoot_tour.js) + the factory presets read from src/Presets.h.
Usage: python3 tools/make_manual.py <unused> docs/manual.txt <out.pdf>      Needs: pip install reportlab"""
import json, os, re, sys
from reportlab.lib import colors
from reportlab.lib.pagesizes import A4
from reportlab.lib.styles import ParagraphStyle
from reportlab.lib.units import mm
from reportlab.lib.utils import ImageReader
from reportlab.platypus import SimpleDocTemplate, Paragraph, Spacer, Table, TableStyle, Image, PageBreak, CondPageBreak, KeepTogether

_, txt_path, out = sys.argv[1:4]
root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
tdir = os.path.join(root, "docs", "tour"); name = "Hairball"
BG, PANEL = colors.HexColor("#14161c"), colors.HexColor("#242833")
PINK, TEAL, INK, DIM, AMBER = colors.HexColor("#ff1383"), colors.HexColor("#0aa38a"), colors.HexColor("#e6e8ef"), colors.HexColor("#9aa0b4"), colors.HexColor("#ffb020")
st = lambda n, **k: ParagraphStyle(n, fontName=k.pop("font", "Helvetica"), **k)
H1 = st("h1", fontSize=26, leading=30, textColor=PINK, font="Helvetica-Bold")
H2 = st("h2", fontSize=14, leading=18, textColor=AMBER, font="Helvetica-Bold", spaceBefore=10, spaceAfter=4)
H3 = st("h3", fontSize=11.5, leading=15, textColor=TEAL, font="Helvetica-Bold", spaceBefore=6, spaceAfter=3)
B = st("b", fontSize=10, leading=14, textColor=INK)
SM = st("s", fontSize=8.5, leading=11.5, textColor=DIM)
CELL = st("c", fontSize=9, leading=12, textColor=INK)
CELLB = st("cb", fontSize=9, leading=12, textColor=PINK, font="Helvetica-Bold")
esc = lambda t: t.replace("&", "&amp;").replace("<", "&lt;").replace(">", "&gt;")

sec = {}; cur = None
for line in open(txt_path, encoding="utf-8").read().splitlines():
    if line.startswith("# "): cur = line[2:].strip(); sec[cur] = []
    elif cur is not None: sec[cur].append(line)
para = lambda k: [p.strip() for p in "\n".join(sec.get(k, [])).split("\n\n") if p.strip()]
bullets = lambda k: [l[2:].strip() for l in sec.get(k, []) if l.startswith("- ")]
presets = re.findall(r'\{\s*"([^"]+)",\s*"([^"]+)",\s*\{', open(os.path.join(root, "src", "Presets.h"), encoding="utf-8").read())

def img(path, width, maxh=None):
    w, h = ImageReader(path).getSize()
    if maxh and width * h / w > maxh: width = maxh * w / h
    return Image(path, width=width, height=width * h / w)
def tbl(rows):
    t = Table(rows, colWidths=[10 * mm, 34 * mm, 126 * mm]); t.setStyle(TableStyle([("VALIGN", (0, 0), (-1, -1), "TOP"), ("BACKGROUND", (0, 0), (-1, -1), PANEL), ("LINEBELOW", (0, 0), (-1, -1), 0.5, BG), ("TOPPADDING", (0, 0), (-1, -1), 4), ("BOTTOMPADDING", (0, 0), (-1, -1), 4)])); return t

d = json.load(open(os.path.join(tdir, "hairball.json"))); X = json.load(open(os.path.join(tdir, "extra.json")))["hairball"]
story = [Paragraph(name, H1), Paragraph("Manual . free and open source (AGPL-3.0-or-later) . github.com/SpeakingColors/hairball . by Studio 87", SM), Spacer(1, 6)]
for p in para("Intro"): story += [Paragraph(esc(p), B), Spacer(1, 4)]
story.append(Paragraph("Quick start", H2))
for t in bullets("Quick start"): story.append(Paragraph("&bull; " + esc(t), B))

# ---- interface tour
story += [CondPageBreak(120 * mm), Paragraph("Interface tour", H2),
  Paragraph(esc("Pink numbers mark controls, teal letters mark displays. Every number is explained in the tables on the next pages. The window has three tabs; one picture per tab follows. The pictures come from the interactive test page of the plugin window; the compiled plugin may differ by a few pixels of layout."), B), Spacer(1, 4)]
for f in d["fulls"]:
    story += [KeepTogether([Paragraph(esc(f["label"]), H3), img(os.path.join(tdir, f["img"]), 150 * mm, 112 * mm), Spacer(1, 4)])]
story += [Paragraph("Signal flow", H3), Paragraph(esc(X["flow"]), B)]
story += [CondPageBreak(60 * mm), Paragraph("Header bar", H3), tbl([[Paragraph(str(t["n"]), CELLB), Paragraph(esc(t["name"]), CELLB), Paragraph(esc(t["tip"]), CELL)] for t in d["top"]])]
for P in d["items"]:
    letters = [x["letter"] for x in P.get("disps", []) if x.get("letter")]
    head = esc(P["title"] + (" (display " + ", ".join(letters) + ")" if letters else "") + ("  [%s]" % dict(grains="Grains page", riser="Riser page", thicken="Thicken page").get(P["tab"], "") if P["tab"] != "grains" or P["title"] not in ("Source, display and keys", "Output") else ""))
    blk = [CondPageBreak(110 * mm), Paragraph(head, H2), img(os.path.join(tdir, P["img"]), 170 * mm if P.get("wide") else 120 * mm, 62 * mm if not P.get("wide") else 120 * mm), Spacer(1, 4)]
    for x in P.get("disps", []):
        if x.get("letter"): blk += [Paragraph("<b>Display %s:</b> %s" % (x["letter"], esc(X["displays"][x["letter"]])), B), Spacer(1, 4)]
    rows = []
    for it in P["items"]:
        nm = X.get("names", {}).get(str(it["n"]), it["name"]); val = (" (shown: %s)" % it["value"]) if it["value"] else ""
        rows.append([Paragraph(str(it["n"]), CELLB), Paragraph(esc(nm), CELLB), Paragraph(esc(it["tip"]) + "<font color='#8a90a8'>" + esc(val) + "</font>", CELL)])
    if rows: blk.append(tbl(rows))
    story += blk
tips = bullets("Recommendations")
if tips:
    story += [PageBreak(), Paragraph("Recommendations", H2)]
    for t in tips: story.append(Paragraph("&bull; " + esc(t), B))
if presets:
    story.append(Paragraph("Factory presets", H2)); story.append(Paragraph(esc(", ".join("%s (%s)" % p for p in presets)), B))
story += [Paragraph("Under the hood", H2)] + [Paragraph("&bull; " + esc(t), B) for t in X["deep"]]
story.append(Paragraph("Install, remove and your saved presets", H2))
for t in bullets("Install"): story.append(Paragraph("&bull; " + esc(t), B))

def footer(c, doc):
    c.saveState(); c.setFillColor(BG); c.rect(0, 0, A4[0], A4[1], stroke=0, fill=1)
    c.setFillColor(PANEL); c.rect(0, A4[1] - 5 * mm, A4[0], 5 * mm, stroke=0, fill=1); c.setFillColor(PINK); c.rect(0, A4[1] - 5.8 * mm, A4[0], 0.8 * mm, stroke=0, fill=1)
    c.setFont("Helvetica", 8); c.setFillColor(DIM); c.drawString(20 * mm, 10 * mm, name + " manual"); c.drawRightString(190 * mm, 10 * mm, "page %d" % doc.page); c.restoreState()
SimpleDocTemplate(out, pagesize=A4, leftMargin=20 * mm, rightMargin=20 * mm, topMargin=16 * mm, bottomMargin=18 * mm, title=name + " manual", author="Studio 87").build(story, onFirstPage=footer, onLaterPages=footer)
print("wrote", out, "presets", len(presets))
