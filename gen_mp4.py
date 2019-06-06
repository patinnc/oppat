import glob
import sys
import imageio

y_shift = 800
gb = 1
fpsn=10
fpsn=1
xold = 1017
yold = 1388
# old png x:1017 y:1388
# new png x: 912 y:1244
bmark = "Geekbench"
bmarkf = "Geekbench4"
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
    #print("row= %d" % (row[0]))
    return row[1]

img = Image.new("RGBA", (szx,szy), (255,255,255))
draw = ImageDraw.Draw(img)
fsz = 60
sz = fsz + int(yfctr*4)
font = ImageFont.truetype("/Program Files/Microsoft Office 15/root/vfs/Fonts/private/arialn.ttf", fsz)
row = [0, 10, fsz]
offx = 40
if gb == 1:
    txt = bmark+" - the movie, v4"
else:
    txt = "spin_test - the movie, v4"
pg_txt = [
    txt,
    "Created using OPPAT",
    "Open Power/Performance Analysis Tool",
    "See https://patinnc.github.io/",
]
for i in range(0, len(pg_txt)):
    draw.text((offx, gety(row)), pg_txt[i], (0,0,0), font=font)

pg_txt = [
    "The power/perf data is from a single run of "+bmarkf+" on a",
    "4 CPU (2 core) Intel Haswell CPU.",
    "The OS is 64bit Ubuntu 18.04. Geekbench ver is 4.3.3.",
    "",
    "The Operating System (OS) view gives a system view of performance",
    "as seen from the OS.",
    "Hovering over an OS_view chart shows the call graph or value.",
    "Clicking on a chart goes to the underlying chart.",
    "The OS_view top layer is the application software.",
    "This layer calls into the OS using the system calls.",
    "There are some main OS stacks shown by:",
    "  Virtual File System IO:",
    "      File system reads/writes, swap IO, block IO and device IO",
    "  Socket IO:",
    "      socket operations, TCP oper/sec, UDP oper/sec, device IO",
    "  Scheduler and Virtual memory:",
    "      Scheduling per CPU, virtual memory usage, hard/soft page faults",
    "Below the OS is the hardware including the CPU diagram",
    "",
    "Application software is shown using flamegraphs. If you hover over",
    "The cpu diagram shows resource constraints on Haswell such as",
    "the max UOPS/cycle (4) from the RAT. OPPAT calculates the current",
    "resource utilization for each resource and categorizes the usage",
    "with a simple red/yellow/green light for each core.",
    "This lets you see whether the system is stalled on memory",
    "L3, or L2 bandwidth, waiting on buffers, instruction misses, etc.",
    "",
    "Data is collected using linux perf stat/record, trace-cmd and",
    "custom utils. Perf collects data for the 50+ hw events, frace",
    "and flamegraphs. GB phase, system usage, power, T, disk bw is shown.",
    "OPPAT can collect/analyze data on:",
    " Linux/Android - perf, trace-cmd, sysfs, iostat, vmstat, iostat, other",
    " windows - ETW, PCM, other",
    "",
    "This movie shows the values of a bunch of data for each",
    "Geekbench sub-test. OPPAT tries to show just the non-idle part",
    "of each sub-test (eliminating the idle period after each sub-test).",
    "",
    "On windows the movie is best viewed vertically (ctrl+alt+right_arrow)",
    "Do ctrl+alt+up_arrow to return to normal mode.",
    "contact patrick.99.fay@gmail.com",
    "",
    "Block diagram courtesy of WikiChip.org",
]
fsz = int(yfctr*35)
sz = fsz + int(yfctr*4)
font = ImageFont.truetype("/Program Files/Microsoft Office 15/root/vfs/Fonts/private/arialn.ttf", fsz)
row[1] += 30
row[2] = sz
for i in range(0, len(pg_txt)):
    draw.text((offx, gety(row)), pg_txt[i], (0,0,0), font=font)

print("y last= %d" % (gety(row)))
draw = ImageDraw.Draw(img)
img.save("a_test.png")

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

fsz = int(yfctr*24)
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

fsz = int(yfctr*22)
sz = fsz + int(yfctr*4)
font = ImageFont.truetype("/Program Files/Microsoft Office 15/root/vfs/Fonts/private/arialn.ttf", fsz)
x, y = (2*rct_brdr, 280)
text = "Virtual File System"
w, h = font.getsize(text)
tx_end = szx
drawd.rectangle((x+rct_brdr,     y,    x + w+rct_brdr, y + h), fill='black')
drawd.text(     (x+rct_brdr, y), text, fill='white', font=font)
text = "app syscall rd+wr,swap,block,dev IO"
w2, h2 = font.getsize(text)
drawd.rectangle((x+rct_brdr,     y+h,    x + w2+rct_brdr, y + h + h2), fill='black')
drawd.text(     (x+rct_brdr, y+h), text, fill='white', font=font)
drawd.rectangle([x,     y,        335, 770], width = rct_brdr, outline="#0000ff")

