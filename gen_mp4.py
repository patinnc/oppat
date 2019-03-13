import glob
import imageio
#fileList = []
fileList = glob.glob("pat*.png")
#for file in os.listdir(path):
#    if file.startswith(name):
#        complete_path = path + file
#        fileList.append(complete_path)

writer = imageio.get_writer('test.mp4', fps=10)

for im in fileList:
    writer.append_data(imageio.imread(im))
writer.close()

