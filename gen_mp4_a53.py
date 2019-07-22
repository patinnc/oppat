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
pg_txt = [
"The performance data is from a single run of "+bmarkf+" 32bit",
"on a 4 core ARM Cortex A53 (raspberry pi 3 b+).",
"The OS is 64bit Ubuntu Mate 18.04. Geekbench4 isn't available",

"for ARM Linux so I used a 32bit v2.4.2 ARM build. See",
"http://geekbench.s3.amazonaws.com/geekbench-2.4.2-LinuxARM.tar.gz",
"The A53 is a 4 core, in-order, 8 stage pipeline with 512KB shared L2.",
"The cpu diagram shows resource constraints on A53 such as",
"the max 2 instructions/cycle from the Instruction Fetch Unit and pathway bytes/cycle.",
"OPPAT shows resource utilization numerically and with a simple",
"red/yellow/green light for each core. This lets you see where the pipeline",
"is stalled (frontend or backend L2 or memory, buffers, interlocks, etc.",
"Data is collected using Linux perf stat/record, trace-cmd and custom utils.",
"Perf collects data for the 50+ hw events, frace and flamegraphs.",
"GB phase is used but no power or temperature is avail.",
"OPPAT can collect/analyze data on:",
" Linux/Android - perf, trace-cmd, sysfs, other",
" windows - ETW, PCM, other",
"",
"This movie shows average value for each phase.",
"OPPAT calculates when the 'multi-core' section of each phase begins.",
"The single core values are harder to interpret since GB can change cores.",
"",
"On windows the movie is best viewed vertically (ctrl+alt+right_arrow)",
"Do ctrl+alt+up_arrow to return to normal mode.",
"contact patrick.99.fay@gmail.com",
"Block diagram derived from WikiChip.org",
]

for i in range(0, len(pg_txt)):
    draw.text((offx, gety(row)), pg_txt[i], (0,0,0), font=font)
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

fsz = int(yfctr*30)
sz = fsz + int(yfctr*4)
font = ImageFont.truetype("/Program Files/Microsoft Office 15/root/vfs/Fonts/private/arialn.ttf", fsz)

imgd = Image.open(pdir+"pat_base.png")
drawd = ImageDraw.Draw(imgd)
rct_brdr = 5
y_shift = 0

x, y = (0, 0)
text = "Operating System View: App SW->System calls->(Virtual File System, Sockets, Scheduler, Virtual Mem)"
w, h = font.getsize(text)
drawd.rectangle((x,       y,    x + w + 2*rct_brdr, (y + h)), fill='black')
drawd.text((x+rct_brdr, y), text, fill='white', font=font)
drawd.rectangle([x,     y,  szx,             780], width = rct_brdr, outline="#0000ff")

fsz = int(yfctr*20)
sz = fsz + int(yfctr*4)
font = ImageFont.truetype("/Program Files/Microsoft Office 15/root/vfs/Fonts/private/arialn.ttf", fsz)
# y = 40 - 150
x, y = (2*rct_brdr, 40)
text = "App Software: Flamegraph for each thread. Hover shows callstack. Click goes to underlaying chart"
w, h = font.getsize(text)
tx_end = szx
drawd.rectangle((x+rct_brdr,     y,    x + w, y + h), fill='black')
drawd.text(     (x+rct_brdr, y), text, fill='white', font=font)
drawd.rectangle([x,     y,        szx-2*rct_brdr, 150], width = rct_brdr, outline="#0000ff")

x, y = (2*rct_brdr, 160)
text = "App Software calls into System calls: syscalls outstanding, non-blocking & total."
w, h = font.getsize(text)
tx_end = szx
drawd.rectangle((x+rct_brdr,     y,    x + w, y + h), fill='black')
drawd.text(     (x+rct_brdr, y), text, fill='white', font=font)
drawd.rectangle([x,     y,        szx-2*rct_brdr, 270], width = rct_brdr, outline="#0000ff")

x, y = (2*rct_brdr, 280)
text = "Virt.File Sys:app syscall rd+wr,swap,block,dev IO"
w, h = font.getsize(text)
tx_end = szx
drawd.rectangle((x+rct_brdr,     y,    x + w+rct_brdr, y + h), fill='black')
drawd.text(     (x+rct_brdr, y), text, fill='white', font=font)
drawd.rectangle([x,     y,        380, 770], width = rct_brdr, outline="#0000ff")

