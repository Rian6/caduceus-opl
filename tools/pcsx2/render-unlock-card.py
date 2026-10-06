"""Rasterize rectangles exported by test-ra-overlay.py; no invented UI pixels."""
from pathlib import Path
import sys
from PIL import Image
root=Path(__file__).resolve().parents[2]
prefix='ra-static-card' if '--static' in sys.argv else 'ra-unlock-card'
rects=[]
for line in (root/f'pcsx2-test/{prefix}-primitives.txt').read_text().splitlines():
    if line.startswith('RECT '):
        _,x,y,x1,y1,c=line.split();rects.append((int(x),int(y),int(x1),int(y1),int(c,16)))
x0=min(r[0] for r in rects);y0=min(r[1] for r in rects)
x1=max(r[2] for r in rects);y1=max(r[3] for r in rects)
card=Image.new('RGB',(x1-x0,y1-y0))
for x,y,x2,y2,c in rects:
    card.paste((c&255,(c>>8)&255,(c>>16)&255),(x-x0,y-y0,x2-x0,y2-y0))
card.resize((card.width*3,card.height*3),Image.Resampling.NEAREST).save(root/f'pcsx2-test/{prefix}-detail.png')
scene=Image.new('RGB',(640,448),(25,30,37));scene.paste(card,(x0,y0))
scene.save(root/f'pcsx2-test/{prefix}-example.png')
print('Generated preview from actual GS rectangles')
