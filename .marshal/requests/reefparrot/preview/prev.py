# offline preview of KeysReefLifeBody.cpp's BuildBody + a species' paint (a line-for-line port; the game is the truth)
import numpy as np, math, sys
from PIL import Image
HP=math.pi/2
def hexc(h):
    c=np.array([(h>>16)&255,(h>>8)&255,h&255])/255.0
    return np.where(c<=0.04045,c/12.92,((c+0.055)/1.055)**2.4)
def mix(a,b,t): t=min(1,max(0,t)); return a+(b-a)*t
class Body:
    HalfH=.17;HalfW=.075;Peak=.32;Blunt=.5;NoseR=.22;Ped=.4;PedW=.2;Arch=.08;TailLen=.17;TailH=.3;TailFork=0
    Dor0=.3;Dor1=.9;DorH=.07;An0=.62;An1=.9;AnH=.055;PecU=.27;PecV=-.15;PecLen=.17;PecW=.09;PelU=.34;PelLen=.09;EyeU=.1;EyeV=.4;EyeR=.022
def sm(t): t=min(1,max(0,t)); return t*t*(3-2*t)
def profH(B,U):
    if U<B.Peak: return B.NoseR+(1-B.NoseR)*math.sin(U/B.Peak*HP)**(1+(.42-1)*B.Blunt)
    return 1+(B.Ped-1)*sm((U-B.Peak)/(1-B.Peak))
def profW(B,U):
    Pk=B.Peak*.85
    if U<Pk: return .4+.6*math.sin(U/Pk*HP)**.6
    return 1+(B.PedW-1)*sm((U-Pk)/(1-Pk))
def bx(B,U): return .5-U*(1-B.TailLen)
def bpt(B,U,Th):
    S=math.sin(Th);C=math.cos(Th)
    H=B.HalfH*profH(B,U)*((1+B.Arch) if S>0 else (1-B.Arch))
    return np.array([bx(B,U),B.HalfW*profW(B,U)*C,H*S])
def bnorm(B,U,Th):
    E=.004
    Du=bpt(B,min(1,U+E),Th)-bpt(B,max(0,U-E),Th); Dt=bpt(B,U,Th+.02)-bpt(B,U,Th-.02)
    N=np.cross(Dt,Du); n=np.linalg.norm(N)
    N=N/n if n>1e-12 else np.array([0,math.cos(Th),math.sin(Th)])
    if N[1]*math.cos(Th)+N[2]*math.sin(Th)<0: N=-N
    return N
