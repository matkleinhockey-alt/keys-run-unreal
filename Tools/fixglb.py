# fixglb.py in.glb out.glb (after unmeshopt.py): every skinned mesh brought into the skeleton's own rest frame and merged into one mesh
# under one skin whose inverse bind matrices are the joints' rest transforms (what Unreal's importer assumes).
import sys, os, math; sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from glbio import *
js,bn=read(sys.argv[1])
nodes=js['nodes']
par={}
for i,n in enumerate(nodes):
    for c in n.get('children',[]): par[c]=i
def local(n):
    if 'matrix' in n: return list(n['matrix'])
    t=n.get('translation',[0,0,0]); r=n.get('rotation',[0,0,0,1]); s=n.get('scale',[1,1,1])
    x,y,z,w=r
    R=[1-2*(y*y+z*z),2*(x*y+z*w),2*(x*z-y*w),0, 2*(x*y-z*w),1-2*(x*x+z*z),2*(y*z+x*w),0, 2*(x*z+y*w),2*(y*z-x*w),1-2*(x*x+y*y),0, 0,0,0,1]
    for c in range(3):
        for r_ in range(3): R[r_+4*c]*=s[c]
    R[12],R[13],R[14]=t
    return R
G={}
def glob(i):
    if i in G: return G[i]
    m=local(nodes[i])
    if i in par: m=mm(glob(par[i]),m)
    G[i]=m; return m
skins=js['skins']; joints=skins[0]['joints']
# the skinned meshes' own world transforms (three.js binds with them: bindMatrix = the mesh node's matrixWorld)
MW={i:glob(i) for i,n in enumerate(nodes) if 'mesh' in n and 'skin' in n}
# each mesh's raw vertices to the rest world, from the hierarchy as it was exported (before anything is folded)
DM={}
for ni in MW:
    IB0=acc(js,bn,skins[nodes[ni]['skin']]['inverseBindMatrices'])
    Ds0=[mm(mm(glob(joints[j]),IB0[j]),MW[ni]) for j in range(len(joints))]
    DM[ni]=(Ds0[0], max(max(abs(a-b) for a,b in zip(Ds0[0],D)) for D in Ds0))
# fold every non-joint ancestor's transform (a Sketchfab export's -90 / +90 / -90 about X) into the top joints, and
# make those ancestors identity: Unreal builds the skeleton from the joints alone
def qmul(a,b):
    ax,ay,az,aw=a; bx,by,bz,bw=b
    return [aw*bx+ax*bw+ay*bz-az*by, aw*by-ax*bz+ay*bw+az*bx, aw*bz+ax*by-ay*bx+az*bw, aw*bw-ax*bx-ay*by-az*bz]
jset=set(joints)
tops=[j for j in joints if par.get(j) not in jset]
for tj in tops:
    A=[1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1]; qa=[0,0,0,1]; chain=[]
    i=par.get(tj)
    while i is not None:
        chain.append(i); i=par.get(i)
    for i in reversed(chain):
        n=nodes[i]
        assert 'matrix' not in n and n.get('scale',[1,1,1])==[1,1,1], 'ancestor with scale/matrix'
        A=mm(A,local(n)); qa=qmul(qa,n.get('rotation',[0,0,0,1]))
    # The top joint (a VRM export's _rootJoint) holds still at identity; the ancestors' turn goes on its children
    # (rest and keys), where Unreal treats it like any other bone's rotation. An animated top joint takes it itself.
    n=nodes[tj]
    still=n.get('rotation',[0,0,0,1])==[0,0,0,1] and n.get('translation',[0,0,0])==[0,0,0]
    for an in js.get('animations',[]):
        for ch in an['channels']:
            if ch['target'].get('node')==tj and ch['target']['path'] in ('rotation','translation'):
                sm=an['samplers'][ch['sampler']]; vals=acc(js,bn,sm['output'])
                rest=[0,0,0,1] if ch['target']['path']=='rotation' else [0,0,0]
                if max(max(abs(a-b) for a,b in zip(v,rest)) for v in vals)>1e-5: still=False
                else: ch['drop']=True
        an['channels']=[c for c in an['channels'] if not c.get('drop')]
    targets=[c for c in n.get('children',[]) if c in jset] if still else [tj]
    for c in targets:
        m=nodes[c]
        m['translation']=list(xf(A,m.get('translation',[0,0,0])))
        m['rotation']=qmul(qa,m.get('rotation',[0,0,0,1]))
        for an in js.get('animations',[]):
            for ch in an['channels']:
                if ch['target'].get('node')!=c: continue
                sm=an['samplers'][ch['sampler']]; path=ch['target']['path']
                vals=acc(js,bn,sm['output'])
                if path=='rotation': sm['output']=add(js,bn,[tuple(qmul(qa,list(v))) for v in vals],5126,'VEC4')
                elif path=='translation': sm['output']=add(js,bn,[xf(A,v) for v in vals],5126,'VEC3')
        print('folded',len(chain),'ancestors into',m.get('name'))
for tj in tops:
    i=par.get(tj)
    while i is not None:
        for k in ('translation','rotation','scale'): nodes[i].pop(k,None)
        i=par.get(i)
G.clear()
meshnodes=[i for i,n in enumerate(nodes) if 'mesh' in n and 'skin' in n]
prims=[]
for ni in meshnodes:
    n=nodes[ni]; sk=skins[n['skin']]
    assert sk['joints']==joints
    IB=acc(js,bn,sk['inverseBindMatrices'])
    D,dev=DM[ni]
    sc=(D[0]**2+D[1]**2+D[2]**2)**.5
    print('mesh node',ni,'D scale %.4f dev %.2e'%(sc,dev))
    for p in js['meshes'][n['mesh']]['primitives']:
        a=dict(p['attributes'])
        P=[xf(D,v) for v in acc(js,bn,a['POSITION'])]
        a['POSITION']=add(js,bn,P,5126,'VEC3',34962,True)
        if 'NORMAL' in a:
            Nn=[]
            for v in acc(js,bn,a['NORMAL']):
                q=xf(D,v,0.0); l=max(1e-9,(q[0]**2+q[1]**2+q[2]**2)**.5); Nn.append(tuple(x/l for x in q))
            a['NORMAL']=add(js,bn,Nn,5126,'VEC3',34962)
        np=dict(p); np['attributes']=a; prims.append(np)
IBM=[inv(glob(j)) for j in joints]
ibm=add(js,bn,IBM,5126,'MAT4')
js['meshes']=[{'name':'Body','primitives':prims}]
skin={'joints':joints,'inverseBindMatrices':ibm,'name':'Skin'}
js['skins']=[skin]
keep=meshnodes[0]
for ni in meshnodes:
    if ni!=keep:
        nodes[ni].pop('mesh',None); nodes[ni].pop('skin',None)
nodes[keep]['mesh']=0; nodes[keep]['skin']=0; nodes[keep]['name']='Body'
# the mesh node at the scene's root with no transform, so no parent can move it (three.js binds with its world)
for n in nodes:
    if keep in n.get('children',[]): n['children']=[c for c in n['children'] if c!=keep]
    if 'children' in n and not n['children']: n.pop('children')
for k in ('translation','rotation','scale','matrix'): nodes[keep].pop(k,None)
js['scenes'][js.get('scene',0)]['nodes'].append(keep)
write(sys.argv[2],js,bn)
print('ok', len(prims),'primitives')
