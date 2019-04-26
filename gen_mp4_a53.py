import glob
import sys
import imageio

gb = 1
fpsn=10
fpsn=1
xold = 1017
yold = 1388
# 1148,1326
# old png x:1017 y:1388
# new png x: 912 y:1244
bmark = "Geekbench"
bmarkf = "Geekbench 2.4.2"
pdir = "../gb_pngs2/"
pdir = "./"
if len(sys.argv) > 1:
    print("args= ", sys.argv)
    for j in range(1, len(sys.argv)):
        print("arg[%d]= %s" % (j, sys.argv[j]))
        if sys.argv[j] == "spin":
            print("got spin")
            gb = 0
            fpsn = 1
            bmark = "spin_test"
            bmarkf = bmark
            pdir = "./"

#fileList = []
fileList_orig = glob.glob(pdir+"pat0*.png")
#for file in os.listdir(path):
#    if file.startswith(name):
#        complete_path = path + file
#        fileList.append(complete_path)


#sys.exit(0)

writer = imageio.get_writer('test.mp4', fps=fpsn)

fileList = fileList_orig
img_dta = imageio.imread(fileList[0])
img_shape = img_dta.shape
szx = img_shape[1]
szy = img_shape[0]
xnew = szx
ynew = szy
xfctr = xnew/xold
yfctr = ynew/yold
xfctr = 1.0
yfctr = 1.0
print("xsz= %d, ysz= %d, xfctr= %f yfctr= %f" % (szx, szy, xfctr, yfctr))


from PIL import Image, ImageFont, ImageDraw
#font = ImageFont.truetype("/usr/share/fonts/dejavu/DejaVuSans.ttf", 25)
font = ImageFont.truetype("/Program Files/Microsoft Office 15/root/vfs/Fonts/private/arialn.ttf", 50)
fontb = ImageFont.truetype("/Program Files/Microsoft Office 15/root/vfs/Fonts/private/arialnb.ttf", 60)
#img = Image.new("RGBA", (200,200), (120,20,20))

def gety(row):
    row[1] = row[1] + row[2]
    row[0] += 1
    print("row= %d" % (row[0]))
    return row[1]

img = Image.new("RGBA", (szx,szy), (255,255,255))
draw = ImageDraw.Draw(img)
#image = Image.open(fileList[0])
#draw = ImageDraw.Draw(image)
sz = 60
row = [0, 10, 60]
offx = 40
if gb == 1:
    draw.text((offx, gety(row)), bmark+" on ARM Cortex A53 - the movie", (0,0,0), font=fontb)
else:
    draw.text((offx, gety(row)), "spin_test - the movie", (0,0,0), font=fontb)
draw.text((offx, gety(row)), "Created using OPPAT", (0,0,0), font=font)
draw.text((offx, gety(row)), "Open Power/Performance Analysis Tool", (0,0,0), font=font)
draw.text((offx, gety(row)), "See https://patinnc.github.io/", (0,0,0), font=font)
draw_ttl = draw
fsz = int(yfctr*35)
sz = fsz + int(yfctr*4)
font = ImageFont.truetype("/Program Files/Microsoft Office 15/root/vfs/Fonts/private/arialn.ttf", fsz)
row[1] += 30
row[2] = sz
draw.text((offx, gety(row)), "The performance data is from a single run of "+bmarkf+" 32bit", (0,0,0), font=font)
draw.text((offx, gety(row)), "on a 4 core ARM Cortex A53 (raspberry pi 3 b+).", (0,0,0), font=font)
draw.text((offx, gety(row)), "The OS is 64bit Ubuntu Mate 18.04. Geekbench4 isn't available", (0,0,0), font=font)