def build(B,Lod,withPec,paint):
    V=[];N=[];C=[];T=[]
    def add(p,n,c): V.append(np.array(p,float));N.append(np.array(n,float));C.append(np.array(c,float)); return len(V)-1
    def col(part,U,Vv,I=0,J=0,Side=1): return paint(dict(Part=part,U=U,V=Vv,I=I,J=J,Side=Side,Lod=Lod))
    def sheet(P,Cs,NU,NV,n):
        i0=len(V)
        for p,c in zip(P,Cs): add(p,n,c)
        for v in range(NV-1):
            for u in range(NU-1):
                a=i0+v*NU+u; T.extend([(a,a+1,a+NU+1),(a,a+NU+1,a+NU)])
    NS={0:24,1:10,2:5}[Lod];NA={0:16,1:8,2:4}[Lod]
    Us=[.012+.988*(i/NS)**1.3 for i in range(NS+1)]
    for i in range(NS+1):
        for j in range(NA):
            Th=HP+2*math.pi*j/NA
            add(bpt(B,Us[i],Th),bnorm(B,Us[i],Th),col('Body',Us[i],math.sin(Th),i,j,1 if math.cos(Th)>=0 else -1))
    for i in range(NS):
        for j in range(NA):
            a=i*NA+j;b=i*NA+(j+1)%NA;c=(i+1)*NA+(j+1)%NA;d=(i+1)*NA+j; T.extend([(a,b,c),(a,c,d)] if (i+j)&1 else [(a,b,d),(b,c,d)])
    nose=add([bx(B,0)+.006,0,0],[1,0,0],col('Body',0,0)); butt=add([bx(B,1),0,0],[-1,0,0],col('Body',1,0,NS))
    for j in range(NA): T.append((nose,j,(j+1)%NA)); T.append((butt,NS*NA+j,NS*NA+(j+1)%NA))
    Nn={0:9,1:4,2:2}[Lod]
    for Fin in range(2):
        U0,U1,Ht,Sg=(B.An0,B.An1,B.AnH,-1) if Fin else (B.Dor0,B.Dor1,B.DorH,1)
        if Ht<=0 or U1<=U0: continue
        P=[None]*(Nn*2);Cs=[None]*(Nn*2)
        for k in range(Nn):
            Tt=k/(Nn-1);U=U0+(U1-U0)*Tt
            Zb=bpt(B,U,Sg*HP)[2]*.93;Hp=Ht*math.sin(math.pi*(.05+.9*Tt))**.4
            P[k]=[bx(B,U),0,Zb];P[Nn+k]=[bx(B,U)-Hp*.35,0,Zb+Sg*Hp]
            part='Anal' if Fin else 'Dorsal'
            Cs[k]=col(part,Tt,0,k,0);Cs[Nn+k]=col(part,Tt,1,k,1)
        sheet(P,Cs,Nn,2,[0,1,0])
    NU,NV=(5,7) if Lod==0 else (2,3)
    Xp=bx(B,1)+.01;R0=B.HalfH*B.Ped*.95;P=[];Cs=[]
    for v in range(NV):
        for u in range(NU):
            Sx=u/(NU-1);Vv=-1+2*v/(NV-1)
            Edge=-.5+(B.TailFork*B.TailLen*(1-Vv*Vv) if B.TailFork>=0 else -B.TailFork*B.TailLen*Vv*Vv)
            P.append([Xp+(Edge-Xp)*Sx,0,Vv*(R0+(B.TailH*.5-R0)*Sx**.8)]);Cs.append(col('Tail',Sx,Vv,u,v))
    sheet(P,Cs,NU,NV,[0,1,0])
    if Lod<2:
        for Sd in (-1,1):
            Th=math.asin(B.PecV) if Sd>0 else math.pi-math.asin(B.PecV)
            if withPec and B.PecLen>0:
                Rt=bpt(B,B.PecU,Th)+np.array([0,Sd*.003,0]);Dir=np.array([-.84,Sd*.5,-.2]);up=np.array([0,0,1.0])
                P=[Rt-up*B.PecW*.25,Rt+Dir*B.PecLen*.85-up*B.PecW*.5,Rt+up*B.PecW*.25,Rt+Dir*B.PecLen+up*B.PecW*.5]
                sheet(P,[col('Pectoral',0,-1),col('Pectoral',1,-1),col('Pectoral',0,1),col('Pectoral',1,1)],2,2,[.5,Sd*.84,0])
            if Lod==0 and B.PelLen>0:
                Rt=bpt(B,B.PelU,-HP+Sd*.35)
                P=[Rt,Rt-np.array([B.PelLen*.35,0,0]),Rt+np.array([-B.PelLen*.5,Sd*.012,-B.PelLen*.45]),Rt+np.array([-B.PelLen,Sd*.02,-B.PelLen*.4])]
                sheet(P,[col('Pelvic',0,0),col('Pelvic',0,1),col('Pelvic',1,0),col('Pelvic',1,1)],2,2,[0,Sd,0])
            if B.EyeR>0:
                Te=math.asin(B.EyeV) if Sd>0 else math.pi-math.asin(B.EyeV)
                Nr=bnorm(B,B.EyeU,Te);Ctr=bpt(B,B.EyeU,Te)+Nr*.004
                T1=np.cross(Nr,[0,0,1]);T1/=np.linalg.norm(T1);T2=np.cross(Nr,T1)
                NE=8 if Lod==0 else 4;E0=add(Ctr+Nr*.003,Nr,col('Eye',0,0))
                for Ring in range(2):
                    for k in range(NE):
                        A=2*math.pi*k/NE;Rr=B.EyeR*(1.0 if Ring else .55)
                        add(Ctr+(T1*math.cos(A)+T2*math.sin(A))*Rr+Nr*(0 if Ring else .002),Nr,col('Eye',1.0 if Ring else .55,0))
                for k in range(NE):
                    a=E0+1+k;b=E0+1+(k+1)%NE; T.extend([(E0,a,b),(a,a+NE,b+NE),(a,b+NE,b)])
    return np.array(V),np.array(N),np.array(C),T
