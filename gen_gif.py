import imageio
import glob
images = []
filenames = glob.glob("pat*.png")
for filename in filenames:
    images.append(imageio.imread(filename))
imageio.mimsave('movie.gif', images, "gif", duration=1)