draw.text((offx, gety(row)), "for ARM Linux so I used a 32bit v2.4.2 ARM build. See", (0,0,0), font=font)
draw.text((offx, gety(row)), "http://geekbench.s3.amazonaws.com/geekbench-2.4.2-LinuxARM.tar.gz", (0,0,0), font=font)
draw.text((offx, gety(row)), "The A53 is a 4 core, in-order, 8 stage pipeline with 512KB shared L2.", (0,0,0), font=font)
draw.text((offx, gety(row)), "The cpu diagram shows resource constraints on A53 such as", (0,0,0), font=font)
draw.text((offx, gety(row)), "the max 2 instructions/cycle from the Instruction Fetch Unit and pathway bytes/cycle.", (0,0,0), font=font)
draw.text((offx, gety(row)), "OPPAT shows resource utilization numerically and with a simple", (0,0,0), font=font)
draw.text((offx, gety(row)), "red/yellow/green light for each core. This lets you see where the pipeline", (0,0,0), font=font)
draw.text((offx, gety(row)), "is stalled (frontend or backend L2 or memory, buffers, interlocks, etc.", (0,0,0), font=font)
draw.text((offx, gety(row)), "Data is collected using Linux perf stat/record, trace-cmd and custom utils.", (0,0,0), font=font)
draw.text((offx, gety(row)), "Perf collects data for the 50+ hw events, frace and flamegraphs.", (0,0,0), font=font)
draw.text((offx, gety(row)), "GB phase is used but no power or temperature is avail.", (0,0,0), font=font)
draw.text((offx, gety(row)), "OPPAT can collect/analyze data on:", (0,0,0), font=font)
draw.text((offx, gety(row)), " Linux/Android - perf, trace-cmd, sysfs, other", (0,0,0), font=font)
draw.text((offx, gety(row)), " windows - ETW, PCM, other", (0,0,0), font=font)
row[1] += 0.8*sz
draw.text((offx, gety(row)), "This movie shows average value for each phase.", (0,0,0), font=font)
draw.text((offx, gety(row)), "OPPAT calculates when the 'multi-core' section of each phase begins.", (0,0,0), font=font)
draw.text((offx, gety(row)), "The single core values are harder to interpret since GB can change cores.", (0,0,0), font=font)
row[1] += 0.8*sz
draw.text((offx, gety(row)), "On windows the movie is best viewed vertically (ctrl+alt+right_arrow)", (0,0,0), font=font)
draw.text((offx, gety(row)), "Do ctrl+alt+up_arrow to return to normal mode.", (0,0,0), font=font)
draw.text((offx, gety(row)), "contact patrick.99.fay@gmail.com", (0,0,0), font=font)
draw.text((offx, gety(row)), "Block diagram derived from WikiChip.org", (0,0,0), font=font)
print("y last= %d" % (gety(row)))
draw = ImageDraw.Draw(img)
img.save("a_test.png")
#draw = ImageDraw.Draw(image)
#image.save("a_test.png")
#sys.exit(0)

#06/30/2015  03:23 PM           175,956 ARIALN.TTF
#06/30/2015  03:23 PM           180,740 ARIALNB.TTF
#06/30/2015  03:23 PM           180,084 ARIALNBI.TTF
#06/30/2015  03:23 PM           181,124 ARIALNI.TTF
#06/30/2015  03:23 PM        23,275,812 ARIALUNI.TTF

#imgd = Image.open(pdir+"pat00000.png")
#drawd = ImageDraw.Draw(imgd)
#rct_brdr = 10
#x, y = (10, 50 - fsz)
#text = "CPU front end: instructions from L2 or memory for execution"
#w, h = font.getsize(text)
#drawd.rectangle((xfctr*x, yfctr*y, xfctr*(x + w + 2*rct_brdr), yfctr*(y + h)), fill='black')
#drawd.text((xfctr*(x+rct_brdr), yfctr*y), text, fill='white', font=font)
#drawd.rectangle([xfctr*x,yfctr*y,xfctr*(szx-10),yfctr*600], width = rct_brdr, outline="#0000ff")
#
#x, y = (10, 660 - fsz)
#text = "CPU execution engine: UOPS+data execute in ports"
#w, h = font.getsize(text)
#drawd.rectangle((xfctr*x, yfctr*y, xfctr*(x + w + 2*rct_brdr), yfctr*(y + h)), fill='black')
#drawd.text((xfctr*(x+rct_brdr), yfctr*y), text, fill='white', font=font)
#drawd.rectangle([xfctr*x,yfctr*y,xfctr*800,yfctr*(y+500)], width = rct_brdr, outline="#0000ff")
#
#x, y = (330, 1100)
#text = "Data to L1 from memory (L2+L3)"
#w, h = font.getsize(text)
#drawd.rectangle((x, y, x + w + 2*rct_brdr, y + h), fill='black')
#drawd.text((x+rct_brdr, y), text, fill='white', font=font)
#drawd.rectangle([x,y,x+600,y+280], width = rct_brdr, outline="#0000ff")
#
#imgd.save('b_test.png')

