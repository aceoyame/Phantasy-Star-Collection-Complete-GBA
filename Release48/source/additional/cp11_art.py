"""Palette-exact angular lettering: never map antialiased grayscale to indices."""
from PIL import Image, ImageDraw
import struct

# Five-column, nine-row display alphabet. The split strokes and rightward
# slant echo the collection's angular title lettering without raster fringes.
FONT = {
 'A':['00100','01010','10001','10001','11111','10001','10001','10001','10001'],
 'D':['11110','10001','10001','10001','10001','10001','10001','10001','11110'],
 'I':['11111','00100','00100','00100','00100','00100','00100','00100','11111'],
 'T':['11111','00100','00100','00100','00100','00100','00100','00100','00100'],
 'O':['01110','10001','10001','10001','10001','10001','10001','10001','01110'],
 'N':['10001','11001','11001','10101','10101','10101','10011','10011','10001'],
 'L':['10000','10000','10000','10000','10000','10000','10000','10000','11111'],
 'C':['01111','10000','10000','10000','10000','10000','10000','10000','01111'],
 'E':['11111','10000','10000','10000','11110','10000','10000','10000','11111'],
}

def word(im, s, y, scale, ink, shadow=None, yscale=None):
    sy = scale if yscale is None else yscale
    width = sum(4 if c==' ' else 6 for c in s)*scale-scale+2*scale
    x = (240-width)//2
    d = ImageDraw.Draw(im)
    for c in s:
        if c==' ': x+=4*scale; continue
        points=[]
        for r,row in enumerate(FONT[c]):
            slant=(8-r)//4
            for col,v in enumerate(row):
                if v=='1': points.append((x+(col+slant)*scale,y+r*sy))
        if shadow is not None:
            for xx,yy in points:d.rectangle((xx+1,yy+1,xx+scale,yy+sy),fill=shadow)
        for xx,yy in points:d.rectangle((xx,yy,xx+scale-1,yy+sy-1),fill=ink)
        x+=6*scale

def make_card():
    im=Image.new('L',(240,160),0)
    word(im,'ADDITIONAL',57,3,14,3,yscale=2)
    word(im,'CONTENT',87,3,14,3,yscale=2)
    # Small diamond instead of the former solid blue strip.
    ImageDraw.Draw(im).polygon([(116,127),(120,123),(124,127),(120,131)],fill=15)
    return im

def make_background():
    im=Image.new('L',(240,160),0);d=ImageDraw.Draw(im)
    for x,y in [(17,6),(223,8),(9,28),(231,31),(18,128),(227,128)]:im.putpixel((x,y),1)
    word(im,'ADDITIONAL CONTENT',9,2,3)
    d.line((12,34,228,34),fill=4)
    # Open corners keep the background distinct from the ten-entry text list.
    for x,dx in [(4,1),(235,-1)]:
        d.line((x,40,x,129),fill=4);d.line((x,40,x+5*dx,35),fill=4)
        d.line((x,129,x+5*dx,134),fill=4)
    return im

def encode_card(im):
    tiles=[bytes(32)];lookup={tiles[0]:0};mapping=[]
    for ty in range(20):
        for tx in range(30):
            tile=bytes(im.getpixel((tx*8+x,ty*8+y))|(im.getpixel((tx*8+x+1,ty*8+y))<<4)
                       for y in range(8) for x in range(0,8,2))
            if tile not in lookup:lookup[tile]=len(tiles);tiles.append(tile)
            mapping.append(lookup[tile])
    gfx=b''.join(b'\0'+t for t in tiles)+b'\x80'
    assert len(gfx)<=0x1400, 'launcher tile budget exceeded'
    return gfx,struct.pack('<600H',*mapping),len(tiles)