x, y = (340, 280)
text = "Sockets:"
w, h = font.getsize(text)
tx_end = szx
drawd.rectangle((x+rct_brdr,     y,    x + w+rct_brdr, y + h), fill='black')
drawd.text(     (x+rct_brdr, y), text, fill='white', font=font)
text = "app socket syscalls,TCP,UDP,device"
w2, h2 = font.getsize(text)
drawd.rectangle((x+rct_brdr,     y+h,    x + w2+rct_brdr, y + h + h2), fill='black')
drawd.text(     (x+rct_brdr, y+h), text, fill='white', font=font)
drawd.rectangle([x,     y,        660, 770], width = rct_brdr, outline="#0000ff")

x, y = (670, 280)
text = "Scheduler,"
w, h = font.getsize(text)
tx_end = szx
drawd.rectangle((x+rct_brdr,     y,    x + w+rct_brdr, y + h), fill='black')
drawd.text(     (x+rct_brdr, y), text, fill='white', font=font)
text = "virtual memory usage and page faults"
w2, h2 = font.getsize(text)
drawd.rectangle((x+rct_brdr,     y+h,    x + w2+rct_brdr, y + h + h2), fill='black')
drawd.text(     (x+rct_brdr, y+h), text, fill='white', font=font)
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
text = "CPU front end: instructions from memory (L2+L3) -> UOPS for execution"
w, h = font.getsize(text)
drawd.rectangle((xfctr*x,       y_shift+yfctr*y, xfctr*(x) + w + 2*rct_brdr, y_shift+yfctr*(y + h)), fill='black')
drawd.text((xfctr*(x+rct_brdr), y_shift+yfctr*(y)), text, fill='white', font=font)
drawd.rectangle([xfctr*(x),     y_shift+yfctr*(y),xfctr*(szx-10),            y_shift+yfctr*(630)], width = rct_brdr, outline="#0000ff")

x, y = (10, 620)
text = "CPU execution engine: UOPS+data execute in ports"
w, h = font.getsize(text)
drawd.rectangle((xfctr*(x),     y_shift+yfctr*(y), xfctr*(x + w + 2*rct_brdr), y_shift+yfctr*(y + h)), fill='black')
drawd.text((xfctr*(x+rct_brdr), y_shift+yfctr*(y+4)), text, fill='white', font=font)
drawd.rectangle([xfctr*(x),     y_shift+yfctr*(y),xfctr*(800),                 y_shift+yfctr*(y+500)], width = rct_brdr, outline="#0000ff")

x, y = (330, 1100)
text = "Data to L1 from memory (L2+L3)"
w, h = font.getsize(text)
drawd.rectangle((xfctr*(x),     y_shift+yfctr*(y), xfctr*(x + w + 2*rct_brdr), y_shift+yfctr*(y + h)), fill='black')
drawd.text((xfctr*(x+rct_brdr), y_shift+yfctr*(y)), text, fill='white', font=font)
drawd.rectangle([xfctr*(x),     y_shift+yfctr*(y), xfctr*(x+600),              y_shift+yfctr*(y+280)], width = rct_brdr, outline="#0000ff")

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
text = "in terms of UOPS/cycle or bytes/cycle"
w, h = font.getsize(text)
drawd.rectangle((x,     y_shift+y, x + w + 2*rct_brdr, y_shift+y + h), fill='black')
drawd.text((x+rct_brdr, y_shift+y), text, fill='white', font=font)

drawd.rectangle([270,   y_shift+480,345, y_shift+530], width = rct_brdr, outline="#0000ff")
drawd.rectangle([240,   y_shift+650,335, y_shift+710], width = rct_brdr, outline="#0000ff")
drawd.rectangle([850,   y_shift+1150,950,y_shift+1270], width = rct_brdr, outline="#0000ff")
imgd.save('d_test.png')

imgd = Image.open(pdir+"pat00000.png")
drawd = ImageDraw.Draw(imgd)
rct_brdr = 10
x, y = (0, 0)
text = "OPPAT also shows stalls due to resource saturation such as"
w, h = font.getsize(text)
drawd.rectangle((x,     y_shift+y, x + w + 2*rct_brdr, y_shift+y + h), fill='black')
drawd.text((x+rct_brdr, y_shift+y), text, fill='white', font=font)

x, y = (0, y+h)
text = "no fill buffers, no store buffers or no reserveration station entries"
w, h = font.getsize(text)
drawd.rectangle((x,     y_shift+y, x + w + 2*rct_brdr, y_shift+y + h), fill='black')
drawd.text((x+rct_brdr, y_shift+y), text, fill='white', font=font)


