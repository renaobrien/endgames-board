# Endgames Board: flat template to check the 4 corner holes, the USB-C port the case uses, and the power slider slot.
# Lay the board SCREEN DOWN on it, USB-C edge against the short wall,
# UART1/I2C edge on the side with the notch.
# Geometry below uses the case's screen-up numbers, then the frame is mirrored across its width,
# because laying the board screen down flips it. (First print skipped this and came out backwards.)
import trimesh
from trimesh.creation import box as _box, cylinder as _cyl
def box(x0,y0,z0,x1,y1,z1):
    b=_box((x1-x0,y1-y0,z1-z0)); b.apply_translation(((x0+x1)/2,(y0+y1)/2,(z0+z1)/2)); return b
def cyl(x,y,z0,z1,d): c=_cyl(radius=d/2,height=z1-z0,sections=40); c.apply_translation((x,y,(z0+z1)/2)); return c
U=lambda ms: trimesh.boolean.union(ms); D=lambda a,b: trimesh.boolean.difference([a,b])
L,W=132.0,80.12; T=1.6; BORDER=9; CLR=1.6; WALL=1.6   # board size from Elecrow's Eagle file; CLR covers the ~1 mm USB-C overhang
# Screen down: x=0 is the USB-C edge, y measured from the UART1/I2C edge (same numbers as the case).
plate=box(0,0,0,L,W,T)
plate=D(plate, box(BORDER,BORDER,-1,L-BORDER,W-BORDER,T+1))
wall=box(-CLR-WALL,0,0,-CLR,W,15.5)                      # short wall along the USB-C edge
base=box(-CLR-WALL,0,0,0.01,W,T)                       # joins wall to plate
t=U([plate,wall,base])
cuts=[cyl(x,y,-1,T+1,3.4) for x in (3.0,L-3.0) for y in (2.82,W-3.0)]   # Eagle hole centers, M3 clearance
# Screen down: glass+PCB = 5.1 mm, USB-C receptacle (3.3 mm tall) sits on top, center about 6.8 mm up.
for y in (48.57,):                                   # only the USB-C the case uses (USB2.0)
    cuts.append(box(-CLR-WALL-1,y-5.0,T+6.9-2.2,-CLR+1,y+5.0,T+6.9+2.2))   # board rests on the plate, so + T   # 10 x 4.4 mm window, real port is about 9 x 3.3
cuts.append(box(L/2-4,-1,-1,L/2+4,3,T+1))          # notch marks the UART1/I2C edge
# Power slider slot, same as the case: extender arm runs 3.7 to 6.1 mm off the board's back,
# back is 5.1 mm above the plate when screen down. Slot length = arm width 4 + max travel 6.1.
cuts.append(box(-CLR-WALL-1,18.62-(4+7.7)/2-0.4,T+5.1+3.73-0.4,-CLR+1,18.62+(4+7.7)/2+0.4,T+5.1+6.13+0.4))
t=D(t,U(cuts))
import numpy as np
m=np.eye(4); m[1,1]=-1; m[1,3]=W                         # mirror y -> W-y (screen down)
t.apply_transform(m)
if t.volume<0: t.invert()
print(t.is_watertight,[round(v,1) for v in t.extents], round(t.volume/1000,1),'cm3')
# Slider extender printed alongside, in the open middle of the frame (socket facing up).
# Same part as case/power-slider-extender.stl, but its arm only needs to reach past this template's wall.
EXT_W=4.0; SW_X=132.0-127.2; arm_len=SW_X+CLR+WALL+2.5
ext=U([box(-arm_len,-EXT_W/2,0,2.0,EXT_W/2,1.6), box(-2.0,-2.0,0,2.0,2.0,2.5), box(-arm_len,-EXT_W/2,0,-arm_len+1.6,EXT_W/2,4.0)])
ext=D(ext,box(-0.85,-0.85,1.0,0.85,0.85,3.6))
ext.apply_translation((L/2, W/2, 0))
t=trimesh.util.concatenate([t,ext])
t.export('template.stl')
