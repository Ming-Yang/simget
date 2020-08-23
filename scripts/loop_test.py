import json
import sys
import os
import subprocess
import time
from multiprocessing import Pool
from multiprocessing import cpu_count
from loop_calc import simpoint_loop_err_calc


def run_loop_test(cfg, run=False):
    # use loop to get all intervals and print to file
    simpoint_cfg_prefix = str(int(cfg["interval_size"]/1000000)) + \
        'M_max'+str(cfg["simpoint"]["maxK"])+"_"

    if os.path.exists(cfg["dir_out"]) == False:
        print("no folder exists!")
        return
    os.chdir(cfg["dir_out"])
    for dirname in filter(os.path.isdir, os.listdir(os.getcwd())):
        if cfg["partial_test"] == True and dirname not in cfg["user_test_list"]:
            continue
        os.chdir(dirname)
        for inputs in filter(os.path.isdir, os.listdir(os.getcwd())):
            os.chdir(inputs)

            if run == False:
                print(os.path.join(cfg["simget_home"], "bin/perf_ov_loop") +
                      " " + os.path.join(os.getcwd(), simpoint_cfg_prefix+"loop_cfg.json"))
            else:
                subprocess.run(os.path.join(
                    cfg["simget_home"], "bin/perf_ov_loop") + " " + os.path.join(os.getcwd(), simpoint_cfg_prefix+"loop_cfg.json"), shell=True)

            os.chdir("..")
        os.chdir("..")

    return


def result_calc(cfg):
    # select simpoints from all intervals results and calc
    simpoint_cfg_prefix = str(int(cfg["interval_size"]/1000000)) + \
        'M_max'+str(cfg["simpoint"]["maxK"])+"_"

    if os.path.exists(cfg["dir_out"]) == False:
        print("no folder exists!")
        return
    os.chdir(cfg["dir_out"])
    collect_file = open(simpoint_cfg_prefix+"collect_res.txt", 'a')
    print(time.asctime(time.localtime(time.time())),
          "===================================================", file=collect_file)

    for dirname in filter(os.path.isdir, os.listdir(os.getcwd())):
        os.chdir(dirname)
        print(dirname, file=collect_file)
        for inputs in filter(os.path.isdir, os.listdir(os.getcwd())):
            os.chdir(inputs)
            with open(simpoint_cfg_prefix + "loop_cfg.json", 'r') as loop_cfg_file:
                loop_cfg = json.load(loop_cfg_file)
                try:
                    a, b, c, d, e = simpoint_loop_err_calc(loop_cfg)
                except ZeroDivisionError:
                    print("input file wrong!", file=collect_file)
                    print("spec run fail, caused by coredump, need fix",
                          file=collect_file)
                except IndexError:
                    print("input file wrong!", file=collect_file)
                    print("spec run fail, caused by coredump, need fix",
                          file=collect_file)
                except Exception as exc:
                    print("unknown except!", file=collect_file)
                    print(exc, file=collect_file)
                else:
                    print(inputs, file=collect_file)
                    print(a, b, c, file=collect_file)
                    print(d, e, file=collect_file)
                    print(file=collect_file)

            os.chdir("..")
        os.chdir("..")


cfg = {}
with open(sys.argv[1], 'r') as cfg_file:
    cfg = json.load(cfg_file)

run_loop_test(cfg)
result_calc(cfg)