imgd = Image.open(pdir+"pat_base.png")
drawd = ImageDraw.Draw(imgd)
rct_brdr = 10
x, y = (10, 50 - fsz)
text = "CPU front end: up to 2 instructions/cycle from L2 or memory for execution. 3 stages"
w, h = font.getsize(text)
drawd.rectangle((xfctr*x, yfctr*y, xfctr*(x) + w + 2*rct_brdr, yfctr*(y + h)), fill='black')
drawd.text((xfctr*(x+rct_brdr), yfctr*(y)), text, fill='white', font=font)
drawd.rectangle([xfctr*(x),yfctr*(y),xfctr*(szx-10),yfctr*(630)], width = rct_brdr, outline="#0000ff")

x, y = (10, 620)
text = "CPU execution engine: Decodes up to 2 inst/cycle issued to ports.   5-7 stages"
w, h = font.getsize(text)
drawd.rectangle((xfctr*(x), yfctr*(y), xfctr*(x + w + 2*rct_brdr), yfctr*(y + h)), fill='black')
drawd.text((xfctr*(x+rct_brdr), yfctr*(y+4)), text, fill='white', font=font)
drawd.rectangle([xfctr*(x),yfctr*(y),xfctr*(870),yfctr*(1080)], width = rct_brdr, outline="#0000ff")

x, y = (400, 1080)
text = "Data to L1 to/from L2 or memory"
w, h = font.getsize(text)
drawd.rectangle((xfctr*(x), yfctr*(y), xfctr*(x + w + 2*rct_brdr), yfctr*(y + h)), fill='black')
drawd.text((xfctr*(x+rct_brdr), yfctr*(y)), text, fill='white', font=font)
drawd.rectangle([xfctr*(x),yfctr*(y),xfctr*(1025),yfctr*(1322)], width = rct_brdr, outline="#0000ff")

imgd.save('c_test.png')

imgd = Image.open(pdir+"pat_base.png")
drawd = ImageDraw.Draw(imgd)
rct_brdr = 10
x, y = (0, 0)
text = "CPU block diagram: shows resource constraints between blocks"
w, h = font.getsize(text)
drawd.rectangle((x, y, x + w + 2*rct_brdr, y + h), fill='black')
drawd.text((x+rct_brdr, y), text, fill='white', font=font)

x, y = (0, y+h)
text = "in terms of instructions, UOPS or bytes per cycle"
w, h = font.getsize(text)
drawd.rectangle((x, y, x + w + 2*rct_brdr, y + h), fill='black')
drawd.text((x+rct_brdr, y), text, fill='white', font=font)

drawd.rectangle([405,135,595, 179], width = rct_brdr, outline="#0000ff") # l1i->ifu
drawd.rectangle([925,148,1037,285], width = rct_brdr, outline="#0000ff") # l1i->L2
drawd.rectangle([370,260,435,305], width = rct_brdr, outline="#0000ff") # ifu->decoder
drawd.rectangle([595,260,650,305], width = rct_brdr, outline="#0000ff") # ifu->decoder
drawd.rectangle([341,783,670,816], width = rct_brdr, outline="#0000ff")
drawd.rectangle([225,902,834,938], width = rct_brdr, outline="#0000ff")
drawd.rectangle([450,986,660,1046], width = rct_brdr, outline="#0000ff")
drawd.rectangle([904,1080,1012,1310], width = rct_brdr, outline="#0000ff")
drawd.rectangle([1046,876,1150,945], width = rct_brdr, outline="#0000ff")
imgd.save('d_test.png')

imgd = Image.open(pdir+"pat00000.png")
drawd = ImageDraw.Draw(imgd)
rct_brdr = 10
x, y = (0, 0)
text = "OPPAT also shows stalls due to resource saturation such as front end stalls (due to"
w, h = font.getsize(text)
drawd.rectangle((x, y, x + w + 2*rct_brdr, y + h), fill='black')
drawd.text((x+rct_brdr, y), text, fill='white', font=font)

