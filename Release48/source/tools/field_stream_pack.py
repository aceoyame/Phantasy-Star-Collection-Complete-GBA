"""Small deterministic LZ4 block encoder for immutable load-time field streams.

Only the ROM representation changes. The runtime expands directly into the
original destination, with no cache or additional persistent RAM.
"""
def pack_block(data):
    out=bytearray();table={};anchor=0;i=0;limit=len(data)-12
    def extra(n):
        while n>=255:out.append(255);n-=255
        out.append(n)
    while i<=limit:
        key=data[i:i+4];previous=table.get(key);table[key]=i
        if previous is None or i-previous>65535:
            i+=1;continue
        end=i+4
        while end<len(data)-5 and data[previous+end-i]==data[end]:end+=1
        literal=i-anchor;match=end-i-4
        out.append((min(literal,15)<<4)|min(match,15))
        if literal>=15:extra(literal-15)
        out.extend(data[anchor:i]);out.extend((i-previous).to_bytes(2,'little'))
        if match>=15:extra(match-15)
        for pos in range(i+1,end):table[data[pos:pos+4]]=pos
        i=anchor=end
    literal=len(data)-anchor;out.append(min(literal,15)<<4)
    if literal>=15:extra(literal-15)
    out.extend(data[anchor:]);return bytes(out)

def pack_bank(decoded):
    blob=bytearray();entries={0:[],1:[]};cache={};raw_bytes=0;packed_count=0
    for codec,source,data in decoded:
        encoded=pack_block(data);packed=len(encoded)<len(data)
        payload=encoded if packed else data;key=(packed,payload)
        if key in cache:offset=cache[key]
        else:
            blob.extend(bytes((-len(blob))&3));offset=len(blob)
            blob.extend(payload);cache[key]=offset
        entries[codec].append((source,offset|(0x80000000 if packed else 0),len(data)))
        raw_bytes+=len(data);packed_count+=packed
    blob.extend(bytes((-len(blob))&3))
    return blob,entries,{'decoded_bytes':raw_bytes,'packed_streams':packed_count,'stored_bytes':len(blob),'unique_payloads':len(cache)}
