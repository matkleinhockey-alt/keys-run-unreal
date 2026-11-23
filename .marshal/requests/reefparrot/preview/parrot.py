import numpy as np, math
from prev import *
from PIL import Image
def paint_initial(At):
    Scale=hexc(0x5b473d);Edge=hexc(0xe2dccf);Red=hexc(0xc9261c);Beak=hexc(0xece6d6);Brown=hexc(0x6e5546);FinRed=hexc(0xbf2a20);Pale=hexc(0xddd2c4)
    p=At['Part'];U=At['U'];V=At['V']
    if p=='Body':
        if U<.035: return Beak
        Flank=(Edge if ((At['I']+At['J'])&1) else Scale) if At['Lod']==0 else mix(Scale,Edge,.3)
        Upper=mix(Brown,Flank,(U-.08)/.12) if U<.2 else Flank
        C=mix(Red,Upper,(V+.35)/.3)
        if U>.93: C=mix(C,Pale,.7)
        return C
    if p=='Tail': return mix(Pale,FinRed,U/.22) if U<.22 else mix(FinRed,hexc(0x8e1d18),(U-.7)/.3)
    if p=='Pectoral': return mix(hexc(0xb9584a),hexc(0xd79c8c),U)
    if p=='Eye': return np.array([.004]*3) if U<.6 else hexc(0xb87a2e)
    return FinRed
def paint_terminal(At):
    Green=hexc(0x13a36b);GreenD=hexc(0x0f8459);Belly=hexc(0x4fcaa2);Pink=hexc(0xee6f8c);Yellow=hexc(0xf4d318);Beak=hexc(0xc4e6da);Teal=hexc(0x177f93);Orange=hexc(0xf0a629)
    p=At['Part'];U=At['U'];V=At['V']
    if p=='Body':
        if U<.035: return Beak
        C=(Green if ((At['I']+At['J'])&1) else GreenD) if At['Lod']==0 else mix(Green,GreenD,.5)
        C=mix(Belly,C,(V+.75)/.5)
        Tol=.02 if At['Lod']==0 else .045
        if abs(U-.25)<Tol and -.65<V<.5: C=Pink
        if abs(U-.25)<Tol and .5<=V<.8: C=Yellow
        if At['Lod']==0 and .05<U<.2 and abs(V+.1-(U-.05)*2.2)<.12: C=mix(C,Pink,.8)
        if U>.93 and abs(V)<.6: C=Yellow
        return C
    if p=='Tail':
        Cres=U-.12*V*V
        if .3<Cres<.62: return Yellow
        return Teal if Cres>=.62 else Green
    if p in('Dorsal','Anal'): return Teal if V>.6 else Orange
    if p=='Pectoral': return mix(hexc(0x1c6f5e),hexc(0x8fd6c2),U)
    if p=='Pelvic': return Orange
    if p=='Eye': return np.array([.004]*3) if U<.6 else hexc(0xf0b030)
    return Green
def stoplight(term):
    B=Body();B.HalfH=.18;B.HalfW=.072;B.Peak=.38;B.Blunt=.75;B.NoseR=.3;B.Ped=.42;B.PedW=.2;B.Arch=.1;B.TailLen=.17;B.TailH=.28;B.TailFork=.04
    B.Dor0=.27;B.Dor1=.9;B.DorH=.06;B.An0=.6;B.An1=.9;B.AnH=.05;B.PecU=.3;B.PecV=-.1;B.PecLen=.2;B.PecW=.1;B.PelU=.36;B.PelLen=.09;B.EyeU=.12;B.EyeV=.45;B.EyeR=.02
    if term: B.TailH=.36;B.TailFork=.32
    return B
if __name__=='__main__':
    rows=[]
    for term in (0,1):
        B=stoplight(term);P=paint_terminal if term else paint_initial
        ims=[]
        for lod in (0,1,2):
            parts=[build(B,lod,lod>0,P)]
            if lod==0:
                Th=math.asin(B.PecV);root=np.array([.5-B.PecU*(1-B.TailLen),B.HalfW*.98*math.cos(Th)+.004,B.HalfH*math.sin(Th)*(1-B.Arch)*.95])
                parts.append(pectoral(B,P,root,-35))
            print('term',term,'lod',lod,'tris',sum(len(p[3]) for p in parts))
            ims.append(render(parts,W=700))
        ims.append(render([build(B,0,True,P)],W=700,view='top'))
        row=Image.new('RGB',(700*4,350));[row.paste(im,(700*i,0)) for i,im in enumerate(ims)]
        rows.append(row)
    out=Image.new('RGB',(2800,700));out.paste(rows[0],(0,0));out.paste(rows[1],(0,350));out.save('parrot.png')