drawd.rectangle([350,y_shift+140, 480, y_shift+190], width = rct_brdr, outline="#0000ff")
drawd.rectangle([ 80,y_shift+623, 245, y_shift+680], width = rct_brdr, outline="#0000ff")
drawd.rectangle([180,y_shift+740, 305, y_shift+790], width = rct_brdr, outline="#0000ff")
drawd.rectangle([435,y_shift+800, 575, y_shift+850], width = rct_brdr, outline="#0000ff")
drawd.rectangle([905,y_shift+1105,1015,y_shift+1155], width = rct_brdr, outline="#0000ff")
drawd.rectangle([650,y_shift+1175, 775,y_shift+1230], width = rct_brdr, outline="#0000ff")
drawd.rectangle([680,y_shift+1342, 827,y_shift+1400], width = rct_brdr, outline="#0000ff")
imgd.save('e_test.png')

imgd = Image.open(pdir+"pat00000.png")
drawd = ImageDraw.Draw(imgd)
rct_brdr = 10
x, y = (0, 0 )
text = "OPPAT: system info: %busy, freq, T, power, disk & mem bw"
w, h = font.getsize(text)
drawd.rectangle((x,     y_shift+y, x + w + 2*rct_brdr, y_shift+y + h), fill='black')
drawd.text((x+rct_brdr, y_shift+y), text, fill='white', font=font)

drawd.rectangle([0,     y_shift+y+h,360, y_shift+170], width = rct_brdr, outline="#0000ff")

x, y = (0, 250 )
text = "Display begin/end "+bmarkf+" sub-benchmark for interval, score, metric"
w, h = font.getsize(text)
drawd.rectangle((x,     y_shift+y, x + w + 2*rct_brdr, y_shift+y + h), fill='black')
drawd.text((x+rct_brdr, y_shift+y), text, fill='white', font=font)
drawd.rectangle([0,     y_shift+288,360,               y_shift+350], width = rct_brdr, outline="#0000ff")

x, y = (szx, 822 )
text = "L2 miss, L3 miss, %Peak mem BW, Super Queue full"
w, h = font.getsize(text)
drawd.rectangle((x-w-2*rct_brdr, y_shift+y, x,     y_shift+y + h), fill='black')
drawd.text((x-w-rct_brdr,        y_shift+y), text, fill='white', font=font)
drawd.rectangle([800,            y_shift+y+h,1030, y_shift+1150], width = rct_brdr, outline="#0000ff")

imgd.save('f_test.png')

