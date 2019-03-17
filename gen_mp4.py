import glob
import sys
import imageio

#fileList = []
fileList_orig = glob.glob("../gb_pngs/pat*.png")
#for file in os.listdir(path):
#    if file.startswith(name):
#        complete_path = path + file
#        fileList.append(complete_path)


writer = imageio.get_writer('test.mp4', fps=10)

fileList = fileList_orig
img_dta = imageio.imread(fileList[0])
img_shape = img_dta.shape


from PIL import Image, ImageFont, ImageDraw
#font = ImageFont.truetype("/usr/share/fonts/dejavu/DejaVuSans.ttf", 25)
font = ImageFont.truetype("/Program Files/Microsoft Office 15/root/vfs/Fonts/private/arialn.ttf", 50)
fontb = ImageFont.truetype("/Program Files/Microsoft Office 15/root/vfs/Fonts/private/arialnb.ttf", 60)
#img = Image.new("RGBA", (200,200), (120,20,20))

szx = img_shape[1]
szy = img_shape[0]
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
draw.text((offx, gety(row)), "Geekbench - the movie", (0,0,0), font=fontb)
draw.text((offx, gety(row)), "Created using OPPAT", (0,0,0), font=font)
draw.text((offx, gety(row)), "Open Power/Performance Analysis Tool", (0,0,0), font=font)
draw.text((offx, gety(row)), "See https://patinnc.github.io/", (0,0,0), font=font)
fsz = 35
sz = fsz + 4
font = ImageFont.truetype("/Program Files/Microsoft Office 15/root/vfs/Fonts/private/arialn.ttf", fsz)
row[1] += 30
row[2] = sz
draw.text((offx, gety(row)), "The power/perf data is from a single run of geekbench4 on a", (0,0,0), font=font)
draw.text((offx, gety(row)), "4 CPU (2 core) Intel Haswell CPU.", (0,0,0), font=font)
draw.text((offx, gety(row)), "The cpu diagram shows resource constraints on Haswell such as", (0,0,0), font=font)
draw.text((offx, gety(row)), "the max UOPS/cycle (4) from the RAT. OPPAT calculates the current", (0,0,0), font=font)
draw.text((offx, gety(row)), "resource utilization for each resource and categorizes the usage", (0,0,0), font=font)
draw.text((offx, gety(row)), "with a simple red/yellow/green light for each core.", (0,0,0), font=font)
draw.text((offx, gety(row)), "This lets you see whether the system is stalled on memory", (0,0,0), font=font)
draw.text((offx, gety(row)), "L3, or L2 bandwidth, waiting on buffers, instruction misses, etc.", (0,0,0), font=font)
draw.text((offx, gety(row)), "Data is collected using linux perf stat/record, trace-cmd and", (0,0,0), font=font)
draw.text((offx, gety(row)), "custom utils. Perf collects data for the 50+ hw events, frace", (0,0,0), font=font)
draw.text((offx, gety(row)), "and flamegraphs. GB phase, system usage, power, T, disk bw is shown.", (0,0,0), font=font)
draw.text((offx, gety(row)), "OPPAT can collect/analyze data on:", (0,0,0), font=font)
draw.text((offx, gety(row)), " Linux/Android - perf, trace-cmd, sysfs, other", (0,0,0), font=font)
draw.text((offx, gety(row)), " windows - ETW, PCM, other", (0,0,0), font=font)
row[1] += sz
draw.text((offx, gety(row)), "This movie shows the values of a bunch of data over 0.1 sec", (0,0,0), font=font)
draw.text((offx, gety(row)), "subintervals during the GB run. GB has sleep intervals and,", (0,0,0), font=font)
draw.text((offx, gety(row)), "if the system is less than half a cpu busy then the subinterval", (0,0,0), font=font)
draw.text((offx, gety(row)), "is dropped. The actual data is much higher resolution than 0.1 sec.", (0,0,0), font=font)
row[1] += sz
draw.text((offx, gety(row)), "On windows the movie is best viewed vertically (ctrl+alt+right_arrow)", (0,0,0), font=font)
draw.text((offx, gety(row)), "Do ctrl+alt+up_arrow to return to normal mode.", (0,0,0), font=font)
draw.text((offx, gety(row)), "contact patrick.99.fay@gmail.com", (0,0,0), font=font)
row[1] += sz
draw.text((offx, gety(row)), "Block diagram courtesy of WikiChip.org", (0,0,0), font=font)
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

imgd = Image.open("../gb_pngs/pat00000.png")
drawd = ImageDraw.Draw(imgd)
rct_brdr = 10
x, y = (10, 50 - fsz)
text = "CPU front end: instructions from memory (L2+L3) -> UOPS for execution"
w, h = font.getsize(text)
drawd.rectangle((x, y, x + w + 2*rct_brdr, y + h), fill='black')
drawd.text((x+rct_brdr, y), text, fill='white', font=font)
drawd.rectangle([x,y,szx-10,600], width = rct_brdr, outline="#0000ff")

x, y = (10, 660 - fsz)
text = "CPU execution engine: UOPS+data execute in ports"
w, h = font.getsize(text)
drawd.rectangle((x, y, x + w + 2*rct_brdr, y + h), fill='black')
drawd.text((x+rct_brdr, y), text, fill='white', font=font)
drawd.rectangle([x,y,800,y+500], width = rct_brdr, outline="#0000ff")

