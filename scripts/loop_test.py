import json
import sys
import os
import subprocess
from multiprocessing import Pool
from multiprocessing import cpu_count
from loop_calc import simpoint_err_calc

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
    for dirname in filter(os.path.isdir, os.listdir(os.getcwd())):
        os.chdir(dirname)
        for inputs in filter(os.path.isdir, os.listdir(os.getcwd())):
            os.chdir(inputs)

            # print(os.path.join(cfg["simget_home"], "bin/perf_ov_loop") +
            #       " " + os.path.join(os.getcwd(), simpoint_cfg_prefix+"loop_cfg.json"))
            subprocess.run(os.path.join(
                cfg["simget_home"], "bin/perf_ov_loop") + " " + os.path.join(os.getcwd(), simpoint_cfg_prefix+"loop_cfg.json"), shell=True)

            os.chdir("..")
        os.chdir("..")

    return


def result_calc(cfg):
    if os.path.exists(cfg["dir_out"]) == False:
        print("no folder exists!")
        return
    os.chdir(cfg["dir_out"])
    collect_file = open(simpoint_cfg_prefix+"collect_res.txt", 'w')

    for dirname in filter(os.path.isdir, os.listdir(os.getcwd())):
        os.chdir(dirname)
        print(dirname, file=collect_file)
        for inputs in filter(os.path.isdir, os.listdir(os.getcwd())):
            os.chdir(inputs)
            with open(simpoint_cfg_prefix + "loop_cfg.json", 'r') as loop_cfg_file:
                loop_cfg = json.load(loop_cfg_file)
                try:
                    a, b, c, d, e = simpoint_err_calc(loop_cfg)
                except ZeroDivisionError:
                    print("input file wrong!", file=collect_file)
                    print("spec run fail, caused by coredump, need fix",
                          file=collect_file)
                except IndexError:
                    print("input file wrong!", file=collect_file)
                    print("spec run fail, caused by coredump, need fix",
                          file=collect_file)
                except Exception as e:
                    print("unknown except!", file=collect_file)
                    print(e, file=collect_file)
                else:
                    print(inputs, file=collect_file)
                    print(a, b, c, file=collect_file)
                    print(d, e, file=collect_file)
                    print(file=collect_file)

            os.chdir("..")
        print("-----------------", file=collect_file)
        os.chdir("..")


result_calc(cfg)
