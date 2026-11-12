import json, struct
CT={5126:('f',4),5123:('H',2),5121:('B',1),5125:('I',4),5122:('h',2),5120:('b',1)}
NC={'SCALAR':1,'VEC2':2,'VEC3':3,'VEC4':4,'MAT4':16}
def read(path):
    b=open(path,'rb').read(); off=12; js=None; bn=b''
    while off<len(b):
        ln,ty=struct.unpack('<II',b[off:off+8]); c=b[off+8:off+8+ln]
        if ty==0x4E4F534A: js=json.loads(c)
        elif ty==0x004E4942: bn=c
        off+=8+ln
    return js,bytearray(bn)
def acc(js,bn,i):
    a=js['accessors'][i]
    f,sz=CT[a['componentType']]; n=NC[a['type']]
    if 'bufferView' not in a: return [tuple([0]*n)]*a['count']
    bv=js['bufferViews'][a['bufferView']]
    st=bv.get('byteStride',sz*n); base=bv.get('byteOffset',0)+a.get('byteOffset',0)
    out=[]
    for k in range(a['count']):
        out.append(struct.unpack_from('<%d%s'%(n,f),bn,base+k*st))
    return out
def add(js,bn,vals,ctype,typ,target=None,minmax=False):
    f,sz=CT[ctype]; n=NC[typ]
    while len(bn)%4: bn.append(0)
    off=len(bn)
    for v in vals: bn.extend(struct.pack('<%d%s'%(n,f),*v))
    bv={'buffer':0,'byteOffset':off,'byteLength':len(bn)-off}
    if target: bv['target']=target
    js['bufferViews'].append(bv)
    a={'bufferView':len(js['bufferViews'])-1,'componentType':ctype,'count':len(vals),'type':typ}
    if minmax:
        a['min']=[min(v[k] for v in vals) for k in range(n)]; a['max']=[max(v[k] for v in vals) for k in range(n)]
    js['accessors'].append(a); return len(js['accessors'])-1
def write(path,js,bn):
    while len(bn)%4: bn.append(0)
    js['buffers']=[{'byteLength':len(bn)}]
    j=json.dumps(js,separators=(',',':')).encode()
    while len(j)%4: j+=b' '
    out=bytearray(b'glTF'+struct.pack('<II',2,12+8+len(j)+8+len(bn)))
    out+=struct.pack('<II',len(j),0x4E4F534A)+j+struct.pack('<II',len(bn),0x004E4942)+bn
    open(path,'wb').write(out)
# column-major 4x4
def mm(a,b): return [sum(a[r+4*k]*b[k+4*c] for k in range(4)) for c in range(4) for r in range(4)]
def inv(m):
    import copy
    a=[[m[r+4*c] for c in range(4)]+[1.0 if r==i else 0.0 for i in range(4)] for r in range(4)]
    for c in range(4):
        p=max(range(c,4),key=lambda r:abs(a[r][c])); a[c],a[p]=a[p],a[c]
        d=a[c][c]; a[c]=[x/d for x in a[c]]
        for r in range(4):
            if r!=c:
                f=a[r][c]; a[r]=[x-f*y for x,y in zip(a[r],a[c])]
    return [a[r][4+c] for c in range(4) for r in range(4)]
def xf(m,v,w=1.0): return tuple(sum(m[r+4*k]*(list(v)+[w])[k] for k in range(4)) for r in range(3))
