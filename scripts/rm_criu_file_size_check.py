import os
import sys
import json
import subprocess
from multiprocessing import Pool
from multiprocessing import cpu_count


def rm_criu_file_size_check(cfg, ignore_list, run_path):
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


def rm_criu_file_size_check_loop(cfg, ignore_list):
    if os.path.exists(cfg["dir_out"]) == False:
        print("no folder exists!")
        return
    pool = Pool(int(cpu_count()/2))
    simpoint_cfg_prefix = str(int(cfg["interval_size"]/1000000)) + \
        'M_max'+str(cfg["simpoint"]["maxK"])+"_"
    os.chdir(cfg["dir_out"])
    for dirname in filter(os.path.isdir, os.listdir(os.getcwd())):
        if cfg["partial_test"] == True and dirname not in cfg["user_test_list"]:
            continue
        os.chdir(dirname)
        for inputs in filter(os.path.isdir, os.listdir(os.getcwd())):
            os.chdir(inputs)
            dump_path = simpoint_cfg_prefix+"dump" + \
                str(cfg["perf_config"]["dump_idx"])
            if os.path.exists(dump_path) == False:
                os.chdir("..")
                continue

            os.chdir(dump_path)
            for checkpoint in filter(os.path.isdir, os.listdir(os.getcwd())):
                pool.apply_async(func=rm_criu_file_size_check,
                                 args=[cfg, ignore_list, os.path.join(os.getcwd(), checkpoint)])

            os.chdir("..")
            os.chdir("..")
        os.chdir("..")
    pool.close()
    print("waiting for processing...")
    pool.join()

    return


def gettestpath(cfg, testname):
    spec2k_int = ["164.gzip", "181.mcf", "252.eon", "255.vortex", "175.vpr", "186.crafty",
                  "253.perlbmk", "256.bzip2", "176.gcc", "197.parser", "254.gap", "300.twolf"]
    spec2k_fp = ["168.wupwise", "178.galgel", "189.lucas", "171.swim", "179.art", "191.fma3d", "172.mgrid",
                 "183.equake", "200.sixtrack", "173.applu", "187.facerec", "301.apsi", "177.mesa", "188.ammp"]
    spec2k_all = spec2k_int+spec2k_fp
    int_path = os.path.join(cfg["spec_home"], "benchspec/CINT2000/")
    fp_path = os.path.join(cfg["spec_home"], "benchspec/CFP2000/")

    if testname in spec2k_int:
        return int_path
    elif testname in spec2k_fp:
        return fp_path


def gen_ignore_list(cfg, ignore_cfg):
    ignore_list = []
    for entry in ignore_cfg["ignore_list"]:
        for name in entry["name"]:
            ignore_list.append(os.path.join(gettestpath(cfg,
                                                        entry["test"]), entry["test"], "run", cfg["spec_run"], name))
    return ignore_list


top_cfg = {}
ignore_cfg = {}
with open(sys.argv[1], 'r') as cfg_file:
    top_cfg = json.load(cfg_file)
with open(sys.argv[2], 'r') as ignore_file:
    ignore_cfg = json.load(ignore_file)


ignore_list = gen_ignore_list(top_cfg, ignore_cfg)
# rm_criu_file_size_check(top_cfg, ignore_list)
rm_criu_file_size_check_loop(top_cfg, ignore_list)
