import re,struct,numpy as np,sys
step=[7,8,9,10,11,12,13,14,16,17,19,21,23,25,28,31,34,37,41,45,50,55,60,66,73,80,88,97,107,118,130,143,157,173,190,209,230,253,279,307,337,371,408,449,494,544,598,658,724,796,876,963,1060,1166,1282,1411,1552,1707,1878,2066,2272,2499,2749,3024,3327,3660,4026,4428,4871,5358,5894,6484,7132,7845,8630,9493,10442,11487,12635,13899,15289,16818,18500,20350,22385,24623,27086,29794,32767]
idx_t=[-1,-1,-1,-1,2,4,6,8]
def dec(data):
    out=[]; bad=0
    for b in range(len(data)//36):
        blk=data[b*36:(b+1)*36]; pred,ix=struct.unpack('<hB',blk[:3])
        if ix>88: bad+=1
        ix=min(ix,88); out.append(pred)
        for byte in blk[4:]:
            for nib in (byte&15, byte>>4):
                s=step[ix]; d=s>>3
                if nib&4: d+=s
                if nib&2: d+=s>>1
                if nib&1: d+=s>>2
                pred = pred-d if nib&8 else pred+d
                pred=max(-32768,min(32767,pred)); ix=max(0,min(88,ix+idx_t[nib&7])); out.append(pred)
    return np.array(out,dtype=np.float32),bad
def flat(x):
    n=min(len(x),16384); sp=np.abs(np.fft.rfft(x[:n]*np.hanning(n)))+1e-9; return np.exp(np.log(sp).mean())/sp.mean()
if __name__=='__main__':
    pid=int(open('/root/fo2x/run/pid').read()); log=open('/root/fo2x/run/run.log',errors='ignore').read()
    off=int(re.search(r'host offset 0x([0-9A-Fa-f]+)',log).group(1),16)
    f=open(f'/proc/{pid}/mem','rb',buffering=0)
    for va in [int(a,16) for a in sys.argv[1:]]:
        f.seek(off+va); data=f.read(36*600)
        x,bad=dec(data); print(hex(va),'bad hdr',bad,'/600 rms',int(np.sqrt((x**2).mean())),'flat %.3f'%flat(x), data[:8].hex())
