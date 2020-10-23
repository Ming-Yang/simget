import json
import sys
import os
import subprocess
import time
from multiprocessing import Pool
from multiprocessing import cpu_count
from simget_util import get_test_path, get_simpoint_cfg_prefix


def calc_simpoint_loop_err(top_cfg, count=False):
    # use loop intervals to get simpoints results
    points = []
    weights = []
    intervals = []

    with open(top_cfg["loop"]["out_file"]) as cfg_file:
        for line in cfg_file.readlines():
            data = line.strip().split(" ")
            data = [int(i) for i in data]
            intervals.append(data)

    with open(top_cfg["simpoint"]["point_file"]) as cfg_file:
        for line in cfg_file.readlines():
            data = line.strip().split(" ")
            data = [int(i) for i in data]
            points.append(data)

    with open(top_cfg["simpoint"]["weight_file"]) as cfg_file:
        for line in cfg_file.readlines():
            data = line.strip().split(" ")
            data = [float(i) for i in data]
            weights.append(data)

    total_ipc = intervals[-1][0]/intervals[-1][1]
    if count == True:
        return total_ipc, 0, 0, 0, 0, intervals[-1][0], intervals[-1][1]

    insts = 0
    cycles = 0
    for data in intervals[0:-1]:
        insts += data[0]
        cycles += data[1]
    avg_interval_ipc = insts/cycles

    insts = 0
    cycles = 0
    for point, weight in zip(points, weights):
        insts += intervals[point[0]][0]*weight[0]
        cycles += intervals[point[0]][1]*weight[0]
    simpoint_ipc = insts/cycles

    avg_ipc_err = (avg_interval_ipc-total_ipc)/total_ipc
    simpoint_ipc_err = (simpoint_ipc-total_ipc)/total_ipc

    return total_ipc, avg_interval_ipc, simpoint_ipc, avg_ipc_err, simpoint_ipc_err, intervals[-1][0], intervals[-1][1]


def run_loop_test(top_cfg, run=False):
    # use loop to get all intervals and print to file
    simpoint_warm_cfg_prefix = get_simpoint_cfg_prefix(top_cfg, True)

    if os.path.exists(top_cfg["dir_out"]) == False:
        print("no folder exists!")
        return
    os.chdir(top_cfg["dir_out"])
    for dirname in filter(os.path.isdir, os.listdir(os.getcwd())):
        if top_cfg["partial_test"] == True and dirname not in top_cfg["user_test_list"]:
            continue
        os.chdir(dirname)
        for inputs in filter(os.path.isdir, os.listdir(os.getcwd())):
            os.chdir(inputs)

            if run == False:
                print(os.path.join(top_cfg["simget_home"], "bin/perf_ov_loop") +
                      " " + os.path.join(os.getcwd(), simpoint_warm_cfg_prefix+"loop_cfg.json"))
            else:
                subprocess.run(os.path.join(
                    top_cfg["simget_home"], "bin/perf_ov_loop") + " " + os.path.join(os.getcwd(), simpoint_warm_cfg_prefix+"loop_cfg.json"), shell=True)

            os.chdir("..")
        os.chdir("..")

    return


def calc_loop_result(top_cfg, count=False):
    # select simpoints from all intervals results and calc
    simpoint_warm_cfg_prefix = get_simpoint_cfg_prefix(top_cfg, True)

    if os.path.exists(top_cfg["dir_out"]) == False:
        print("no folder exists!")
        return
    os.chdir(top_cfg["dir_out"])
    collect_file = open(simpoint_warm_cfg_prefix+"loop_res.txt", 'a')
    print(time.asctime(time.localtime(time.time())),
          "===================================================", file=collect_file)
    print("test", "input", "full-insts", "full-cycles", "full-ipc", "loop-ipc", "loop-simpoint-ipc", "loop-ipc-err(%)",
          "loop-simpoint-ipc-err(%)", file=collect_file)
    for dirname in filter(os.path.isdir, os.listdir(os.getcwd())):
        os.chdir(dirname)
        for inputs in filter(os.path.isdir, os.listdir(os.getcwd())):
            os.chdir(inputs)
            with open(simpoint_warm_cfg_prefix + "loop_cfg.json", 'r') as loop_cfg_file:
                loop_cfg = json.load(loop_cfg_file)
                try:
                    a, b, c, d, e, f, g = calc_simpoint_loop_err(loop_cfg, False)
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
                    print(dirname, inputs, file=collect_file, end=' ')
                    print("{:.3f}".format(f), "{:.3f}".format(g),
                          file=collect_file, end=' ')
                    print("{:.3f}".format(a), "{:.3f}".format(b),
                          "{:.3f}".format(c), file=collect_file, end=' ')
                    print("{:.2f}".format(d*100),
                          "{:.2f}".format(e*100), file=collect_file)

            os.chdir("..")
        os.chdir("..")
