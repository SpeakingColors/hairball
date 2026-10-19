#!/usr/bin/env python3
"""Reads src/core/HairballParams.h and writes docs/mockups/params.json (single source of truth for the mockup and the manual)."""
import re, json, sys, os
root = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..')
src = open(os.path.join(root, 'src/core/HairballParams.h'), encoding='utf-8').read()
body = src.split('kParams[pCount] = {')[1].split('};')[0]
enum = re.search(r'enum P\s*\{(.*?)\};', src, re.S).group(1)
names = [n.strip() for n in re.sub(r'//.*', '', enum).replace('\n', ' ').split(',') if n.strip() and n.strip() != 'pCount']
rows = re.findall(r'\{\s*"([^"]+)",\s*"([^"]*)",\s*"([^"]*)",\s*(-?[\d.]+)f?,\s*(-?[\d.]+)f?,\s*(-?[\d.]+)f?,\s*"([^"]*)",\s*"((?:[^"\\]|\\.)*)",\s*(true|false),\s*"([^"]*)"\s*\}', body)
assert len(rows) == len(names), (len(rows), len(names))
out = []
for n, r in zip(names, rows):
    out.append(dict(key=n, id=r[0], name=r[1], group=r[2], lo=float(r[3]), hi=float(r[4]), def_=float(r[5]), unit=r[6], tip=r[7], choice=r[8] == 'true', choices=r[9].split('|') if r[9] else []))
path = sys.argv[1] if len(sys.argv) > 1 else os.path.join(root, 'docs/mockups/params.json')
os.makedirs(os.path.dirname(path), exist_ok=True)
json.dump(out, open(path, 'w'), indent=1)
print('wrote', path, len(out), 'parameters')
