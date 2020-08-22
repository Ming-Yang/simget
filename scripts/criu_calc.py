import json
import sys
import os
import subprocess
import time


def criu_dump_all(cfg, run=False):
    if os.path.exists(cfg["dir_out"]) == False:
        print("no folder exists!")
        return
    simpoint_cfg_prefix = str(int(cfg["interval_size"]/1000000)) + \
        'M_max'+str(cfg["simpoint"]["maxK"])+"_"
    os.chdir(cfg["dir_out"])
    for dirname in filter(os.path.isdir, os.listdir(os.getcwd())):
        if cfg["partial_test"] == True and dirname not in cfg["user_test_list"]:
            continue
        os.chdir(dirname)
        for inputs in filter(os.path.isdir, os.listdir(os.getcwd())):
            os.chdir(inputs)

            cmd = os.path.join(
                cfg["simget_home"], "bin/perf_loop_dump") + " " + os.path.join(os.getcwd(), simpoint_cfg_prefix + "loop_cfg.json")
            if run == True:
                subprocess.run(cmd, shell=True)
            else:
                print(cmd)

            os.chdir("..")

        os.chdir("..")
    return


def simpoint_calc_criu(top_cfg, local_cfg, run=False):
    result_list = []
    os.chdir(local_cfg["image_dir"])
    dir_list = filter(os.path.isdir, os.listdir(os.getcwd()))
    dir_list = [int(item) for item in dir_list]

    idx = 0
    for point, weight in zip(local_cfg["simpoint"]["points"], local_cfg["simpoint"]["weights"]):
        o_cfg = local_cfg.copy()
        o_cfg["simpoint"]["current"] = idx
        if point < int(local_cfg["process"]["warmup_ratio"]):
            target = 0
        else:
            target = (point-int(local_cfg["process"]["warmup_ratio"])) * \
                int(local_cfg["process"]["ov_insts"])
        min_abs = sys.maxsize
        for d in dir_list:
            if min_abs > abs(target-d):
                min_abs = abs(target-d)
                target_dir = d
        if abs(target_dir-target) > int(local_cfg["process"]["ov_insts"]):
            print("no suitable checkpoint!")
            continue

        o_cfg["image_dir"] = os.path.join(
            local_cfg["image_dir"], str(target_dir))
        with open(str(target_dir) + "_restore_cfg.json", 'w') as f:
            f.write(json.dumps(o_cfg, indent=4))

        cmd = top_cfg["simget_home"] + "/bin/perf_restore_cnt " + \
            str(target_dir) + "_restore_cfg.json"
        if run == True:
            result = subprocess.getoutput(cmd)
            print(result)
            result_list.append(int(i) for i in result.split(' '))
        else:
            print(cmd)
        idx += 1

    os.chdir("..")

    if run == True:
        i_all = 0
        c_all = 0
        for [i, c] in result_list:
            i_all += int(i)
            c_all += int(c)

        return i_all/c_all


def criu_calc_all(cfg):
    if os.path.exists(cfg["dir_out"]) == False:
        print("no folder exists!")
        return
    simpoint_cfg_prefix = str(int(cfg["interval_size"]/1000000)) + \
        'M_max'+str(cfg["simpoint"]["maxK"])+"_"

    os.chdir(cfg["dir_out"])
    target_file = open(simpoint_cfg_prefix+"criu_res.log", 'a')
    print(time.asctime(time.localtime(time.time())), file=target_file)
    for dirname in filter(os.path.isdir, os.listdir(os.getcwd())):
        print(dirname, file=target_file)
        print(dirname)
        if cfg["partial_test"] == True and dirname not in cfg["user_test_list"]:
            continue
        os.chdir(dirname)
        for inputs in filter(os.path.isdir, os.listdir(os.getcwd())):
            print(inputs, file=target_file)
            print(inputs)
            os.chdir(inputs)
            criu_simpoint_ipc = 0.0
            full_ipc = 0.0
            with open(simpoint_cfg_prefix+"loop_cfg.json", "r") as local_cfg_file:
                local_cfg = json.load(local_cfg_file)
                try:
                    criu_simpoint_ipc = simpoint_calc_criu(
                        top_cfg, local_cfg, True)
                except ValueError:
                    print("error!", file=target_file)
                    os.chdir("..")
                    continue

            with open(simpoint_cfg_prefix+"loop_intervals.log", "r") as loop_log_file:
                [total_insts, total_cycles] = loop_log_file.readlines()[-1].split(" ")
                full_ipc = int(total_insts)/int(total_cycles)

            print(criu_simpoint_ipc, full_ipc, "err:",
                  (full_ipc - criu_simpoint_ipc)/criu_simpoint_ipc, file=target_file)

            os.chdir("..")

        os.chdir("..")
    return


top_cfg = {}
local_cfg = {}

with open(sys.argv[1], 'r') as cfg_file:
    top_cfg = json.load(cfg_file)

if len(sys.argv) > 2:
    with open(sys.argv[2], 'r') as cfg_file:
        local_cfg = json.load(cfg_file)

# criu_dump_all(top_cfg, True)
# print(simpoint_calc_criu(top_cfg, cfg , True))
criu_calc_all(top_cfg)
