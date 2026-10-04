# Endgames Board: flat template to check the 4 corner holes and the 2 USB-C ports.
# Lay the board SCREEN DOWN on it, USB-C edge against the short wall,
# UART1/I2C edge on the side with the notch.
import trimesh
from trimesh.creation import box as _box, cylinder as _cyl
def box(x0,y0,z0,x1,y1,z1):
    b=_box((x1-x0,y1-y0,z1-z0)); b.apply_translation(((x0+x1)/2,(y0+y1)/2,(z0+z1)/2)); return b
def cyl(x,y,z0,z1,d): c=_cyl(radius=d/2,height=z1-z0,sections=40); c.apply_translation((x,y,(z0+z1)/2)); return c
U=lambda ms: trimesh.boolean.union(ms); D=lambda a,b: trimesh.boolean.difference([a,b])
L,W=131.0,80.2; HOLE_IN=3.5; T=1.6; BORDER=9; CLR=1.6; WALL=1.6   # CLR covers the ~1 mm USB-C overhang
# Screen down: x=0 is the USB-C edge, y measured from the UART1/I2C edge (same numbers as the case).
plate=box(0,0,0,L,W,T)
plate=D(plate, box(BORDER,BORDER,-1,L-BORDER,W-BORDER,T+1))
wall=box(-CLR-WALL,0,0,-CLR,W,12)                      # short wall along the USB-C edge
base=box(-CLR-WALL,0,0,0.01,W,T)                       # joins wall to plate
t=U([plate,wall,base])
cuts=[cyl(x,y,-1,T+1,2.7) for x in (HOLE_IN,L-HOLE_IN) for y in (HOLE_IN,W-HOLE_IN)]
# Screen down: glass+PCB = 5.1 mm, USB-C receptacle (3.3 mm tall) sits on top, center about 6.8 mm up.
for y in (30.5,47.8):
    cuts.append(box(-CLR-WALL-1,y-5.0,6.9-2.2,-CLR+1,y+5.0,6.9+2.2))   # 10 x 4.4 mm window, real port is about 9 x 3.3
cuts.append(box(L/2-4,-1,-1,L/2+4,3,T+1))          # notch marks the UART1/I2C edge
cuts.append(box(-CLR-WALL-1,53,-1,-CLR+1,72,20))      # gap for the white 5V connector that overhangs the edge
t=D(t,U(cuts))
print(t.is_watertight,[round(v,1) for v in t.extents], round(t.volume/1000,1),'cm3')
t.export('template.stl')
