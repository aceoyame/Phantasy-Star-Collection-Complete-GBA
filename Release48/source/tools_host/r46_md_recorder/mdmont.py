from PIL import Image
def montage(paths,out,cols=4,w=320,h=224):
    ims=[Image.open(p) for p in paths]; rows=(len(ims)+cols-1)//cols
    m=Image.new('RGB',(w*cols,h*rows))
    for i,im in enumerate(ims): m.paste(im,((i%cols)*w,(i//cols)*h))
    m.save(out)
