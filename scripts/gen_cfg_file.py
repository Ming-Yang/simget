import json
import sys
import os
import subprocess
from multiprocessing import Pool
from multiprocessing import cpu_count

cfg = {}
with open(sys.argv[1], 'r') as cfg_file:
    cfg = json.load(cfg_file)
    simpoint_cfg_prefix = str(int(cfg["interval_size"]/1000000)) + \
        'M_max'+str(cfg["simpoint"]["maxK"])+"_"


if cfg["spec_version"] == 2000:
    spec2k_int = ["164.gzip", "181.mcf", "252.eon", "255.vortex", "175.vpr", "186.crafty",
                  "253.perlbmk", "256.bzip2", "176.gcc", "197.parser", "254.gap", "300.twolf"]
    spec2k_fp = ["168.wupwise", "178.galgel", "189.lucas", "171.swim", "179.art", "191.fma3d", "172.mgrid",
                 "183.equake", "200.sixtrack", "173.applu", "187.facerec", "301.apsi", "177.mesa", "188.ammp"]
    spec2k_all = spec2k_int+spec2k_fp
    int_path = os.path.join(cfg["spec_home"], "benchspec/CINT2000/")
    fp_path = os.path.join(cfg["spec_home"], "benchspec/CFP2000/")
    invoke_path = os.path.join(cfg["spec_home"], "bin/")
    tail_path = os.path.join("run/", cfg["spec_run"], "speccmds.cmd")

invoke_cmd = invoke_path+"specinvoke -n "


def valgrind_cmd(cfg, bb_file):
    return os.path.join(cfg["valgrind"]["bin_path"], "valgrind") + " --tool=exp-bbv --bb-out-file=" + bb_file + " --interval-size=" + str(cfg["interval_size"])


def simpoint_cmd(cfg, bb_file, points, weights):
    return os.path.join(cfg["simpoint"]["bin_path"], "simpoint ") + " -loadFVFile " + bb_file + " -maxK " + str(cfg["simpoint"]["maxK"]) + " -saveSimpoints " + points + " -saveSimpointWeights " + weights

# use specinvoke to parse speccmds.cmd to raw commond and input file
#
# example input:
# # Starting run for user #0
# ../00000002/parser_base.Ofast.static 2.1.dict -batch < ref.in > ref.out 2>> ref.err
# example output:
# [absolute/address/parser_base.Ofast.static 2.1.dict -batch, ref.in]
# return value:
# [[{path, cmd, inputfile}, {path, cmd, inputfile}], [{path, cmd, inputfile}]]
# one test multi runs


def gen_run_cmd_list(test_list, test_path):
    spec_run_cmd_list = []
    for test_name in test_list:
        if os.path.exists(os.path.join(test_path, test_name, tail_path)) == False:
            print(os.path.join(test_path, test_name, tail_path), "not exist")
            continue
        result = subprocess.getoutput(
            invoke_cmd + os.path.join(test_path, test_name, tail_path))

        cmd_list = []
        for line in result.splitlines():
            if line[0] == '#':
                continue

            dry_cmd = line.strip().split(">")[0]

            cmd_item = {}
            cmd_item["path"] = os.path.join(
                test_path, test_name, "run", cfg["spec_run"])
            cmd_item["run"] = os.path.split(dry_cmd)[1]

            if dry_cmd.find("<") != -1:
                cmd_item["input_file"] = dry_cmd.strip().split("<")[1]
                cmd_item["run"] = cmd_item["run"].strip().split("<")[0]
            else:
                cmd_item["input_file"] = None

            cmd_list.append(cmd_item)

        spec_run_cmd_list.append(cmd_list)
    return spec_run_cmd_list


def run_valgrind(cmd_list, cfg):
    points_list = []
    weights_list = []

    if os.path.exists(cfg["dir_out"]) == False:
        os.mkdir(cfg["dir_out"])
    os.chdir(cfg["dir_out"])

    pool = Pool(int(cpu_count()/2))

    for cmd_set in cmd_list:
        test_name = cmd_set[0]["path"].split('/')[-3]
        if os.path.exists(test_name) == False:
            os.mkdir(test_name)
        os.chdir(test_name)

        i = 0
        for cmd in cmd_set:
            i = i+1
            save_dir = "run" + str(i)
            if os.path.exists(save_dir) == False:
                os.mkdir(save_dir)
            cur_dir = os.getcwd()

            bb_file = os.path.join(cur_dir, save_dir, "valgrind.bb")

            valgrind_full_cmd = valgrind_cmd(cfg, bb_file) + " ./" + cmd["run"]
            if cmd["input_file"] != None:
                valgrind_full_cmd += " < " + cmd["input_file"]
            valgrind_full_cmd += " > std.out 2>> std.err"

            # pool.apply_async(func=sys.stdout.write, args=valgrind_full_cmd)
            # sub_run_valgrind = subprocess.run(valgrind_full_cmd, shell=True, cwd=cmd["path"])
            pool.apply_async(func=subprocess.run, kwds={
                             "args": valgrind_full_cmd, "shell": True, "cwd": cmd["path"]})

        os.chdir("..")

    pool.close()
    print("waiting for calc...")
    pool.join()
    return