x, y = (0, y+h)
text = "no instructions), back-end stalls (load/store stalls, address gen interlocks, simd stalls)"
w, h = font.getsize(text)
drawd.rectangle((x, y, x + w + 2*rct_brdr, y + h), fill='black')
drawd.text((x+rct_brdr, y), text, fill='white', font=font)


drawd.rectangle([  0,137,539,545], width = rct_brdr, outline="#0000ff")
drawd.rectangle([679,140,875,217], width = rct_brdr, outline="#0000ff")
drawd.rectangle([988,371,1144,454], width = rct_brdr, outline="#0000ff")
drawd.rectangle([ 11,910,268,1051], width = rct_brdr, outline="#0000ff")
drawd.rectangle([460,913,818,965], width = rct_brdr, outline="#0000ff")
drawd.rectangle([590,1117,763,1182], width = rct_brdr, outline="#0000ff")
imgd.save('e_test.png')

imgd = Image.open(pdir+"pat00000.png")
drawd = ImageDraw.Draw(imgd)
rct_brdr = 10
x, y = (0, 0 )
text = "OPPAT: system info: %busy, freq, T, power, disk & mem bw"
w, h = font.getsize(text)
drawd.rectangle((x, y, x + w + 2*rct_brdr, y + h), fill='black')
drawd.text((x+rct_brdr, y), text, fill='white', font=font)

drawd.rectangle([0,y+h,360, 170], width = rct_brdr, outline="#0000ff")

x, y = (0, 250 )
text = "Display begin/end "+bmarkf+" sub-benchmark for interval, score, metric"
w, h = font.getsize(text)
drawd.rectangle((x, y, x + w + 2*rct_brdr, y + h), fill='black')
drawd.text((x+rct_brdr, y), text, fill='white', font=font)
drawd.rectangle([0,288,360, 350], width = rct_brdr, outline="#0000ff")

x, y = (szx, 822 )
text = "L2 miss, L3 miss, %Peak mem BW, Super Queue full"
w, h = font.getsize(text)
drawd.rectangle((x-w-2*rct_brdr, y, x, y + h), fill='black')
drawd.text((x-w-rct_brdr, y), text, fill='white', font=font)
drawd.rectangle([800,y+h,1030,1150], width = rct_brdr, outline="#0000ff")

imgd.save('f_test.png')

imgd = Image.open(pdir+"pat00000.png")
drawd = ImageDraw.Draw(imgd)
rct_brdr = 10
x, y = (0, 0 )
fsz = 50
sz = fsz + 4
font = ImageFont.truetype("/Program Files/Microsoft Office 15/root/vfs/Fonts/private/arialn.ttf", fsz)
text = bmark+" on ARM Cortex A53 - the movie"
w, h = font.getsize(text)
x = szx/2 - w/2 - 100
y = szy/10
drawd.rectangle((x, y, x + w + 2*rct_brdr, y + h+rct_brdr), fill='black')
drawd.text((x+rct_brdr, y), text, fill='white', font=font)

text = "Open Power/Performance Analysis Tool"
w, h = font.getsize(text)
x = szx/2 - w/2 - 100
y = 2*szy/10
drawd.rectangle((x, y, x + w + 2*rct_brdr, y + h+rct_brdr), fill='black')
drawd.text((x+rct_brdr, y), text, fill='white', font=font)

text = "See https://patinnc.github.io/"
w, h = font.getsize(text)
x = szx/2 - w/2 - 100
y = 3*szy/10
drawd.rectangle((x, y, x + w + 2*rct_brdr, y + h+rct_brdr), fill='black')
drawd.text((x+rct_brdr, y), text, fill='white', font=font)

#draw.text((offx, gety(row)), "Open Power/Performance Analysis Tool", (0,0,0), font=font)
#draw.text((offx, gety(row)), "See https://patinnc.github.io/", (0,0,0), font=font)
imgd.save('t_test.png')

addList = ("t_test.png", "a_test.png", "c_test.png", "d_test.png", "e_test.png", "f_test.png")
whch = -1
#for j in range(len(addList)-1, -1, -1):
#    whch += 1
#    for i in range(fpsn):
#        fileList.insert(0, addList[j])

