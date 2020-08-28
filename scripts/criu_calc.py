import json
import sys
import os
import subprocess
import time
import shutil
from simget_util import get_test_path, get_simpoint_cfg_prefix


def dump_criu_all(top_cfg, run=False):
    # dump all criu checkpoint into a folder named by checkpoint insts
    if os.path.exists(top_cfg["dir_out"]) == False:
        print("no folder exists!")
        return
    simpoint_warm_cfg_prefix = get_simpoint_cfg_prefix(top_cfg, True)
    os.chdir(top_cfg["dir_out"])
    for dirname in filter(os.path.isdir, os.listdir(os.getcwd())):
        if top_cfg["partial_test"] == True and dirname not in top_cfg["user_test_list"]:
            continue
        os.chdir(dirname)
        for inputs in filter(os.path.isdir, os.listdir(os.getcwd())):
            os.chdir(inputs)

            cmd = os.path.join(
                top_cfg["simget_home"], "bin/perf_loop_dump") + " " + os.path.join(os.getcwd(), simpoint_warm_cfg_prefix + "loop_cfg.json")
            if run == True:
                subprocess.run(cmd, shell=True)
            else:
                print(cmd)

            os.chdir("..")

        os.chdir("..")
    return


def calc_criu_simpoint(top_cfg, local_cfg, run=False, clean=False, bias_check=True):
    # calc criu restored(according to simpoint) ipc result, calc one checkpoint once call
    result_list = []
    least_offset_list = []
    return_dir = os.getcwd()
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
        if bias_check == True and abs(target_dir-target) > int(local_cfg["process"]["ov_insts"]):
            print("no suitable checkpoint!")
            continue

        least_offset_list.append(str(target_dir))
        o_cfg["image_dir"] = os.path.join(
            local_cfg["image_dir"], str(target_dir))
        with open(str(target_dir) + "_restore_cfg.json", 'w') as f:
            f.write(json.dumps(o_cfg, indent=4))

        cmd = top_cfg["simget_home"] + "/bin/perf_restore_cnt " + \
            str(target_dir) + "_restore_cfg.json"
        if run == True:
            result = subprocess.getoutput(cmd)
            print(result)
            result_list.append(int(i)
                               for i in result.split(':')[-1].split(' '))
        else:
            print(cmd)
        idx += 1

    if clean == True:
        for directory in filter(os.path.isdir, os.listdir(os.getcwd())):
            if directory not in least_offset_list:
                shutil.rmtree(directory)

    os.chdir(return_dir)

    if run == True:
        i_all = 0
        c_all = 0
        for [i, c] in result_list:
            i_all += int(i)
            c_all += int(c)

        return i_all/c_all
    else:
        return 0


def calc_criu_all(top_cfg, run, clean, bias_check):
    # calc criu restored(according to simpoint) ipc result for all checkpoint
    # if one simpoint has more than one checkpoint, it will restore the nearest one
    if os.path.exists(top_cfg["dir_out"]) == False:
        print("no folder exists!")
        return
    simpoint_cfg_prefix = get_simpoint_cfg_prefix(top_cfg)
    simpoint_warm_cfg_prefix = get_simpoint_cfg_prefix(top_cfg, True)

    os.chdir(top_cfg["dir_out"])
    target_file = open(simpoint_warm_cfg_prefix+"criu_res.log", 'a')
    print(time.asctime(time.localtime(time.time())),
          "===================================================", file=target_file)
    print("\t\t\t\t"+str(top_cfg["interval_size"]/1000000) +
          'M', top_cfg["warmup_ratio"], file=target_file)
    print("test", "input", "simpoints", "full-ipc", "criu-simpoint-ipc",
          "criu-simpoint-ipc-err(%)", file=target_file)
    for dirname in filter(os.path.isdir, os.listdir(os.getcwd())):
        print(dirname, file=target_file, end=' ')
        print(dirname)
        if top_cfg["partial_test"] == True and dirname not in top_cfg["user_test_list"]:
            print(file=target_file)
            continue
        os.chdir(dirname)
        for inputs in filter(os.path.isdir, os.listdir(os.getcwd())):
            if inputs != "run1":
                print('\t', file=target_file, end=' ')
            print(inputs, file=target_file, end=' ')
            print(inputs)
            os.chdir(inputs)
            criu_simpoint_ipc = 0.0
            full_ipc = 0.0
            with open(simpoint_warm_cfg_prefix+"loop_cfg.json", "r") as local_cfg_file:
                local_cfg = json.load(local_cfg_file)
                print(local_cfg["simpoint"]["k"], file=target_file, end=' ')
                try:
                    criu_simpoint_ipc = calc_criu_simpoint(
                        top_cfg, local_cfg, run, clean, bias_check)
                except ValueError:
                    print("run error!\n\n", file=target_file)
                    os.chdir("..")
                    continue

            with open(simpoint_cfg_prefix+"loop_intervals.log", "r") as loop_log_file:
                [total_insts, total_cycles] = loop_log_file.readlines()[-1].split(" ")
                full_ipc = int(total_insts)/int(total_cycles)

            print("{:.4f}".format(full_ipc), "{:.4f}".format(
                criu_simpoint_ipc), file=target_file, end=' ')
            print("{:.2f}".format((criu_simpoint_ipc - full_ipc) /
                                  full_ipc*100), file=target_file)

            os.chdir("..")

        os.chdir("..")
    target_file.close()
    return