def run_simpoint(cfg):
    pool = Pool(int(cpu_count()/2))

    if os.path.exists(cfg["dir_out"]) == False:
        print("no folder exists!")
        return
    os.chdir(cfg["dir_out"])
    for dirname in os.listdir("./"):
        os.chdir(dirname)
        for inputs in os.listdir("./"):
            os.chdir(inputs)
            simpoint_full_cmd = simpoint_cmd(
                cfg, "valgrind.bb", simpoint_cfg_prefix+"sim.points", simpoint_cfg_prefix+"sim.weights")
            # pool.apply_async(func=sys.stdout.write, args=simpoint_full_cmd)
            pool.apply_async(func=subprocess.run, kwds={
                             "args": simpoint_full_cmd, "shell": True, "cwd": os.getcwd()})
            os.chdir("..")
        os.chdir("..")

    pool.close()
    print("waiting for calc...")
    pool.join()
    return


def gen_perf_loop_cfg_file(cmd_list, cfg):
    if os.path.exists(cfg["dir_out"]) == False:
        os.mkdir(cfg["dir_out"])
    os.chdir(cfg["dir_out"])

    for cmd_set in cmd_list:
        test_name = cmd_set[0]["path"].split('/')[-3]
        if os.path.exists(test_name) == False:
            os.mkdir(test_name)
        os.chdir(test_name)

        i = 0
        for cmd in cmd_set:
            i = i+1
            save_dir = "run" + str(i)
            if os.path.exists(save_dir) == False:
                os.mkdir(save_dir)
            os.chdir(save_dir)
            cur_dir = os.getcwd()

            process_cfg = {}
            process_cfg["path"] = cmd["path"]
            process_cfg["filename"] = cmd["run"].split(" ")[0]
            process_cfg["args"] = cmd["run"].strip().split(" ")[1:] + ["NULL"]
            if cmd["input_file"] != None:
                process_cfg["input_from_file"] = 1
                process_cfg["file_in"] = os.path.join(cmd["path"], cmd["input_file"].strip())
            process_cfg["affinity"] = cfg["perf_config"]["affinity"]
            process_cfg["ov_insts"] = str(cfg["interval_size"])
            process_cfg["irq_offset"] = cfg["perf_config"]["irq_offset"]

            loop_cfg = {}
            loop_cfg["out_file"] = os.path.join(
                cur_dir, simpoint_cfg_prefix + "loop_intervals.log")

            simpoint_cfg = {}
            simpoint_cfg["point_file"] = os.path.join(
                cur_dir, simpoint_cfg_prefix+"sim.points")
            simpoint_cfg["weight_file"] = os.path.join(
                cur_dir, simpoint_cfg_prefix+"sim.weights")

            perf_cfg = {}
            if os.path.exists(simpoint_cfg_prefix + "dump" + str(cfg["perf_config"]["dump_idx"])) == False:
                os.mkdir(simpoint_cfg_prefix + "dump" +
                         str(cfg["perf_config"]["dump_idx"]))
            perf_cfg["image_dir"] = os.path.join(
                cur_dir, simpoint_cfg_prefix + "dump" + str(cfg["perf_config"]["dump_idx"]))
            perf_cfg["process"] = process_cfg
            perf_cfg["loop"] = loop_cfg
            perf_cfg["simpoint"] = simpoint_cfg

            with open(simpoint_cfg_prefix + "loop_cfg.json", 'w') as f:
                f.write(json.dumps(perf_cfg, indent=4))

            os.chdir("..")
        os.chdir("..")
    return


spec2k_int_cmd_list = gen_run_cmd_list(spec2k_int, int_path)
spec2k_fp_cmd_list = gen_run_cmd_list(spec2k_fp, fp_path)
gen_perf_loop_cfg_file(spec2k_int_cmd_list, cfg)
gen_perf_loop_cfg_file(spec2k_fp_cmd_list, cfg)
# run_valgrind(spec2k_int_cmd_list, cfg)
# run_simpoint(cfg)