row = [0, 10, 60]
row[1] += 30
img = Image.new("RGBA", (szx,szy), (255,255,255))
draw = ImageDraw.Draw(img)
fsz = int(yfctr*40)
sz = fsz + int(yfctr*4)
row[2] = sz
font = ImageFont.truetype("/Program Files/Microsoft Office 15/root/vfs/Fonts/private/arialn.ttf", fsz)
pg_txt = [
"Begin Geekbench phase charts.",
"Each chart is zoomed to (contains the only the data for) one phase.",
"To go directly to a particular phase, go to the T=X seconds picture.",
"For example, to see the Multi-Core SGEMM, go to T= 48 seconds.",
"",
"phase: Single-Core AES, 2883, 2.17 GB/sec",
"phase: Single-Core LZMA, 3047, 4.76 MB/sec",
"phase: Single-Core JPEG, 3604, 29.0 Mpixels/sec",
"phase: Single-Core Canny, 3525, 48.9 Mpixels/sec",
"phase: Single-Core Lua, 3717, 3.82 MB/sec",
"phase: Single-Core Dijkstra, 3782, 2.56 MTE/sec",
"phase: Single-Core SQLite, 3790, 105.1 Krows/sec",
"phase: Single-Core HTML5 Parse, 3799, 17.2 MB/sec",
"phase: Single-Core HTML5 DOM, 3325, 3.01 MElements/sec",
"phase: Single-Core Histogram Equalization, 3270, 102.2 Mpixels/sec",
"phase: Single-Core PDF Rendering, 3828, 101.7 Mpixels/sec",
"phase: Single-Core LLVM, 6530, 449.0 functions/sec",
"phase: Single-Core Camera, 3436, 9.53 images/sec",
"phase: Single-Core SGEMM, 2740, 57.9 Gflops",
"phase: Single-Core SFFT, 3423, 8.54 Gflops",
"phase: Single-Core N-Body Physics, 3030, 2.26 Mpairs/sec",
"phase: Single-Core Ray Tracing, 2614, 381.7 Kpixels/sec",
"phase: Single-Core Rigid Body Physics, 3198, 9363.4 FPS",
"phase: Single-Core HDR, 3485, 12.6 Mpixels/sec",
"phase: Single-Core Gaussian Blur, 3373, 59.1 Mpixels/sec",
"phase: Single-Core Speech Recognition, 3070, 26.3 Words/sec",
"phase: Single-Core Face Detection, 3180, 928.9 Ksubwindows/sec",
"phase: Single-Core Memory Copy, 3437, 9.52 GB/sec",
"phase: Single-Core Memory Latency, 5382, 80.4 ns",
"phase: Single-Core Memory Bandwidth, 2751, 14.7 GB/sec",
"phase: Multi-Core AES, 4898, 3.69 GB/sec",
"phase: Multi-Core LZMA, 6333, 9.89 MB/sec",
"phase: Multi-Core JPEG, 7932, 63.8 Mpixels/sec",
"phase: Multi-Core Canny, 7009, 97.2 Mpixels/sec",
"phase: Multi-Core Lua, 5967, 6.13 MB/sec",
"phase: Multi-Core Dijkstra, 9081, 6.14 MTE/sec",
"phase: Multi-Core SQLite, 6469, 179.4 Krows/sec",
"phase: Multi-Core HTML5 Parse, 6257, 28.4 MB/sec",
"phase: Multi-Core HTML5 DOM, 3476, 3.15 MElements/sec",
"phase: Multi-Core Histogram Equalization, 6614, 206.7 Mpixels/sec",
"phase: Multi-Core PDF Rendering, 7393, 196.4 Mpixels/sec",
"phase: Multi-Core LLVM, 12953, 890.6 functions/sec",
"phase: Multi-Core Camera, 7091, 19.7 images/sec",
"phase: Multi-Core SGEMM, 4367, 92.3 Gflops",
"phase: Multi-Core SFFT, 6049, 15.1 Gflops",
"phase: Multi-Core N-Body Physics, 5696, 4.25 Mpairs/sec",
"phase: Multi-Core Ray Tracing, 5458, 797.1 Kpixels/sec",
"phase: Multi-Core Rigid Body Physics, 7369, 21574.1 FPS",
"phase: Multi-Core HDR, 7569, 27.4 Mpixels/sec",
"phase: Multi-Core Gaussian Blur, 5618, 98.4 Mpixels/sec",
"phase: Multi-Core Speech Recognition, 5534, 47.3 Words/sec",
"phase: Multi-Core Face Detection, 6300, 1.84 Msubwindows/sec",
"phase: Multi-Core Memory Copy, 4244, 11.8 GB/sec",
"phase: Multi-Core Memory Latency, 4104, 105.5 ns",
"phase: Multi-Core Memory Bandwidth, 3229, 17.2 GB/sec",
]
beg_num = 10
fsz = int(yfctr*40)
sz = fsz + int(yfctr*4)
row[2] = sz
row = [0, 0, sz]
font = ImageFont.truetype("/Program Files/Microsoft Office 15/root/vfs/Fonts/private/arialn.ttf", fsz)
fnt = fontb
for i in range(0, len(pg_txt)):
    txt = pg_txt[i]
    if i == 1:
        fsz = int(yfctr*33)
        sz = fsz + int(yfctr*4)
        row[1] += sz
        row[2] = sz
        font = ImageFont.truetype("/Program Files/Microsoft Office 15/root/vfs/Fonts/private/arialn.ttf", fsz)
        fnt = font
    if i == 3:
        txt = "For example, to see the Multi-Core SGEMM, go to T= %d seconds." % (beg_num + 38)
    if i > 4:
        nm = "T= %.2d " % (i - 5 + beg_num)
        txt = nm + txt
    draw.text((offx, gety(row)), txt, (0,0,0), font=fnt)
draw = ImageDraw.Draw(img)
img.save("g_test.png")

imgd = Image.open(pdir+"pat00000.png")
drawd = ImageDraw.Draw(imgd)
rct_brdr = 10
x, y = (0, 0 )
fsz = 50
sz = fsz + 4
font = ImageFont.truetype("/Program Files/Microsoft Office 15/root/vfs/Fonts/private/arialn.ttf", fsz)
text = bmark+" - the movie, v4"
w, h = font.getsize(text)
x = szx/2 - w/2 - 100
y = szy/10
drawd.rectangle((x,     y, x + w + 2*rct_brdr, y + h+rct_brdr), fill='black')
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
        img_dta = imageio.imread(im)
        #if 1==20 and i > 100:
            #print(img_dta.shape)
            #else:
            #break
        i += 1
        #img_dta = imageio.imread('black.png').shape
        #writer.append_data(imageio.imread(im))
        writer.append_data(img_dta)
    img_dta = imageio.imread("t_test.png")
    writer.append_data(img_dta)
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