def pectoral(B,paint,root,ang):
    Ss=[0,.35,.72,1];P=[];Cs=[]
    for v in range(3):
        for u in range(4):
            S=Ss[u];Vv=v-1.0
            W=B.PecW*(.22+.78*math.sin(HP*min(1,S/.72)))
            if S>.72: W*=1-.55*((S-.72)/.28)**2
            p=np.array([-S*B.PecLen*(1+.12*Vv),0,Vv*W*.5+S*B.PecLen*.1])
            a=math.radians(ang); p=np.array([p[0]*math.cos(a)-p[1]*math.sin(a), p[0]*math.sin(a)+p[1]*math.cos(a), p[2]])
            P.append(p+root);Cs.append(paint(dict(Part='Pectoral',U=S,V=Vv,I=u,J=v,Side=1,Lod=0)))
    T=[]
    for v in range(2):
        for u in range(3):
            a=v*4+u;T.extend([(a,a+1,a+5),(a,a+5,a+4)])
    return np.array(P),np.tile([0,1.0,0],(12,1)),np.array(Cs),T
def render(parts,W=900,view='side',bg=(0.05,0.22,0.3)):
    H=int(W*.5);img=np.tile(np.array(bg),(H,W,1)).astype(float);zb=np.full((H,W),-1e9)
    L=np.array([.3,.55,.78]);L/=np.linalg.norm(L)
    for V,N,C,T in parts:
        if view=='side': X=-V[:,0];Y=V[:,2];Z=V[:,1];Nz=N[:,1]
        else: X=-V[:,0];Y=V[:,1];Z=V[:,2];Nz=N[:,2]
        sx=(X+.55)/1.1*W; sy=H/2-Y/1.1*W
        for (a,b,c) in T:
            xs=np.array([sx[a],sx[b],sx[c]]);ys=np.array([sy[a],sy[b],sy[c]])
            x0=int(max(0,math.floor(xs.min())));x1=int(min(W-1,math.ceil(xs.max())));y0=int(max(0,math.floor(ys.min())));y1=int(min(H-1,math.ceil(ys.max())))
            if x1<x0 or y1<y0: continue
            gx,gy=np.meshgrid(np.arange(x0,x1+1)+.5,np.arange(y0,y1+1)+.5)
            den=(ys[1]-ys[2])*(xs[0]-xs[2])+(xs[2]-xs[1])*(ys[0]-ys[2])
            if abs(den)<1e-9: continue
            w0=((ys[1]-ys[2])*(gx-xs[2])+(xs[2]-xs[1])*(gy-ys[2]))/den;w1=((ys[2]-ys[0])*(gx-xs[2])+(xs[0]-xs[2])*(gy-ys[2]))/den;w2=1-w0-w1
            m=(w0>=-1e-4)&(w1>=-1e-4)&(w2>=-1e-4)
            z=w0*Z[a]+w1*Z[b]+w2*Z[c]
            sub=zb[y0:y1+1,x0:x1+1];m&=z>sub
            if not m.any(): continue
            n=w0[...,None]*N[a]+w1[...,None]*N[b]+w2[...,None]*N[c]
            nn=n/np.maximum(1e-9,np.linalg.norm(n,axis=2,keepdims=True))
            if view=='side': nv=np.stack([-nn[...,0],nn[...,2],nn[...,1]],-1)
            else: nv=np.stack([-nn[...,0],nn[...,1],nn[...,2]],-1)
            lam=.45+.55*np.abs(nv@L)
            col=(w0[...,None]*C[a]+w1[...,None]*C[b]+w2[...,None]*C[c])*lam[...,None]
            sub[m]=z[m];img[y0:y1+1,x0:x1+1][m]=col[m]
    s=np.where(img<=.0031308,img*12.92,1.055*np.clip(img,0,1)**(1/2.4)-.055)
    return Image.fromarray((np.clip(s,0,1)*255).astype(np.uint8))
