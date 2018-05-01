import os
import sys

def movie_name(start, bw):
    return str(start) + "_" + str(bw) + ".mp4"

bws = [30, 50, 70]
starts = [20, 100]
movie_dir = "./movies/final/"

for bw in bws:
    for start in starts:
        cmd = "ffmpeg -r 1 -i ./imgs_" + str(start) + "_" + str(bw) + "/graph_%05d.png -vcodec mpeg4 -y " + movie_dir + "/" + movie_name(start, bw)
        print(cmd)
        os.system(cmd)
