#!/usr/bin/env python3
"""Regression tests for ansi2ly's scaler.

Guards the two bugs that produced visible artefacts:
  * colour: re-deriving fg/bg per cell invented pairs at fractional scales
  * glyphs: emitting half-blocks a console font lacks shows up as '#'

Run: ./verify_scaler.py [art.ANS]
"""
import importlib.machinery
import importlib.util
import sys

HERE = '/home/dylan/.dotfiles/etc/ly/'
TOOL = '/home/dylan/.dotfiles/.local/bin/ansi2ly'
# ansi2ly has no .py suffix (it is a command on PATH), so the loader must be
# named explicitly - spec_from_file_location cannot infer one from the name.
spec = importlib.util.spec_from_file_location(
    'ansi2ly', TOOL, loader=importlib.machinery.SourceFileLoader('ansi2ly', TOOL))
m = importlib.util.module_from_spec(spec)
spec.loader.exec_module(m)

art = sys.argv[1] if len(sys.argv) > 1 else HERE + '2TOONS.ANS'
cells, w, h = m.render_ans(open(art, 'rb').read())
source_pairs = {(c[1], c[2]) for row in cells for c in row}
print(f'source: {art}  {w}x{h}, {len(source_pairs)} distinct (fg,bg) pairs')
print(f'  bright foregrounds: {sorted({f for f, b in source_pairs if f >= 8})}')
print(f'  backgrounds:        {sorted({b for f, b in source_pairs})}\n')

fails = 0
SCALES = [('2x', w * 2, h * 2), ('3x', w * 3, h * 3), ('fit 256x71', 256, 71),
          ('stretch 256x80', 256, 80), ('odd 250x67', 250, 67), ('320x100', 320, 100)]

# [1] scale 1 must reproduce the source exactly
out = m.scale_art(cells, w, h, w, h)
bad = [(x, y) for y in range(h) for x in range(w) if out[y][x] != cells[y][x]]
print(f'[1] scale-1 identity: {len(bad)}/{w*h} differ', '- PASS' if not bad else '- FAIL')
fails += bool(bad)

# [2] no invented colour pairs, no bright background
print('\n[2] colour integrity:')
for label, tw, th in SCALES:
    o = m.scale_art(cells, w, h, tw, th)
    inv = {(c[1], c[2]) for row in o for c in row} - source_pairs
    badbg = {c[2] for row in o for c in row if c[2] >= 8}
    ok = not inv and not badbg
    print(f'      {label:16}: invented={len(inv)} invalid_bg={sorted(badbg)}',
          '- PASS' if ok else '- FAIL')
    fails += not ok

# [3] colour provenance: colours come from a cell that contributed subpixels
print('\n[3] colour provenance (text exempt, redrawn at 1x):')
for label, tw, th in SCALES[2:4]:
    o = m.scale_art(cells, w, h, tw, th)
    cov, src, sw, sh = m.to_subpixels(cells, w, h)
    sr = m.resample(src, sw, sh, tw * 2, th * 2)
    bad_n = txt = 0
    for y in range(th):
        for x in range(tw):
            ch = o[y][x][0]
            if ch != ' ' and ch not in m.BLOCKS:
                txt += 1
                continue
            yy, xx = y * 2, x * 2
            contrib = {sr[yy][xx], sr[yy][xx + 1], sr[yy + 1][xx], sr[yy + 1][xx + 1]}
            allowed = {(cells[i // w][i % w][1], cells[i // w][i % w][2]) for i in contrib}
            if (o[y][x][1], o[y][x][2]) not in allowed:
                bad_n += 1
    print(f'      {label:16}: {bad_n}/{tw*th - txt} bad ({txt} text exempt)',
          '- PASS' if not bad_n else '- FAIL')
    fails += bool(bad_n)

# [4] never emit a glyph the console font cannot draw
print('\n[4] glyph safety per console font:')
for font in ('ter-120n', 'ter-c24n', 'ter-u20n', 'ter-u24n'):
    allowed = m.font_glyphs(font)
    if not allowed:
        print(f'      {font:12}: no unicode table - skipped')
        continue
    o = m.scale_art(cells, w, h, 256, 71, allowed)
    unren = {c[0] for row in o for c in row if c[0] != ' ' and c[0] not in allowed}
    inv = {(c[1], c[2]) for row in o for c in row} - source_pairs
    ok = not unren and not inv
    lacks = sorted(set(m.BLOCKS) - allowed)
    print(f'      {font:12}: font lacks {"".join(lacks) or "nothing":6} '
          f'-> unrenderable emitted={sorted(unren) or "none"} invented={len(inv)}',
          '- PASS' if ok else '- FAIL')
    fails += not ok

print('\nRESULT:', 'ALL PASS' if not fails else f'{fails} FAILURE(S)')
sys.exit(1 if fails else 0)
