import os.path
import random as rand
import math 

i = 0
while True:
    s = "cfg_synth" + str(i) + ".txt"
    if os.path.isfile(s):
        i += 1
    else:
        break

f = open(s, "w")


sizes = ["K1", "K2", "K4", "K8", "K16", "K32", "K64", "K128", "K256", "K512", "M1", "M2", "M4", "M8"]
patterns = ["STREAMING", "STRIDED", "IRREGULAR"]
burst_lens = [4, 8, 16, 32, 64, 128]
cb_factors = [1, 2, 4, 8, 16, 32]
reuse_factors = [1]
ld_st_ratios = [1, 2, 4]
stride_lens = [32, 64, 128, 256, 512]
phases = rand.randint(1, 100)

f.write("5 5\n")
f.write("4\n")
f.write("0 3 20 23\n")
f.write("12\n")
f.write("1 2 5 8 10 11 12 13 15 18 21 22\n")
f.write(str(phases) + "\n")

for p in range(phases):
    devices = range(12)
    threads = rand.randint(1, 12)
    f.write(str(threads) + "\n")
    for t in range(threads):
        ndev = rand.randint(1, len(devices) - (threads - (t + 1))) 
        f.write(str(ndev) + "\n")
        size = rand.choice(sizes)
        log_size = 10 + sizes.index(size)
        f.write(str(size) + "\n")
        for d in range(ndev):
            #DEVICE
            d = rand.choice(devices)
            devices.remove(d)
            f.write(str(d) + " ")
            
            #PATTERN
            pattern = rand.choice(patterns)
            f.write(pattern + " ")
            
            #ACCESS FACTOR
            if pattern == "IRREGULAR":
                upper = log_size - 10 - ndev - (d + 1) 
                if upper > 4:
                    upper = 4
                if upper < 0:
                    upper = 0
                access_factor = rand.randint(0, upper)
            else:
                access_factor = 0
            log_size -= access_factor
            f.write(str(access_factor) + " ")
            
            #BURST LEN
            if pattern == "IRREGULAR":
                burst_len = 4
            else:
                burst_len = rand.choice(burst_lens)
            f.write(str(burst_len) + " ")
            
            #COMPUTE BOUND FACTOR
            upper = burst_lens.index(burst_len) + 3
            if upper > len(cb_factors):
                upper = len(cb_factors)
            cb_factor = rand.choice(cb_factors[0:upper])
            f.write(str(cb_factor) + " ")
            
            #REUSE FACTOR
            reuse_factor = rand.choice(reuse_factors)
            f.write(str(reuse_factor) + " ")
            
            #LD ST RATIO
            upper = log_size - 10 - ndev - (d + 1)
            if upper > 2:
                upper = 2
            if upper <= 0:
                ld_st_ratio = 1
            elif pattern == "IRREGULAR":
                ld_st_ratio = 1
            else:
                ld_st_ratio = rand.choice(ld_st_ratios[0:upper])
            log_size -= ld_st_ratios.index(ld_st_ratio)
            f.write(str(ld_st_ratio) + " ")
            
            #STRIDE LEN
            if pattern == "STRIDED":
                stride_len = rand.choice(stride_lens)
            else:
                stride_len = 0
            f.write(str(stride_len) + " ")
            
            #IN PLACE
            in_place = rand.randint(0, 1)
            f.write(str(in_place) + "\n")

f.close()