x, y = (330, 1100)
text = "Data to L1 from memory (L2+L3)"
w, h = font.getsize(text)
drawd.rectangle((x, y, x + w + 2*rct_brdr, y + h), fill='black')
drawd.text((x+rct_brdr, y), text, fill='white', font=font)
drawd.rectangle([x,y,x+600,y+280], width = rct_brdr, outline="#0000ff")

imgd.save('b_test.png')

imgd = Image.open("../gb_pngs/pat_base.png")
drawd = ImageDraw.Draw(imgd)
rct_brdr = 10
x, y = (10, 50 - fsz)
text = "CPU front end: instructions from memory (L2+L3) -> UOPS for execution"
w, h = font.getsize(text)
drawd.rectangle((x, y, x + w + 2*rct_brdr, y + h), fill='black')
drawd.text((x+rct_brdr, y), text, fill='white', font=font)
drawd.rectangle([x,y,szx-10,630], width = rct_brdr, outline="#0000ff")

x, y = (10, 620)
text = "CPU execution engine: UOPS+data execute in ports"
w, h = font.getsize(text)
drawd.rectangle((x, y, x + w + 2*rct_brdr, y + h), fill='black')
drawd.text((x+rct_brdr, y+4), text, fill='white', font=font)
drawd.rectangle([x,y,800,y+500], width = rct_brdr, outline="#0000ff")

x, y = (330, 1100)
text = "Data to L1 from memory (L2+L3)"
w, h = font.getsize(text)
drawd.rectangle((x, y, x + w + 2*rct_brdr, y + h), fill='black')
drawd.text((x+rct_brdr, y), text, fill='white', font=font)
drawd.rectangle([x,y,x+600,y+280], width = rct_brdr, outline="#0000ff")

imgd.save('c_test.png')

imgd = Image.open("../gb_pngs/pat_base.png")
drawd = ImageDraw.Draw(imgd)
rct_brdr = 10
x, y = (0, 0)
text = "CPU block diagram: shows resource constraints between blocks"
w, h = font.getsize(text)
drawd.rectangle((x, y, x + w + 2*rct_brdr, y + h), fill='black')
drawd.text((x+rct_brdr, y), text, fill='white', font=font)

x, y = (0, y+h)
text = "in terms of UOPS/cycle or bytes/cycle"
w, h = font.getsize(text)
drawd.rectangle((x, y, x + w + 2*rct_brdr, y + h), fill='black')
drawd.text((x+rct_brdr, y), text, fill='white', font=font)

drawd.rectangle([270,480,345, 530], width = rct_brdr, outline="#0000ff")
drawd.rectangle([240,650,335, 710], width = rct_brdr, outline="#0000ff")
drawd.rectangle([850,1150,950,1270], width = rct_brdr, outline="#0000ff")
imgd.save('d_test.png')

imgd = Image.open("../gb_pngs/pat00000.png")
drawd = ImageDraw.Draw(imgd)
rct_brdr = 10
x, y = (0, 0)
text = "OPPAT also shows stalls due to resource saturation such as"
w, h = font.getsize(text)
drawd.rectangle((x, y, x + w + 2*rct_brdr, y + h), fill='black')
drawd.text((x+rct_brdr, y), text, fill='white', font=font)

x, y = (0, y+h)
text = "no fill buffers, no store buffers or no reserveration station entries"
w, h = font.getsize(text)
drawd.rectangle((x, y, x + w + 2*rct_brdr, y + h), fill='black')
drawd.text((x+rct_brdr, y), text, fill='white', font=font)


drawd.rectangle([350,140,480, 190], width = rct_brdr, outline="#0000ff")
drawd.rectangle([ 80,623,245, 680], width = rct_brdr, outline="#0000ff")
drawd.rectangle([180,740,305, 790], width = rct_brdr, outline="#0000ff")
drawd.rectangle([435,800,575, 850], width = rct_brdr, outline="#0000ff")
drawd.rectangle([905,1105,1015,1155], width = rct_brdr, outline="#0000ff")
drawd.rectangle([650,1175,775,1230], width = rct_brdr, outline="#0000ff")
drawd.rectangle([680,1342,827,1400], width = rct_brdr, outline="#0000ff")
imgd.save('e_test.png')

imgd = Image.open("../gb_pngs/pat00000.png")
drawd = ImageDraw.Draw(imgd)
rct_brdr = 10
x, y = (0, 0 )
text = "OPPAT: system info: %busy, freq, T, power, disk & mem bw"
w, h = font.getsize(text)
drawd.rectangle((x, y, x + w + 2*rct_brdr, y + h), fill='black')
drawd.text((x+rct_brdr, y), text, fill='white', font=font)

drawd.rectangle([0,y+h,360, 170], width = rct_brdr, outline="#0000ff")

x, y = (0, 250 )
text = "Display begin/end geekbench sub-benchmark for interval, score, metric"
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

imgd = Image.open("../gb_pngs/pat00000.png")
drawd = ImageDraw.Draw(imgd)
rct_brdr = 10
x, y = (0, 0 )
fsz = 50
sz = fsz + 4
font = ImageFont.truetype("/Program Files/Microsoft Office 15/root/vfs/Fonts/private/arialn.ttf", fsz)
text = "Geekbench - the movie"
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
for j in range(len(addList)-1, -1, -1):
    whch += 1
    for i in range(10):
        fileList.insert(0, addList[j])

i = 0
for im in fileList:
    img_dta = imageio.imread(im)
    if 1==20 and i > 100:
        #print(img_dta.shape)
        #else:
        break
    i += 1
    #img_dta = imageio.imread('black.png').shape
    #writer.append_data(imageio.imread(im))
    writer.append_data(img_dta)
writer.close()