x, y = (390, 280)
text = "Sockets:app socket syscalls,TCP,UDP,device"
w, h = font.getsize(text)
tx_end = szx
drawd.rectangle((x+rct_brdr,     y,    x + w+rct_brdr, y + h), fill='black')
drawd.text(     (x+rct_brdr, y), text, fill='white', font=font)
drawd.rectangle([x,     y,        745, 770], width = rct_brdr, outline="#0000ff")

x, y = (753, 280)
text = "Scheduler, virtual memory usage and faults"
w, h = font.getsize(text)
tx_end = szx
drawd.rectangle((x+rct_brdr,     y,    x + w+rct_brdr, y + h), fill='black')
drawd.text(     (x+rct_brdr, y), text, fill='white', font=font)
drawd.rectangle([x,     y,        szx-2*rct_brdr, 770], width = rct_brdr, outline="#0000ff")

imgd.save('b_test.png')

y_shift = 800
fsz = int(yfctr*35)
sz = fsz + int(yfctr*4)
font = ImageFont.truetype("/Program Files/Microsoft Office 15/root/vfs/Fonts/private/arialn.ttf", fsz)

imgd = Image.open(pdir+"pat_base.png")
drawd = ImageDraw.Draw(imgd)
rct_brdr = 10
x, y = (10, 50 - fsz)
text = "CPU front end: up to 2 instructions/cycle from L2 or memory for execution. 3 stages"
w, h = font.getsize(text)
drawd.rectangle((xfctr*x,       y_shift+yfctr*y,    xfctr*(x) + w + 2*rct_brdr, y_shift+yfctr*(y + h)), fill='black')
drawd.text((xfctr*(x+rct_brdr), y_shift+yfctr*(y)), text, fill='white', font=font)
drawd.rectangle([xfctr*(x),     y_shift+yfctr*(y),  xfctr*(szx-10),             y_shift+yfctr*(630)], width = rct_brdr, outline="#0000ff")

x, y = (10, 620)
text = "CPU execution engine: Decodes up to 2 inst/cycle issued to ports.   5-7 stages"
w, h = font.getsize(text)
drawd.rectangle((xfctr*(x),     y_shift+yfctr*(y),  xfctr*(x + w + 2*rct_brdr), y_shift+yfctr*(y + h)), fill='black')
drawd.text((xfctr*(x+rct_brdr), y_shift+yfctr*(y+4)), text, fill='white', font=font)
drawd.rectangle([xfctr*(x),     y_shift+yfctr*(y),  xfctr*(870),                y_shift+yfctr*(1080)], width = rct_brdr, outline="#0000ff")

x, y = (400, 1080)
text = "Data to L1 to/from L2 or memory"
w, h = font.getsize(text)
drawd.rectangle((xfctr*(x),     y_shift+yfctr*(y), xfctr*(x + w + 2*rct_brdr), y_shift+yfctr*(y + h)), fill='black')
drawd.text((xfctr*(x+rct_brdr), y_shift+yfctr*(y)), text, fill='white', font=font)
drawd.rectangle([xfctr*(x),     y_shift+yfctr*(y), xfctr*(1025),               y_shift+yfctr*(1322)], width = rct_brdr, outline="#0000ff")

imgd.save('c_test.png')

imgd = Image.open(pdir+"pat_base.png")
drawd = ImageDraw.Draw(imgd)
rct_brdr = 10
x, y = (0, 0)
text = "CPU block diagram: shows resource constraints between blocks"
w, h = font.getsize(text)
drawd.rectangle((x,     y_shift+y, x + w + 2*rct_brdr, y_shift+y + h), fill='black')
drawd.text((x+rct_brdr, y_shift+y), text, fill='white', font=font)

x, y = (0, y+h)
text = "in terms of instructions, UOPS or bytes per cycle"
w, h = font.getsize(text)
drawd.rectangle((x,     y_shift+y, x + w + 2*rct_brdr, y_shift+y + h), fill='black')
drawd.text((x+rct_brdr, y_shift+y), text, fill='white', font=font)

drawd.rectangle([405, y_shift+135,  595,y_shift+179], width = rct_brdr, outline="#0000ff") # l1i->ifu
drawd.rectangle([925, y_shift+148, 1037,y_shift+285], width = rct_brdr, outline="#0000ff") # l1i->L2
drawd.rectangle([370, y_shift+260,  435,y_shift+305], width = rct_brdr, outline="#0000ff") # ifu->decoder
drawd.rectangle([595, y_shift+260,  650,y_shift+305], width = rct_brdr, outline="#0000ff") # ifu->decoder
drawd.rectangle([341, y_shift+783,  670,y_shift+816], width = rct_brdr, outline="#0000ff")
drawd.rectangle([225, y_shift+902,  834,y_shift+938], width = rct_brdr, outline="#0000ff")
drawd.rectangle([450, y_shift+986,  660,y_shift+1046], width = rct_brdr, outline="#0000ff")
drawd.rectangle([904, y_shift+1080,1012,y_shift+1310], width = rct_brdr, outline="#0000ff")
drawd.rectangle([1046,y_shift+876, 1150,y_shift+945], width = rct_brdr, outline="#0000ff")
imgd.save('d_test.png')