i = 0
for im in addList:
    img_dta = imageio.imread(im)
    for i in range(fpsn):
        writer.append_data(img_dta)

if gb == 1:
    i = 0
    for im in fileList:
        #print("im[%d]= %s" % (i, im))
        img_dta = imageio.imread(im)
        #if 1==20 and i > 100:
            #print(img_dta.shape)
            #else:
            #break
        i += 1
        writer.append_data(img_dta)
    #img_dta = imageio.imread("t_test.png")
    img_dta = imageio.imread(fileList[-1])
    writer.append_data(img_dta)
    img_dta = imageio.imread("t_test.png")
    writer.append_data(img_dta)
    img_dta = imageio.imread("t_test.png")
    writer.append_data(img_dta)
    writer.close()
    sys.exit(0)


#img_dta = imageio.imread(fileList[0])
imgd = Image.open(fileList[0])
drawd = ImageDraw.Draw(imgd)
rct_brdr = 10
x, y = (0, 0 )
text = "spin: 1st subtest is read memory BW test"
w, h = font.getsize(text)
drawd.rectangle((x, y, x + w + 2*rct_brdr, y + h), fill='black')
drawd.text((x+rct_brdr, y), text, fill='white', font=font)

drawd.rectangle([0,y+h,360, 170], width = rct_brdr, outline="#0000ff")

x, y = (0, 288-fsz)
text = "Display "+bmarkf+" sub-benchmark for interval, BW"
w, h = font.getsize(text)
drawd.rectangle((x, y, x + w + 2*rct_brdr, y + h), fill='black')
drawd.text((x+rct_brdr, y), text, fill='white', font=font)
#drawd.rectangle([0,288,400, 350], width = rct_brdr, outline="#0000ff")

x, y = (szx, 822 )
text = "L2 miss, L3 miss, 73% Peak BW, SQueue 54% full"
w, h = font.getsize(text)
drawd.rectangle((x-w-2*rct_brdr, y, x, y + h), fill='black')
drawd.text((x-w-rct_brdr, y), text, fill='white', font=font)
drawd.rectangle([800,y+h,1030,1150], width = rct_brdr, outline="#0000ff")
imgd.save('test.png')
img_dta = imageio.imread('test.png')
writer.append_data(img_dta)

x, y = (0, 623-fsz)
text = "Stalls: due to resource saturation/starvation:"
w, h = font.getsize(text)
drawd.rectangle((x, y, x + w + 2*rct_brdr, y + h), fill='black')
drawd.text((x+rct_brdr, y), text, fill='white', font=font)

#x, y = (0, y+h)
#text = "no fill buffers, no store buffers or no reserveration station entries"
#w, h = font.getsize(text)
#drawd.rectangle((x, y, x + w + 2*rct_brdr, y + h), fill='black')
#drawd.text((x+rct_brdr, y), text, fill='white', font=font)


#drawd.rectangle([350,140,480, 190], width = rct_brdr, outline="#0000ff")
drawd.rectangle([ 80,623,245, 720], width = rct_brdr, outline="#0000ff")
#drawd.rectangle([180,740,305, 790], width = rct_brdr, outline="#0000ff")
#drawd.rectangle([435,800,575, 850], width = rct_brdr, outline="#0000ff")
drawd.rectangle([905,1105,1015,1155], width = rct_brdr, outline="#0000ff")
#drawd.rectangle([650,1175,775,1230], width = rct_brdr, outline="#0000ff")
drawd.rectangle([680,1342,827,1400], width = rct_brdr, outline="#0000ff")
imgd.save('test.png')
img_dta = imageio.imread('test.png')
writer.append_data(img_dta)

imgd = Image.open(fileList[1])
drawd = ImageDraw.Draw(imgd)
rct_brdr = 10
x, y = (0, 0 )
text = "spin: 2nd subtest is L3 read BW test"
w, h = font.getsize(text)
drawd.rectangle((x, y, x + w + 2*rct_brdr, y + h), fill='black')
drawd.text((x+rct_brdr, y), text, fill='white', font=font)

