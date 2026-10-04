import trimesh, numpy as np
from trimesh.creation import box as _box, cylinder as _cyl
def box(x0,y0,z0,x1,y1,z1):
    b=_box((x1-x0,y1-y0,z1-z0)); b.apply_translation(((x0+x1)/2,(y0+y1)/2,(z0+z1)/2)); return b
def cyl(x,y,z0,z1,d,fn=48):
    c=_cyl(radius=d/2,height=z1-z0,sections=fn); c.apply_translation((x,y,(z0+z1)/2)); return c
U=lambda ms: trimesh.boolean.union(ms)
D=lambda a,b: trimesh.boolean.difference([a,b])

# Board (top view, screen up). x along 131 mm edge, x=0 = USB-C edge. y=0 = UART1/I2C edge, y=W = microSD edge.
L,W=131.0,80.2
HOLE_IN=3.5
WALL,CLR=2.0,0.4
C=WALL+CLR                      # board offset inside the case
OX,OY=L+2*C,W+2*C               # outer footprint
FLOOR=2.0
BACK_DEPTH=16.5-5.1             # tallest part below the PCB (measured 0.65" total minus 0.2" board+screen)
PCB_BOT=FLOOR+BACK_DEPTH+1.5    # 1.5 mm clearance under the tallest part
PCB_T=1.6
PCB_TOP=PCB_BOT+PCB_T
GLASS_TOP=PCB_BOT+5.1
LID_Z=GLASS_TOP+0.2
LID_T=2.0
TOP=LID_Z+LID_T
holes=[(HOLE_IN,HOLE_IN),(L-HOLE_IN,HOLE_IN),(HOLE_IN,W-HOLE_IN),(L-HOLE_IN,W-HOLE_IN)]
# Lit area from caliper borders: glass 119.4 x 75.8 centered on PCB; borders 0.2" sides, 0.1" UART1 edge, 0.3" microSD edge.
gx=(L-119.4)/2; gy=(W-75.8)/2
win=(gx+5.08+0.3, gy+2.54+0.3, L-gx-5.08-0.3, W-gy-7.62-0.3)

# ---------- tray ----------
tray=box(0,0,0,OX,OY,LID_Z)
tray=D(tray, box(WALL,WALL,FLOOR,OX-WALL,OY-WALL,LID_Z+1))
bosses=[cyl(C+x,C+y,FLOOR-0.01,PCB_BOT,6.5) for x,y in holes]
tray=U([tray]+bosses)
cuts=[]
for x,y in holes:                       # M2.5 screw from below: clearance + head counterbore
    cuts.append(cyl(C+x,C+y,-1,PCB_BOT+1,2.9))
    cuts.append(cyl(C+x,C+y,-1,2.6,5.2))
z_conn=PCB_BOT-1.7                       # connectors hang under the PCB
for y in (30.5,47.8):                    # USB-C UART0 (flash) and USB2.0, generous for cable boots
    cuts.append(box(-1,C+y-6.5,z_conn-4,WALL+1,C+y+6.5,z_conn+4))
cuts.append(box(-1,C+17-5.5,z_conn-3.5,WALL+1,C+17+5.5,PCB_BOT+0.5))   # power switch
cuts.append(box(C+114.3-8,OY-WALL-1,z_conn-2.5,C+114.3+8,OY+1,PCB_BOT+0.5))  # microSD
for y in (48,55):                        # BOOT / RESET access from the back
    cuts.append(cyl(C+127.1,C+y,-1,FLOOR+1,4.5))
tray=D(tray,U(cuts))

# ---------- lid (front frame) ----------
lid=box(0,0,LID_Z,OX,OY,TOP)
lip=box(WALL+0.25,WALL+0.25,LID_Z-1.5,OX-WALL-0.25,OY-WALL-0.25,LID_Z+0.01)   # locating lip inside the walls
lip=D(lip,box(WALL+1.85,WALL+1.85,LID_Z-2,OX-WALL-1.85,OY-WALL-1.85,LID_Z+1))
lb=[cyl(C+x,C+y,PCB_TOP,LID_Z+0.01,6.0) for x,y in holes]
lid=U([lid,lip]+lb)
lid=D(lid,U([box(C+win[0],C+win[1],LID_Z-3,C+win[2],C+win[3],TOP+1)]+[cyl(C+x,C+y,PCB_TOP-1,LID_Z+0.5,2.2) for x,y in holes]))

for name,m in (('case-tray',tray),('case-lid',lid)):
    print(name, m.is_watertight, [round(v,1) for v in m.extents])
    m.export(f'{name}.stl')
print('outer', round(OX,1), round(OY,1), round(TOP,1), 'window', [round(v,1) for v in win], 'screw M2.5 x', round(FLOOR+ (PCB_BOT-FLOOR)+PCB_T+3,1))

# ---------- test-fit tray: same walls, posts and openings, floor cut out to print fast ----------
RING=7.0
test=D(tray, box(C+RING+4, C+RING, -1, C+L-RING-4, C+W-RING, FLOOR+0.01))
print('test-fit tray', test.is_watertight, round(test.volume/1000,1), 'cm3 vs full', round(tray.volume/1000,1))
test.export('test-fit-tray.stl')
