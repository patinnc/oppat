import visvis as vv
import json
import sys
import imageio
from PIL import Image, ImageFont, ImageDraw

# read file
with open('json_table.json', 'r') as myfile:
    data=myfile.read()

# parse file
obj = json.loads(data)
print(obj['txt'])
tarr = obj['txt']
print(len(tarr))
x0 = -1
x1 = -1
y0 = -1
y1 = -1
cpu_typ = 0 # 0=intel, 1=arm
if cpu_typ == 0:
    lkfor = 'RAT_uops/sec values'
else:
    lkfor = 'GIPS GigaInstructions/sec: values'
val_arr = []
lp_arr = []
for i in range(0, len(tarr)):
    print("i= %d, phs_beg= %s" % (i, tarr[i]['phase0']))
    lp_arr.append(tarr[i]['lp'])
    kv_arr = tarr[i]['key_val_arr']
    for j in range(0, len(kv_arr)):
        ky = kv_arr[j]['key']
        val = kv_arr[j]['vals']
        var = kv_arr[j]['yvar']
        if i == 0:
            #print("j= %d, key= '%s'" % (j, ky))
            if len(ky) > 6 and ky[-6:] == ' shape' and len(val) == 4 and var[0] == 'x0':
                if x0 == -1 or x0 > val[0]:
                    x0 = val[0]
                if x1 == -1 or x1 < val[1]:
                    x1 = val[1]
                if y0 == -1 or y0 > val[2]:
                    y0 = val[2]
                if y1 == -1 or y1 < val[3]:
                    y1 = val[3]
                print("key= %s" % (ky))
        if ky == lkfor:
            print("vals ", val)
            val_arr.append(val)

print("x0= %.3f, x1= %.3f, y0= %.3f, y1= %.3f" % (x0, x1, y0, y1))
print("val_arr.len= %d" % (len(val_arr)))
uval = []
for i in range(0, len(val_arr)):
    sum = 0.0
    for j in range(0, len(val_arr[i])):
        sum += val_arr[i][j]
    uval.append([sum, i])

use_len = len(uval)

print("uval.len= %d" % (len(uval)))
print("uval= ", uval)

def firstItem(ls):
    #return the third item of the list
    return ls[0]

uval.sort(key=firstItem)
#array.sort(key = lambda x:x[1])
#l = [[0, 1, 'f'], [4, 2, 't'], [9, 4, 'afsd']]
#l.sort(key=lambda x: x[2])

print("uval sorted: ", uval)

#sys.exit(1)

app = vv.use()

img_lst = []
for i in range(0, use_len):
    img_nm = "pat_dash%.5d.png" % lp_arr[i]
    print("nm= %s" % img_nm)
    img_lst.append(vv.imread(img_nm))

#sys.exit(1)
#img = img[:-1,:-1] # make not-power-of-two (to test if video driver is capable)
#print(img.shape)

y0i = int(y0)
y1i = int(y1)+1
x1i = int(x1)+1

vv.figure()
a1 = vv.subplot(121)
#im = vv.Aarray(im)
im = vv.Aarray((y1i+y0i, use_len*x1i, 4))

# Cut in four pieces, but also change resolution using different step sizes
#im1 = im[:300,:300]
#im2 = im
# Create new camera and attach
#cam = vv.cameras.TwoDCamera()
#a1.camera = cam

for i in range(0, use_len):
    j = uval[i][1]
    im[:y1i+y0i,i*x1i:(i+1)*x1i] = img_lst[j][:y1i+y0i,:x1i]
    #x, y = (0, 288-fsz)
    #text = "Display "+bmarkf+" sub-benchmark for interval, BW"
    #txt = ("GIPS: %.3f bill instr/sec" % uval[i][0])
    #w, h = font.getsize(text)
    #drawd.rectangle((x, y, x + w + 2*rct_brdr, y + h), fill='black')
    #drawd.text((x+rct_brdr, y), text, fill='white', font=font)
    #vv.Text(a1, txt, i*x1i, 20, 0, 'mono', 10)
vv.title("hi")

#t1 = vv.Text(im2, 'Visvis text', 0.2, 9, 0, 'mono', 30)
#vv.Label(im2, '| ' + 'hime'+ ' - %1.2f pixels offset' % offset)

# Create figure with two axes


##label1 = vv.Label(a1, 'This is a Label')
##label1.position = 10, 10
##label1.bgcolor = (0.5, 1, 0.5)
#t1 = vv.Text(a1, 'Visvis text', 0.2, 0, 0, 'mono', 20)

#t = vv.imshow(im2, axes=a1)
t = vv.imshow(im)
t.aa = 2 # more anti-aliasing (default=1)
t.interpolate = True # interpolate pixels


vv.imwrite('pat.png', im)
im4 = Image.open('pat.png')
draw = ImageDraw.Draw(im4)
fsz = int(20)
sz = fsz + int(4)
font = ImageFont.truetype("/Program Files/Microsoft Office 15/root/vfs/Fonts/private/arialn.ttf", fsz)
for i in range(0, use_len):
    j = uval[i][1]
    #im[:y1i+y0i,i*x1i:(i+1)*x1i] = img_lst[j][:y1i+y0i,:x1i]
    #text = "Display "+bmarkf+" sub-benchmark for interval, BW"
    if cpu_typ == 0:
        txt = ("GUOPS: %.3f bill uops/sec" % uval[i][0])
    else:
        txt = ("GIPS: %.3f bill instr/sec" % uval[i][0])

    draw.text((i*x1i, fsz), txt, (0,0,0), font=font)
    #w, h = font.getsize(text)
    #drawd.rectangle((x, y, x + w + 2*rct_brdr, y + h), fill='black')
    #drawd.text((x+rct_brdr, y), text, fill='white', font=font)
    #vv.Text(a1, txt, i*x1i, 20, 0, 'mono', 10)
if cpu_typ == 0:
    txt = "OPPAT dashboard charts for each phase. Phases sorted by lowest to highest GUOPS (billion uops/sec)"
else:
    txt = "OPPAT dashboard charts for each phase. Phases sorted by lowest to highest GIPS (billion instructions/sec)"
draw.text((0, 0), txt, (0,0,0), font=font)
imageio.imwrite('pat.png', im4)

#tt = []
#for im in [im2, im2, im2]:
#    tt.append(vv.imshow(im))

app.Run()
