import json
import sys
import os
import subprocess
from multiprocessing import Pool
from multiprocessing import cpu_count

cfg = {}
with open(sys.argv[1], 'r') as cfg_file:
    cfg = json.load(cfg_file)
    simpoint_cfg_prefix = str(int(cfg["interval_size"]/1000000)) + \
        'M_max'+str(cfg["simpoint"]["maxK"])+"_"


def run_loop_test(cfg):
    if os.path.exists(cfg["dir_out"]) == False:
        print("no folder exists!")
        return
    os.chdir(cfg["dir_out"])
    for dirname in os.listdir("./"):
        os.chdir(dirname)
        for inputs in os.listdir("./"):
            os.chdir(inputs)

            # print(os.path.join(cfg["simget_home"], "bin/perf_ov_loop") +
            #       " " + os.path.join(os.getcwd(), simpoint_cfg_prefix+"loop_cfg.json"))
            subprocess.run(os.path.join(
                cfg["simget_home"], "bin/perf_ov_loop") + " " + os.path.join(os.getcwd(), simpoint_cfg_prefix+"loop_cfg.json"), shell=True)

            os.chdir("..")
        os.chdir("..")

    return


run_loop_test(cfg)
