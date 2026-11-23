from prev import *
import math
def paint(ph):
    Blue=hexc(0x2a4fb0);BlueD=hexc(0x1e3a8c);Bright=hexc(0x4f8ff0);Yellow=hexc(0xf6d21a);YellowD=hexc(0xe0b810)
    def f(At):
        p=At['Part'];U=At['U'];V=At['V'];j=ph==1
        if p=='Body':
            if j: return YellowD if V>.3 else Yellow
            C=BlueD if (At['Lod']==0 and (At['J']&1)) else Blue
            if U>.9 and abs(V)<.45: C=Yellow
            return C
        if p in('Dorsal','Anal'): return Bright if V>.6 else (Yellow if j else Blue)
        if p=='Tail': return Yellow if j else (Bright if U>.8 else Blue)
        if p=='Pectoral': return hexc(0xf2dc6a) if j else hexc(0x6f8fd0)
        if p=='Eye': return np.array([.004]*3) if U<.6 else (Bright if j else hexc(0xd9c75a))
        return Yellow if j else Blue
    return f
B=Body();B.HalfH=.25;B.HalfW=.05;B.Peak=.4;B.Blunt=.6;B.NoseR=.16;B.Ped=.2;B.PedW=.25;B.Arch=.04;B.TailLen=.16;B.TailH=.34;B.TailFork=.25
B.Dor0=.2;B.Dor1=.93;B.DorH=.08;B.An0=.45;B.An1=.93;B.AnH=.07;B.PecU=.3;B.PecV=-.05;B.PecLen=.17;B.PecW=.08;B.PelU=.34;B.PelLen=.08;B.EyeU=.13;B.EyeV=.5;B.EyeR=.022
out=Image.new('RGB',(1400,700))
for ph in (0,1):
    for lod in (0,1): out.paste(render([build(B,lod,True,paint(ph))],W=700),(700*lod,350*ph))
out.save('tang.png')
