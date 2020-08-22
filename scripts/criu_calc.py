import json
import sys
import os
import subprocess
from multiprocessing import Pool
from multiprocessing import cpu_count

cfg = {}
with open(sys.argv[1], 'r') as cfg_file:
    cfg = json.load(cfg_file)
top_cfg = {}
with open(sys.argv[2], 'r') as cfg_file:
    top_cfg = json.load(cfg_file)


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

        # print(path_dir, target_dir)
        o_cfg["image_dir"] = os.path.join(cfg["image_dir"], str(target_dir))
        with open(str(target_dir) + "_restore_cfg.json", 'w') as f:
            f.write(json.dumps(o_cfg, indent=4))

        if run == True:
            result = subprocess.getoutput(
                top_cfg["simget_home"] + "/bin/perf_restore_cnt " + str(target_dir) + "_restore_cfg.json")
            print(result)
            result_list.append(int(i) for i in result.split(' '))
        idx += 1
    
    if run == True:
        i_all = 0
        c_all = 0
        for [i, c] in result_list:
            i_all += int(i)
            c_all += int(c)

        return i_all/c_all

print(simpoint_calc_criu(cfg, top_cfg, True))