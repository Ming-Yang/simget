import json
import sys
import os
import subprocess
from multiprocessing import Pool
from multiprocessing import cpu_count
user_test_list = ["177.mesa"]

def criu_dump_all(cfg, run=False):
    if os.path.exists(cfg["dir_out"]) == False:
        print("no folder exists!")
        return
    simpoint_cfg_prefix = str(int(cfg["interval_size"]/1000000)) + \
        'M_max'+str(cfg["simpoint"]["maxK"])+"_"
    os.chdir(cfg["dir_out"])
    for dirname in filter(os.path.isdir, os.listdir(os.getcwd())):
        if dirname not in user_test_list:
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


def simpoint_calc_criu(cfg, top_cfg, run=False):
    result_list = []
    os.chdir(cfg["image_dir"])
    dir_list = filter(os.path.isdir, os.listdir(os.getcwd()))
    dir_list = [int(item) for item in dir_list]

    idx = 0
    for point, weight in zip(cfg["simpoint"]["points"], cfg["simpoint"]["weights"]):
        o_cfg = cfg.copy()
        o_cfg["simpoint"]["current"] = idx
        target = (point-int(cfg["process"]["warmup_ratio"])) * \
            int(cfg["process"]["ov_insts"])
        min_abs = sys.maxsize
        for d in dir_list:
            if min_abs > abs(target-d):
                min_abs = abs(target-d)
                target_dir = d

        o_cfg["image_dir"] = os.path.join(cfg["image_dir"], str(target_dir))
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

    if run == True:
        i_all = 0
        c_all = 0
        for [i, c] in result_list:
            i_all += int(i)
            c_all += int(c)

        return i_all/c_all


top_cfg = {}
cfg = {}

with open(sys.argv[1], 'r') as cfg_file:
    top_cfg = json.load(cfg_file)

if len(sys.argv) > 2:
    with open(sys.argv[2], 'r') as cfg_file:
        cfg = json.load(cfg_file)

criu_dump_all(top_cfg, True)
# print(simpoint_calc_criu(cfg, top_cfg, True))
