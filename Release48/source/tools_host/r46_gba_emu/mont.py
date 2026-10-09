from PIL import Image
import sys
def montage(paths,out,cols=4):
    ims=[Image.open(p) for p in paths]
    rows=(len(ims)+cols-1)//cols
    m=Image.new('RGB',(240*cols,160*rows))
    for i,im in enumerate(ims): m.paste(im.resize((240,160)),((i%cols)*240,(i//cols)*160))
    m.save(out)
