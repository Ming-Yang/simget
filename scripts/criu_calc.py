import json
import sys
import os
import subprocess
import time
import shutil
import glob
import re
from simget_util import get_test_path, get_simpoint_cfg_prefix, print_criu_result


def get_criu_json_filename(prefix, target=True):
    '''
    criu result file name increase 1 each time
    Args: 
        prefix: string, @request
            criu result file name prefix
        target: bool, @optional
            generate next if true, generate current last result file name otherwise
    '''
    file_list = glob.glob(prefix+"criu_res_*.json")
    if file_list:
        num = [int(n.split('.')[0].split('_')[-1]) for n in file_list]
        num.sort()
        return prefix + "criu_res_" + str(num[-1] + (1 if target else 0)) + ".json"
    return prefix + "criu_res_0.json"


def dump_criu_all(top_cfg, run=False, bias_check=True, bias_clean=True):
    '''
    dump all criu checkpoint into a folder named by checkpoint insts
    Args:
        top_cfg: dict, @request
            base config file, see top_cfg_example.json
        run: bool, @optional
            run perf_loop_dump if true, print commond otherwise
        bias_check: bool, @optional
            check if dump point is correct(the instructions error between a dumped point and simpoint results is less than an interval)
        bias_clean: bool, @optional
            remove the image if the bias is too large
    '''
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

            loop_cfg_filename = simpoint_warm_cfg_prefix+"loop_cfg.json"
            with open(loop_cfg_filename, 'r') as f:
                loop_cfg = json.load(f)
            os.chdir(simpoint_warm_cfg_prefix+"dump")
            dir_list = filter(os.path.isdir, os.listdir(os.getcwd()))
            dir_list = [int(item) for item in dir_list]
            least_offset_list = []
            idx = 0
            for point in loop_cfg["simpoint"]["points"]:
                o_cfg = loop_cfg.copy()
                o_cfg["simpoint"]["current"] = idx
                if point < int(loop_cfg["process"]["warmup_ratio"]):
                    target = 0
                else:
                    target = (point-int(loop_cfg["process"]["warmup_ratio"])) * \
                        int(loop_cfg["process"]["ov_insts"])
                min_abs = sys.maxsize
                for d in dir_list:
                    if min_abs > abs(target-d):
                        min_abs = abs(target-d)
                        target_dir = d
                if bias_check == True and min_abs > int(loop_cfg["process"]["ov_insts"]):
                    print("no suitable checkpoint for", dirname, inputs, idx)
                    idx += 1
                    continue

                least_offset_list.append(str(target_dir))
                o_cfg["image_dir"] = os.path.join(
                    loop_cfg["image_dir"], str(target_dir))

                files = os.listdir(str(target_dir))
                for img_file in files:
                    pids = re.findall(r"mm-(\d+)\.img", img_file)
                    if pids:
                        o_cfg["process"]["pid"] = int(pids[0])
                        break
                with open(str(target_dir) + "_restore_cfg.json", 'w') as f:
                    json.dump(o_cfg, f, indent=4)
                idx += 1

            for f in os.listdir(os.getcwd()):
                if len(re.findall(r"\d+", f)) > 0 and re.findall(r"\d+", f)[0] not in least_offset_list:
                    print("remove checkpoint ", f)
                    if bias_clean == True:
                        if os.path.isdir(f) == True:
                            shutil.rmtree(f)
                        else:
                            os.remove(f)
                # elif clean == True and os.path.exists(directory+"_restore_cfg.json") == False:
                #     shutil.rmtree(directory)

            os.chdir("../..")
        os.chdir("..")
    return


def calc_criu_simpoint(top_cfg, local_cfg, run=False):
    '''
    calc criu restored(according to simpoint) ipc result, calc one testcase once call
    Args:
        top_cfg: dict, @request
            base config file, see top_cfg_example.json
        local_cfg: dict, @request
            config file used for current checkpoint restore
        run: bool, @optional
            run perf_restore_cnt if true, print commond otherwise
    '''
    result_list = []
    return_dir = os.getcwd()
    os.chdir(local_cfg["image_dir"])

    for cfg_filename in glob.glob("*.json"):
        print(cfg_filename.split('_')[0], end='\t')
        sys.stdout.flush()
        cmd = top_cfg["simget_home"] + "/bin/perf_restore_cnt " + cfg_filename
        if run == True:
            ret = subprocess.run(cmd, stdout=subprocess.PIPE, shell=True)
            result = ret.stdout.decode()
            print(result, end='')
            result_list.append(float(i) if '.' in i else int(i)
                               for i in result.split(':')[-1].split(' '))
        else:
            print(cmd)

    os.chdir(return_dir)

    if run == True:
        i_all = 0
        c_all = 0
        for [i, c, weight] in result_list:
            i_all += int(i)
            c_all += int(c)*weight

        return i_all, c_all, i_all/len(result_list)/c_all
    else:
        return 0, 0, 0


def calc_criu_all(top_cfg, run=False):
    '''
    calculater criu restored(according to simpoint) ipc result for all checkpoint
    if one simpoint has more than one checkpoint, it will restore the nearest one
    Args:
        top_cfg: dict, @request
            base config file, see top_cfg_example.json
        run: bool, @optional
            run perf_restore_cnt if true, print commond otherwise, pass to calc_criu_simpoint
    '''
    if os.path.exists(top_cfg["dir_out"]) == False:
        print("no folder exists!")
        return
    simpoint_warm_cfg_prefix = get_simpoint_cfg_prefix(top_cfg, True)

    os.chdir(top_cfg["dir_out"])
    criu_res_dict = {}
    criu_res_dict["time"] = time.asctime(time.localtime(time.time()))
    criu_res_dict["warmup_ratio"] = top_cfg["warmup_ratio"]
    criu_res_dict["interval_size"] = top_cfg["interval_size"]
    criu_res_dict["maxK"] = top_cfg["simpoint"]["maxK"]
    with open(get_criu_json_filename(simpoint_warm_cfg_prefix), 'w') as json_file:
        for dirname in filter(os.path.isdir, os.listdir(os.getcwd())):
            if top_cfg["partial_test"] == True and dirname not in top_cfg["user_test_list"]:
                continue
            os.chdir(dirname)
            criu_res_dict[dirname] = {}
            for inputs in filter(os.path.isdir, os.listdir(os.getcwd())):
                os.chdir(inputs)
                criu_res_dict[dirname][inputs] = {}
                try:
                    local_cfg_file = open(
                        simpoint_warm_cfg_prefix+"loop_cfg.json", "r")
                    local_cfg = json.load(local_cfg_file)
                    criu_res_dict[dirname][inputs]["points"] = local_cfg["simpoint"]["k"]
                    criu_res_dict[dirname][inputs]["size"] = subprocess.getoutput(
                        "du -sh "+local_cfg["image_dir"]).split()[0]
                    print(dirname, inputs)
                    criu_res_dict[dirname][inputs]["insts"], \
                        criu_res_dict[dirname][inputs]["cycles"], \
                        criu_res_dict[dirname][inputs]["ipc"] = \
                        calc_criu_simpoint(top_cfg, local_cfg, run)
                    print("ipc:", criu_res_dict[dirname][inputs]["ipc"])

                except Exception as exc:
                    print("run error!", exc)

                os.chdir("..")
            os.chdir("..")

        json.dump(criu_res_dict, json_file, indent=4)

    print_criu_result(top_cfg, get_criu_json_filename(
        simpoint_warm_cfg_prefix, False))
    return
