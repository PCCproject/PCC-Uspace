import os

cmd = "cd ../src && make clean && make -j8 && cd ../sim && make clean && make -j8"
print(cmd)
os.system(cmd)
