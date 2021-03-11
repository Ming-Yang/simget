import json
import sys
import os
import subprocess
import time
import platform
import re
from multiprocessing import Pool
from multiprocessing import cpu_count
from simget_util import get_test_path, get_simpoint_cfg_prefix


def resolve_loop_result_file(local_cfg):
    # use loop intervals to get simpoints results
    points = []
    weights = []
    intervals = []

    with open(local_cfg["loop"]["out_file"]) as cfg_file:
        for line in cfg_file.readlines():
            data = line.strip().split(" ")
            data = [int(i) for i in data]
            intervals.append(data)

    with open(local_cfg["simpoint"]["point_file"]) as cfg_file:
        for line in cfg_file.readlines():
            data = line.strip().split(" ")
            data = [int(i) for i in data]
            points.append(data)

    with open(local_cfg["simpoint"]["weight_file"]) as cfg_file:
        for line in cfg_file.readlines():
            data = line.strip().split(" ")
            data = [float(i) for i in data]
            weights.append(data)

    total_insts = intervals[-1][0]
    total_cycles = intervals[-1][1]
    total_ipc = total_insts/total_cycles

    avg_insts = 0
    avg_cycles = 0
    for data in intervals[0:-1]:
        avg_insts += data[0]
        avg_cycles += data[1]
    avg_ipc = avg_insts/avg_cycles

    simpoint_insts = 0
    simpoint_cycles = 0
    for point, weight in zip(points, weights):
        simpoint_insts += intervals[point[0]][0]
        simpoint_cycles += intervals[point[0]][1]*weight[0]
    simpoint_ipc = simpoint_insts/len(points)/simpoint_cycles

    return total_insts, total_cycles, total_ipc,\
        avg_insts, avg_cycles, avg_ipc,\
        simpoint_insts, simpoint_cycles, simpoint_ipc


def save_fixed_result(top_cfg):
    # save fixed results to json
    simpoint_warm_cfg_prefix = get_simpoint_cfg_prefix(top_cfg, True)

    if os.path.exists(top_cfg["dir_out"]) == False:
        print("no folder exists!")
        return
    os.chdir(top_cfg["dir_out"])
    collect_dict = {}
    collect_dict["time"] = time.asctime(time.localtime(time.time()))
    collect_dict["platform"] = platform.processor()
    for dirname in filter(os.path.isdir, os.listdir(os.getcwd())):
        os.chdir(dirname)
        dir_result = {}
        for inputs in filter(os.path.isdir, os.listdir(os.getcwd())):
            os.chdir(inputs)
            input_result = {}
            bb_result = {}
            loop_result = {}
            avg_result = {}
            simpoint_result = {}
            perf_result = {}
            pin_result = {}
            mycnt_result = {}

            try:
                bb_file = open("simpoint.bb", 'r')
                bb_insts = re.compile(r"#\s*Total instructions:\s*(\d+)")
                for line in bb_file.readlines():
                    match_insts = bb_insts.match(line)
                    if match_insts:
                        bb_result["insts"] = int(
                            match_insts.group(1).replace(',', ''))
            except FileNotFoundError:
                print(dirname, inputs, "simpoint.bb not found")

            try:
                perf_file = open("perf.result", 'r')
                perf_insts = re.compile(r"\s*([\d,]*)\s*instructions:u.*")
                perf_cycles = re.compile(r"\s*([\d,]*)\s*cycles:u.*")
                for line in perf_file.readlines():
                    match_insts = perf_insts.match(line)
                    match_cycles = perf_cycles.match(line)
                    if match_insts:
                        perf_result["insts"] = int(
                            match_insts.group(1).replace(',', ''))
                    elif match_cycles:
                        perf_result["cycles"] = int(
                            match_cycles.group(1).replace(',', ''))
            except FileNotFoundError:
                print(dirname, inputs, "perf.result not found")

            try:
                pin_file = open("pin.result", 'r')
                pin_insts = re.compile(r"Count ([\d]*)")
                for line in pin_file.readlines():
                    match_insts = pin_insts.match(line)
                    if match_insts:
                        pin_result["insts"] = int(
                            match_insts.group(1))
            except FileNotFoundError:
                print(dirname, inputs, "pin.result not found")

            try:
                perf_file = open("mycnt.result", 'r')
                mycnt_insts = re.compile(r"total inst counts:([\d,]*)")
                for line in perf_file.readlines():
                    match_insts = mycnt_insts.match(line)
                    if match_insts:
                        mycnt_result["insts"] = int(
                            match_insts.group(1).replace(',', ''))
            except FileNotFoundError:
                print(dirname, inputs, "mycnt.result not found")

            try:
                loop_cfg_file = open(
                    simpoint_warm_cfg_prefix + "loop_cfg.json", 'r')
                loop_cfg = json.load(loop_cfg_file)
                loop_result["insts"], loop_result["cycles"], loop_result["ipc"], \
                    avg_result["insts"], avg_result["cycles"], avg_result["ipc"], \
                    simpoint_result["insts"], simpoint_result["cycles"], simpoint_result["ipc"] = \
                    resolve_loop_result_file(loop_cfg)
            except Exception as exc:
                print(dirname, inputs, "Exception", exc)

            if len(bb_result) > 0:
                input_result["bb_result"] = bb_result
            if len(perf_result) > 0:
                input_result["perf_result"] = perf_result
            if len(pin_result) > 0:
                input_result["pin_result"] = pin_result
            if len(mycnt_result) > 0:
                input_result["mycnt_result"] = perf_result

            if len(loop_result) > 0:
                input_result["loop_result"] = loop_result
            if len(avg_result) > 0:
                input_result["avg_result"] = avg_result
            if len(simpoint_result) > 0:
                input_result["simpoint_result"] = simpoint_result

            if len(input_result) > 0:
                dir_result[inputs] = input_result
            os.chdir("..")
            collect_dict[dirname] = dir_result
        os.chdir("..")

    with open("fix_test_res.json", 'w') as f:
        json.dump(collect_dict, f, indent=4)


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
