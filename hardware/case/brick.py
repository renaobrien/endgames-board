import trimesh, numpy as np
from trimesh.creation import box as _box, cylinder as _cyl
def box(x0,y0,z0,x1,y1,z1):
    b=_box((x1-x0,y1-y0,z1-z0)); b.apply_translation(((x0+x1)/2,(y0+y1)/2,(z0+z1)/2)); return b
def cyl(x,y,z0,z1,d,fn=48):
    c=_cyl(radius=d/2,height=z1-z0,sections=fn); c.apply_translation((x,y,(z0+z1)/2)); return c
U=lambda ms: trimesh.boolean.union(ms)
D=lambda a,b: trimesh.boolean.difference([a,b])

# Board (top view, screen up). x along 131 mm edge, x=0 = USB-C edge. y=0 = UART1/I2C edge, y=W = microSD edge.
L,W=132.0,80.12   # Elecrow Eagle board file (ESP32-P4 Display 5.0 inch V1.0.brd)
HOLE_IN=3.5
WALL,CLR=2.0,0.4
C=WALL+CLR                      # board offset inside the case
USB_EXTRA=1.2                   # USB-C shells overhang the board edge ~1 mm (side photo)
CX=C+USB_EXTRA                  # board offset in x (USB-C edge side)
OX,OY=L+2*C+USB_EXTRA,W+2*C     # outer footprint
FLOOR=2.0
BACK_DEPTH=16.5-5.1             # tallest part below the PCB (measured 0.65" total minus 0.2" board+screen)
PCB_BOT=FLOOR+BACK_DEPTH+1.5    # 1.5 mm clearance under the tallest part
PCB_T=1.6
PCB_TOP=PCB_BOT+PCB_T
GLASS_TOP=PCB_BOT+5.1
LID_Z=GLASS_TOP+0.2
LID_T=2.0
TOP=LID_Z+LID_T
# Hole centers from the Eagle file: 3.0 mm from the short edges, 2.82 mm from the UART1/I2C edge, 3.0 mm from the microSD edge. Drill 3.2 mm (M3).
# Caliper 2026-10-04 overrides Eagle: 127.0 x 73.66 mm center to center, 3.33 mm from the UART1/I2C edge.
holes=[(2.5,3.33),(L-2.5,3.33),(2.5,W-3.13),(L-2.5,W-3.13)]
# Lit area from caliper borders: glass 119.4 x 75.8 centered on PCB; borders 0.2" sides, 0.1" UART1 edge, 0.3" microSD edge.
gx=(L-119.4)/2; gy=(W-75.8)/2
win=(gx+5.08+0.3, gy+2.54+0.3, L-gx-5.08-0.3, W-gy-7.62-0.3)

# ---------- tray ----------
tray=box(0,0,0,OX,OY,LID_Z)
tray=D(tray, box(WALL,WALL,FLOOR,OX-WALL,OY-WALL,LID_Z+1))
bosses=[cyl(CX+x,C+y,FLOOR-0.01,PCB_BOT,7.0) for x,y in holes]
tray=U([tray]+bosses)
cuts=[]
for x,y in holes:                       # M3 screw from below: 3.5 clearance (prints ~3.3) + head counterbore. Board holes measured 3.39 mm.
    cuts.append(cyl(CX+x,C+y,-1,PCB_BOT+1,3.5))
    cuts.append(cyl(CX+x,C+y,-1,2.6,6.0))
z_conn=PCB_BOT-1.8                       # connectors hang under the PCB
for y in (48.57,):                        # only the lower USB-C (USB2.0, power). UART0 stays closed: it sits next to the power slider and is only needed for flashing, done with the case open
    cuts.append(box(-1,C+y-6.5,z_conn-4,WALL+1,C+y+6.5,z_conn+4))