#drawd.rectangle([0,y+h,360, 170], width = rct_brdr, outline="#0000ff")

x, y = (0, 288-fsz)
text = "Display "+bmarkf+" sub-benchmark for interval, BW"
w, h = font.getsize(text)
drawd.rectangle((x, y, x + w + 2*rct_brdr, y + h), fill='black')
drawd.text((x+rct_brdr, y), text, fill='white', font=font)
#drawd.rectangle([0,288,400, 350], width = rct_brdr, outline="#0000ff")

x, y = (szx, 850 )
text = "Hi L2miss byte/cyc, lo L3 miss & memBW, SQ 29% full"
w, h = font.getsize(text)
drawd.rectangle((x-w-2*rct_brdr, y, x, y + h), fill='black')
drawd.text((x-w-rct_brdr, y), text, fill='white', font=font)
drawd.rectangle([800,y+h,1030,1150], width = rct_brdr, outline="#0000ff")
imgd.save('test.png')
img_dta = imageio.imread('test.png')
writer.append_data(img_dta)

x, y = (0, 623-fsz)
text = "Stalls: SQ, FB, RAT_uops/cycle, hi L1<->L2 traffic"
w, h = font.getsize(text)
drawd.rectangle((x, y, x + w + 2*rct_brdr, y + h), fill='black')
drawd.text((x+rct_brdr, y), text, fill='white', font=font)

#x, y = (0, y+h)
#text = "no fill buffers, no store buffers or no reserveration station entries"
#w, h = font.getsize(text)
#drawd.rectangle((x, y, x + w + 2*rct_brdr, y + h), fill='black')
#drawd.text((x+rct_brdr, y), text, fill='white', font=font)


#drawd.rectangle([350,140,480, 190], width = rct_brdr, outline="#0000ff")
drawd.rectangle([ 80,623,245, 720], width = rct_brdr, outline="#0000ff")
#drawd.rectangle([180,740,305, 790], width = rct_brdr, outline="#0000ff")
#drawd.rectangle([435,800,575, 850], width = rct_brdr, outline="#0000ff")
drawd.rectangle([905,1105,1015,1155], width = rct_brdr, outline="#0000ff")
#drawd.rectangle([650,1175,775,1230], width = rct_brdr, outline="#0000ff")
drawd.rectangle([680,1342,827,1400], width = rct_brdr, outline="#0000ff")
imgd.save('test.png')
img_dta = imageio.imread('test.png')

imgd = Image.open(fileList[2])
drawd = ImageDraw.Draw(imgd)
rct_brdr = 10
x, y = (0, 0 )
text = "spin: 3nd subtest is L2 read BW test"
w, h = font.getsize(text)
drawd.rectangle((x, y, x + w + 2*rct_brdr, y + h), fill='black')
drawd.text((x+rct_brdr, y), text, fill='white', font=font)

#drawd.rectangle([0,y+h,360, 170], width = rct_brdr, outline="#0000ff")

x, y = (0, 288-fsz)
text = "Display "+bmarkf+" sub-benchmark for interval, BW"
w, h = font.getsize(text)
drawd.rectangle((x, y, x + w + 2*rct_brdr, y + h), fill='black')
drawd.text((x+rct_brdr, y), text, fill='white', font=font)
#drawd.rectangle([0,288,400, 350], width = rct_brdr, outline="#0000ff")

x, y = (szx, 850 )
text = "lo L2miss, L3 miss, memBW, SQ %full"
w, h = font.getsize(text)
drawd.rectangle((x-w-2*rct_brdr, y, x, y + h), fill='black')
drawd.text((x-w-rct_brdr, y), text, fill='white', font=font)
drawd.rectangle([800,y+h,1030,1150], width = rct_brdr, outline="#0000ff")
imgd.save('test.png')
img_dta = imageio.imread('test.png')
writer.append_data(img_dta)

x, y = (0, 623-fsz)
text = "Stalls: hi FB, better RATuops/c, hi L1<->L2 traffic"
w, h = font.getsize(text)
drawd.rectangle((x, y, x + w + 2*rct_brdr, y + h), fill='black')
drawd.text((x+rct_brdr, y), text, fill='white', font=font)

