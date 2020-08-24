import os
import sys
import json
import subprocess
from multiprocessing import Pool
from multiprocessing import cpu_count
from simget_util import get_test_path, get_simpoint_cfg_prefix


def rm_criu_file_size_check(ignore_list, run_path):
    # remove criu file size check in one files.img
    old_dir = os.getcwd()
    os.chdir(run_path)
    mod = False
    res = json.loads(subprocess.getoutput(
        "crit decode -i files.img"))
    for entry in res["entries"]:
        if "reg" in entry and "name" in entry["reg"] and entry["reg"]["name"] in ignore_list:
            if "size" in entry["reg"]:
                del entry["reg"]["size"]
                mod = True
    if mod == True:
        print(run_path)
        subprocess.run("crit encode -o files.img", shell=True,
                       input=bytearray(json.dumps(res), "utf8"))
    os.chdir(old_dir)
    return


def rm_criu_file_size_check_all(top_cfg, ignore_list):
    # remove criu file size check in ignore_list
    if os.path.exists(top_cfg["dir_out"]) == False:
        print("no folder exists!")
        return
    pool = Pool(int(cpu_count()/2))
    simpoint_warm_cfg_prefix = get_simpoint_cfg_prefix(top_cfg, True)
    os.chdir(top_cfg["dir_out"])
    for dirname in filter(os.path.isdir, os.listdir(os.getcwd())):
        if top_cfg["partial_test"] == True and dirname not in top_cfg["user_test_list"]:
            continue
        os.chdir(dirname)
        for inputs in filter(os.path.isdir, os.listdir(os.getcwd())):
            os.chdir(inputs)
            dump_path = simpoint_warm_cfg_prefix+"dump"
            if os.path.exists(dump_path) == False:
                os.chdir("..")
                continue

            os.chdir(dump_path)
            for checkpoint in filter(os.path.isdir, os.listdir(os.getcwd())):
                pool.apply_async(func = rm_criu_file_size_check,
                                 args = [ignore_list, os.path.join(os.getcwd(), checkpoint)])

            os.chdir("..")
            os.chdir("..")
        os.chdir("..")
    pool.close()
    print("waiting for processing...")
    pool.join()

    return


def gen_ignore_list(top_cfg, ignore_cfg):
    # generate ignore list by ignore cfg file
    ignore_list=[]
    for entry in ignore_cfg["ignore_list"]:
        for name in entry["name"]:
            ignore_list.append(os.path.join(get_test_path(top_cfg,
                                                        entry["test"]), entry["test"], "run", top_cfg["spec_run"], name))
    return ignore_list