# Power slider (caliper): body 7.62 x 3.68 x 5.13 mm, 3.49 mm in from the USB-C edge, nub 1.5 mm square,
# nub tip 7.07 mm below the PCB. Slides along the edge. Extender clips on the nub, tab exits a wall slot.
SW_Y=18.62                                 # switch center along the edge (Eagle SW1)
SW_X=132.0-127.2                           # nub center from the board edge (Eagle SW1)
NUB_TIP=PCB_BOT-5.13                       # side photo: 0.202" is board to nub tip
SOCK_DEPTH=1.5
EXT_Z0,EXT_Z1=NUB_TIP-1.0, NUB_TIP-1.0+2.4    # socket bottom 1 mm past the nub tip # extender arm height band
TRAVEL_MAX=9.2-1.5   # Eagle footprint body length 9.2 mm                        # most the nub can move inside its body
EXT_W=4.0
cuts.append(box(-1,C+SW_Y-(EXT_W+TRAVEL_MAX)/2-0.4,EXT_Z0-0.4,WALL+1,C+SW_Y+(EXT_W+TRAVEL_MAX)/2+0.4,EXT_Z1+0.4))   # power slider slot
cuts.append(box(CX+114.3-8,OY-WALL-1,z_conn-2.5,CX+114.3+8,OY+1,PCB_BOT+0.5))  # microSD
for y in (46.48,52.98):                        # BOOT / RESET access from the back
    cuts.append(cyl(CX+127.79,C+y,-1,FLOOR+1,4.5))
tray=D(tray,U(cuts))

# ---------- lid (front frame) ----------
lid=box(0,0,LID_Z,OX,OY,TOP)
lip=box(WALL+0.25,WALL+0.25,LID_Z-1.5,OX-WALL-0.25,OY-WALL-0.25,LID_Z+0.01)   # locating lip inside the walls
lip=D(lip,box(WALL+1.85,WALL+1.85,LID_Z-2,OX-WALL-1.85,OY-WALL-1.85,LID_Z+1))
lb=[cyl(CX+x,C+y,PCB_TOP,LID_Z+0.01,6.0) for x,y in holes]
lid=U([lid,lip]+lb)
lid=D(lid,U([box(CX+win[0],C+win[1],LID_Z-3,CX+win[2],C+win[3],TOP+1)]+[cyl(CX+x,C+y,PCB_TOP-1,LID_Z+0.5,2.6) for x,y in holes]))   # 2.6 = M3 pilot, the screw cuts its own thread in the lid post

for name,m in (('case-tray',tray),('case-lid',lid)):
    print(name, m.is_watertight, [round(v,1) for v in m.extents])
    m.export(f'{name}.stl')
print('outer', round(OX,1), round(OY,1), round(TOP,1), 'window', [round(v,1) for v in win], 'screw M3 x', round(FLOOR+ (PCB_BOT-FLOOR)+PCB_T+3,1))

# ---------- test-fit tray: same walls, posts and openings, floor cut out to print fast ----------
RING=7.0
test=D(tray, box(CX+RING+4, C+RING, -1, CX+L-RING-4, C+W-RING, FLOOR+0.01))
print('test-fit tray', test.is_watertight, round(test.volume/1000,1), 'cm3 vs full', round(tray.volume/1000,1))
test.export('test-fit-tray.stl')


# ---------- power slider extender ----------
# Printed with the socket opening facing up. Push the socket onto the nub; the tab sticks out the side of the case.
arm_len=SW_X+CLR+USB_EXTRA+WALL+2.5                  # nub center to 2.5 mm past the outside of the case
ext=box(-arm_len,-EXT_W/2,0,2.0,EXT_W/2,1.6)                   # arm
ext=U([ext,box(-2.0,-2.0,0,2.0,2.0,1.0+SOCK_DEPTH),            # socket block around the nub
       box(-arm_len,-EXT_W/2,0,-arm_len+1.6,EXT_W/2,4.0)])     # grip ridge on the tab
ext=D(ext,box(-0.85,-0.85,1.0,0.85,0.85,1.0+SOCK_DEPTH+1))     # 1.7 mm square socket for the 1.5 mm nub
print('extender', ext.is_watertight, [round(v,1) for v in ext.extents])
ext.export('power-slider-extender.stl')