#x, y = (0, y+h)
#text = "no fill buffers, no store buffers or no reserveration station entries"
#w, h = font.getsize(text)
#drawd.rectangle((x, y, x + w + 2*rct_brdr, y + h), fill='black')
#drawd.text((x+rct_brdr, y), text, fill='white', font=font)


#drawd.rectangle([350,140,480, 190], width = rct_brdr, outline="#0000ff")
drawd.rectangle([ 80,623,245, 720], width = rct_brdr, outline="#0000ff")
#drawd.rectangle([180,740,305, 790], width = rct_brdr, outline="#0000ff")
#drawd.rectangle([435,800,575, 850], width = rct_brdr, outline="#0000ff")
#drawd.rectangle([905,1105,1015,1155], width = rct_brdr, outline="#0000ff")
#drawd.rectangle([650,1175,775,1230], width = rct_brdr, outline="#0000ff")
drawd.rectangle([680,1342,827,1400], width = rct_brdr, outline="#0000ff")
imgd.save('test.png')
img_dta = imageio.imread('test.png')
writer.append_data(img_dta)

imgd = Image.open(fileList[3])
drawd = ImageDraw.Draw(imgd)
rct_brdr = 10
x, y = (0, 0 )
text = "spin: 4th subtest is spin (add loop) test"
w, h = font.getsize(text)
drawd.rectangle((x, y, x + w + 2*rct_brdr, y + h), fill='black')
drawd.text((x+rct_brdr, y), text, fill='white', font=font)

#drawd.rectangle([0,y+h,360, 170], width = rct_brdr, outline="#0000ff")

x, y = (0, 288-fsz)
text = "Display "+bmarkf+" sub-benchmark for interval, BW"
w, h = font.getsize(text)
drawd.rectangle((x, y, x + w + 2*rct_brdr, y + h), fill='black')
drawd.text((x+rct_brdr, y), text, fill='white', font=font)
#drawd.rectangle([0,288,400, 350], width = rct_brdr, outline="#0000ff")

x, y = (szx, 850 )
text = "lo L2miss, L3 miss, memBW, SQ %full"
w, h = font.getsize(text)
drawd.rectangle((x-w-2*rct_brdr, y, x, y + h), fill='black')
drawd.text((x-w-rct_brdr, y), text, fill='white', font=font)
drawd.rectangle([800,y+h,1030,1150], width = rct_brdr, outline="#0000ff")
imgd.save('test.png')
img_dta = imageio.imread('test.png')
writer.append_data(img_dta)

x, y = (0, 623-fsz)
text = "Stalls: lo FB, RAT, L1/L2 use; RATuops/cyc ~4 max pos"
w, h = font.getsize(text)
drawd.rectangle((x, y, x + w + 2*rct_brdr, y + h), fill='black')
drawd.text((x+rct_brdr, y), text, fill='white', font=font)

#x, y = (0, y+h)
#text = "no fill buffers, no store buffers or no reserveration station entries"
#w, h = font.getsize(text)
#drawd.rectangle((x, y, x + w + 2*rct_brdr, y + h), fill='black')
#drawd.text((x+rct_brdr, y), text, fill='white', font=font)

x, y = (0, 720)
text = "RATuops/cyc is 3.3, close to ~4 max uops/cyc"
w, h = font.getsize(text)
drawd.rectangle((x, y, x + w + 2*rct_brdr, y + h), fill='black')
drawd.text((x+rct_brdr, y), text, fill='white', font=font)

#drawd.rectangle([350,140,480, 190], width = rct_brdr, outline="#0000ff")
drawd.rectangle([ 80,623,345, 720], width = rct_brdr, outline="#0000ff")
#drawd.rectangle([180,740,305, 790], width = rct_brdr, outline="#0000ff")
#drawd.rectangle([435,800,575, 850], width = rct_brdr, outline="#0000ff")
#drawd.rectangle([905,1105,1015,1155], width = rct_brdr, outline="#0000ff")
#drawd.rectangle([650,1175,775,1230], width = rct_brdr, outline="#0000ff")
#drawd.rectangle([680,1342,827,1400], width = rct_brdr, outline="#0000ff")
imgd.save('test.png')
img_dta = imageio.imread('test.png')
writer.append_data(img_dta)

writer.close()

