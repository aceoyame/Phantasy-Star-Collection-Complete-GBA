"""Lossless PSIV R235 battle row token packer (240 indexed pixels per row).
R199 backdrop_rows.py is the reference decoder. No extracted game assets here."""
def ext(n):
    assert n >= 0
    if n < 15: return n,b''
    n-=15; chunks=[]
    while n>=255: chunks.append(255); n-=255
    chunks.append(n); return 15,bytes(chunks)
def match(buf,pos):
    if pos<1 or pos+4>len(buf):return (0,0)
    best=(0,0); start=max(0,pos-255)
    at=buf.find(buf[pos:pos+4],start,pos)
    while at>=0:
        maximum=min(255,len(buf)-pos); length=4
        while length<maximum and buf[at+length]==buf[pos+length]:length+=1
        if length>best[0]:best=(length,pos-at)
        if length==maximum:break
        at=buf.find(buf[pos:pos+4],at+1,pos)
    return best
def encode_row(data):
    data=bytes(data)
    assert len(data)==240
    out=bytearray(); literal=bytearray(); i=0
    while i<240:
        length,distance=match(data,i)
        if length<4:
            literal.append(data[i]);i+=1;continue
        high,high_ext=ext(len(literal));low,low_ext=ext(length-4)
        out.append((high<<4)|low)
        out.extend(high_ext);out.extend(literal)
        out.append(distance);out.extend(low_ext)
        i+=length;literal.clear()
    if literal:
        high,extra=ext(len(literal))
        out.append(high<<4);out.extend(extra);out.extend(literal)
    return bytes(out)
def decode_row(stream):
    stream=bytes(stream);p=0;out=bytearray()
    def var(n):
        nonlocal p
        if n==15:
            while True:
                x=stream[p];p+=1;n+=x
                if x!=255:break
        return n
    while len(out)<240:
        token=stream[p];p+=1
        length=var(token>>4)
        out.extend(stream[p:p+length]);p+=length
        if len(out)==240:break
        offset=stream[p];p+=1
        for _ in range(var(token&15)+4):out.append(out[-offset])
    assert len(out)==240 and p==len(stream)
    return bytes(out)
if __name__=='__main__':
    import random
    for t in range(400):
        raw=bytes(random.randrange(8) for _ in range(240)) if t%2 else bytes([random.randrange(8)])*240
        assert decode_row(encode_row(raw))==raw
    print('400 battle row encoder/decoder round trips PASS')