imgd = Image.open(pdir+"pat00000.png")
drawd = ImageDraw.Draw(imgd)
rct_brdr = 10
x, y = (0, 0)
text = "OPPAT also shows stalls due to resource saturation such as front end stalls (due to"
w, h = font.getsize(text)
drawd.rectangle((x,     y_shift+y, x + w + 2*rct_brdr, y_shift+y + h), fill='black')
drawd.text((x+rct_brdr, y_shift+y), text, fill='white', font=font)

x, y = (0, y+h)
text = "no instructions), back-end stalls (load/store stalls, address gen interlocks, simd stalls)"
w, h = font.getsize(text)
drawd.rectangle((x,     y_shift+y, x + w + 2*rct_brdr, y_shift+y + h), fill='black')
drawd.text((x+rct_brdr, y_shift+y), text, fill='white', font=font)


drawd.rectangle([  0, y_shift+ 137, 539,y_shift+ 545], width = rct_brdr, outline="#0000ff")
drawd.rectangle([679, y_shift+ 140, 875,y_shift+ 217], width = rct_brdr, outline="#0000ff")
drawd.rectangle([988, y_shift+ 371,1144,y_shift+ 454], width = rct_brdr, outline="#0000ff")
drawd.rectangle([ 11, y_shift+ 910, 268,y_shift+1051], width = rct_brdr, outline="#0000ff")
drawd.rectangle([460, y_shift+ 913, 818,y_shift+ 965], width = rct_brdr, outline="#0000ff")
drawd.rectangle([590, y_shift+1117, 763,y_shift+1182], width = rct_brdr, outline="#0000ff")
imgd.save('e_test.png')

imgd = Image.open(pdir+"pat00000.png")
drawd = ImageDraw.Draw(imgd)
rct_brdr = 10
x, y = (0, 0 )
text = "OPPAT: system info: %busy, freq, T, power, disk & mem bw"
w, h = font.getsize(text)
drawd.rectangle((x,     y_shift+y, x + w + 2*rct_brdr, y_shift+y + h), fill='black')
drawd.text((x+rct_brdr, y_shift+y), text, fill='white', font=font)

drawd.rectangle([0,y_shift+y+h,360, y_shift+170], width = rct_brdr, outline="#0000ff")

x, y = (500, 487 )
text = "Display begin/end "+bmarkf+" sub-benchmark for interval, score, metric"
w, h = font.getsize(text)
drawd.rectangle((x,     y_shift+y, x + w + 2*rct_brdr, y_shift+y + h), fill='black')
drawd.text((x+rct_brdr, y_shift+y), text, fill='white', font=font)
drawd.rectangle([x,     y_shift+y,              x+ 700, y_shift+y+130], width = rct_brdr, outline="#0000ff")

x, y = (szx, 660 )
text = "Shared L2: traffic type, L2 miss, Mem BW"
w, h = font.getsize(text)
drawd.rectangle((x-w-2*rct_brdr, y_shift+y, x,    y_shift+y + h), fill='black')
drawd.text((x-w-rct_brdr,        y_shift+y), text, fill='white', font=font)
drawd.rectangle([850,            y_shift+y+h,1145,y_shift+y+h+600], width = rct_brdr, outline="#0000ff")

imgd.save('f_test.png')

row = [0, 10, 60]
row[1] += 30
img = Image.new("RGBA", (szx,szy), (255,255,255))
draw = ImageDraw.Draw(img)
fsz = int(yfctr*35)
sz = fsz + int(yfctr*4)
row[2] = sz
font = ImageFont.truetype("/Program Files/Microsoft Office 15/root/vfs/Fonts/private/arialn.ttf", fsz)
spin_pg_txt = [
"Begin spin.x phase charts.",
"Each chart is zoomed to (contains the only the data for) one phase.",
"To go directly to a particular phase, go to the T=X seconds picture.",
"For example, to go directly to the multi-core spin test (which is",
"just a busy loop), go to T=14 seconds.",
"",
"T= 10 phase: Multi-core memory Read Bandwidth, GB/sec= 2.103833",
"T= 11 phase: Multi-core memory Read/write Bandwidth, GB/sec= 1.843209",
"T= 12 phase: Multi-core L2 Read Bandwidth, GB/sec= 19.755875",
"T= 13 phase: Multi-core L2 Read/write Bandwidth, GB/sec= 19.172812",
"T= 14 phase: Multi-core spin (busy up), Gops/sec= 1.133419",
]
pg_txt = [
"Begin Geekbench phase charts.",
"Each chart is zoomed to (contains the only the data for) one phase.",
"To go directly to a particular phase, go to the T=X seconds picture.",
"For example, to see the multi-core Dot product, go to T=24 seconds.",
"",
"T= 10 phase: sngle-core: Blowfish, score:668, metric:29.4 MB/sec",
"T= 11 phase: multi-core: Blowfish score:2865 metric:117 MB/sec",
"T= 12 phase: sngle-core: Text Compress, score:719, metric:2.30 MB/sec",
"T= 13 phase: multi-core: Text Compress score:1752 metric:5.75 MB/sec",
"T= 14 phase: sngle-core: Text Decompress, score:457, metric:1.88 MB/sec",
"T= 15 phase: multi-core: Text Decompress score:963 metric:3.84 MB/sec",
"T= 16 phase: sngle-core: Image Compress, score:854, metric:7.06 Mpixels/sec",
"T= 17 phase: multi-core: Image Compress score:3332 metric:28.0 Mpixels/sec",
"T= 18 phase: multi-core: Image Decompress score:2610 metric:42.6 Mpixels/sec",
"T= 19 phase: sngle-core: Lua, score:1403, metric:540 Knodes/sec",
"T= 20 phase: multi-core: Lua score:5586 metric:2.15 Mnodes/sec",
"T= 21 phase: sngle-core: Mandelbrot, score:652, metric:434 Mflops",
"T= 22 phase: multi-core: Mandelbrot score:2645 metric:1.73 Gflops",
"T= 23 phase: sngle-core: Dot Product, score:1125, metric:544 Mflops",
"T= 24 phase: multi-core: Dot Product score:4762 metric:2.17 Gflops",
"T= 25 phase: sngle-core: LU Decomposition, score:264, metric:235 Mflops",
"T= 26 phase: multi-core: LU Decomposition score:410 metric:360 Mflops",
"T= 27 phase: sngle-core: Primality Test, score:1492, metric:223 Mflops",
"T= 28 phase: multi-core: Primality Test score:2134 metric:396 Mflops",
"T= 29 phase: sngle-core: Sharpen Image, score:2632, metric:6.14 Mpixels/sec",
"T= 30 phase: multi-core: Sharpen Image score:10486 metric:24.2 Mpixels/sec",
"T= 31 phase: sngle-core: Blur Image, score:3583, metric:2.84 Mpixels/sec",
"T= 32 phase: sngle-core: Read Sequential, score:1310, metric:1.60 GB/sec",
"T= 33 phase: sngle-core: Write Sequential, score:1782, metric:1.22 GB/sec",
"T= 34 phase: sngle-core: Stdlib Allocate, score:732, metric:2.73 Mallocs/sec",
"T= 35 phase: sngle-core: Stdlib Write, score:807, metric:1.67 GB/sec",
"T= 36 phase: sngle-core: Stdlib Copy, score:1646, metric:1.70 GB/sec",
"T= 37 phase: sngle-core: Stream Copy, score:1583, metric:2.17 GB/sec",
"T= 38 phase: sngle-core: Stream Scale, score:905, metric:1.18 GB/sec",
"T= 39 phase: sngle-core: Stream Add, score:870, metric:1.31 GB/sec",
"T= 40 phase: sngle-core: Stream Triad, score:705, metric:998 MB/sec",
]
if gb == 0:
    pg_txt = spin_pg_txt

for i in range(0, len(pg_txt)):
    draw.text((offx, gety(row)), pg_txt[i], (0,0,0), font=font)
draw = ImageDraw.Draw(img)
img.save("g_test.png")

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

addList = ("t_test.png", "a_test.png", "b_test.png", "c_test.png", "d_test.png", "e_test.png", "f_test.png", "g_test.png")
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

if gb == 0:
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
    # can't insert the dashboard png since it is a different size
    #img_dta = imageio.imread("pat.png")
    #writer.append_data(img_dta)
    img_dta = imageio.imread("t_test.png")
    writer.append_data(img_dta)
    img_dta = imageio.imread("t_test.png")
    writer.append_data(img_dta)
    writer.close()
    print("end code for spin ")
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

