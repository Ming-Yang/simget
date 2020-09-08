import json
import sys
import os
import re
import subprocess
from multiprocessing import Pool
from multiprocessing import cpu_count
from simget_util import get_test_path, get_simpoint_cfg_prefix


def valgrind_cmd(top_cfg, bb_file):
    return os.path.join(top_cfg["valgrind"]["bin_path"], "valgrind") + " --tool=exp-bbv --bb-out-file=" + bb_file + " --interval-size=" + str(top_cfg["interval_size"])


def simpoint_cmd(top_cfg, bb_file, points, weights):
    return os.path.join(top_cfg["simpoint"]["bin_path"], "simpoint ") + " -loadFVFile " + bb_file + " -maxK " + str(top_cfg["simpoint"]["maxK"]) + " -saveSimpoints " + points + " -saveSimpointWeights " + weights


def gen_run_cmd_list(top_cfg, test_list):
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
    spec_run_cmd_list = []
    invoke_cmd = os.path.join(top_cfg["spec_home"], "bin/specinvoke") + " -n "
    tail_path = os.path.join("run", top_cfg["spec_run"], "speccmds.cmd")
    for test_name in test_list:
        test_path = get_test_path(top_cfg, test_name)
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
                test_path, test_name, "run", top_cfg["spec_run"])
            cmd_item["run"] = os.path.split(dry_cmd)[1]

            if dry_cmd.find("<") != -1:
                cmd_item["input_file"] = dry_cmd.strip().split("<")[1]
                cmd_item["run"] = cmd_item["run"].strip().split("<")[0]
            else:
                cmd_item["input_file"] = None

            cmd_list.append(cmd_item)

        spec_run_cmd_list.append(cmd_list)
    return spec_run_cmd_list


def run_valgrind(top_cfg, cmd_list, run=False):
    # use valgrind to generate bbvs file
    points_list = []
    weights_list = []

    if os.path.exists(top_cfg["dir_out"]) == False:
        os.mkdir(top_cfg["dir_out"])
    os.chdir(top_cfg["dir_out"])

    pool = Pool(int(cpu_count()/2))

    for cmd_set in cmd_list:
        test_name = cmd_set[0]["path"].split('/')[-3]
        if top_cfg["partial_test"] == True and test_name not in top_cfg["user_test_list"]:
            continue

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

            valgrind_full_cmd = valgrind_cmd(
                top_cfg, bb_file) + " ./" + cmd["run"]
            if cmd["input_file"] != None:
                valgrind_full_cmd += " < " + cmd["input_file"]
            valgrind_full_cmd += " > std.out 2>> std.err"

            if run == False:
                pool.apply_async(func=sys.stdout.write, args=valgrind_full_cmd)
                if os.path.exists(bb_file):
                    re_insts = re.compile(r"#\s*Total instructions:\s*(\d+)")
                    with open(bb_file, "r") as bb:
                        for line in bb.readlines():
                            match_insts = re_insts.match(line)
                            if match_insts:
                                print(os.path.join(cur_dir, save_dir),
                                      match_insts.group(1))

            else:
                pool.apply_async(func=subprocess.run, kwds={
                    "args": valgrind_full_cmd, "shell": True, "cwd": cmd["path"]})

        os.chdir("..")

    pool.close()
    print("waiting for calc...")
    pool.join()
    return


def run_simpoint(top_cfg, run):
    # use simpoint to generate points
    simpoint_cfg_prefix = get_simpoint_cfg_prefix(top_cfg)
    pool = Pool(int(cpu_count()/2))

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
            simpoint_full_cmd = simpoint_cmd(
                top_cfg, "valgrind.bb", simpoint_cfg_prefix+"sim.points", simpoint_cfg_prefix+"sim.weights")
            if run == False:
                pool.apply_async(func=sys.stdout.write, args=simpoint_full_cmd)
            else:
                pool.apply_async(func=subprocess.run, kwds={
                    "args": simpoint_full_cmd, "shell": True, "cwd": os.getcwd()})
            os.chdir("..")
        os.chdir("..")

    pool.close()
    print("waiting for calc...")
    pool.join()
    return


def gen_perf_loop_cfg_file(top_cfg, cmd_list):
    # for each folder, generate a .json file which can be used by perf* binary files(also used by criu dump, but not used by criu restore)
    simpoint_cfg_prefix = get_simpoint_cfg_prefix(top_cfg)
    simpoint_warm_cfg_prefix = get_simpoint_cfg_prefix(top_cfg, True)

    if os.path.exists(top_cfg["dir_out"]) == False:
        os.mkdir(top_cfg["dir_out"])
    os.chdir(top_cfg["dir_out"])

    for cmd_set in cmd_list:
        test_name = cmd_set[0]["path"].split('/')[-3]
        if top_cfg["partial_test"] == True and test_name not in top_cfg["user_test_list"]:
            continue
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
            process_cfg["args"] = cmd["run"].strip().split(" ")[1:]
            if cmd["input_file"] != None:
                process_cfg["input_from_file"] = 1
                process_cfg["file_in"] = os.path.join(
                    cmd["path"], cmd["input_file"].strip())
            process_cfg["affinity"] = top_cfg["perf_config"]["affinity"]
            process_cfg["ov_insts"] = str(top_cfg["interval_size"])
            process_cfg["irq_offset"] = top_cfg["perf_config"]["irq_offset"]
            process_cfg["warmup_ratio"] = top_cfg["warmup_ratio"]

            loop_cfg = {}
            loop_cfg["out_file"] = os.path.join(
                cur_dir, simpoint_cfg_prefix + "loop_intervals.log")

            simpoint_cfg = {}
            simpoint_cfg["point_file"] = os.path.join(
                cur_dir, simpoint_cfg_prefix+"sim.points")
            simpoint_cfg["weight_file"] = os.path.join(
                cur_dir, simpoint_cfg_prefix+"sim.weights")

            k = 0
            point_weight_pair = []
            points = []
            weights = []
            with open(os.path.join(cur_dir, simpoint_cfg_prefix+"sim.points")) as points_file:
                for line in points_file.readlines():
                    point_weight_pair.append([int(line.split(' ')[0]), ])

            with open(os.path.join(cur_dir, simpoint_cfg_prefix+"sim.weights")) as weights_file:
                for line in weights_file.readlines():
                    point_weight_pair[k].append(float(line.split(' ')[0]))
                    k += 1

            point_weight_pair.sort(key=lambda x: x[0])
            for pair in point_weight_pair:
                points.append(pair[0])
                weights.append(pair[1])

            simpoint_cfg["k"] = k
            simpoint_cfg["points"] = points
            simpoint_cfg["weights"] = weights

            perf_cfg = {}
            if os.path.exists(simpoint_warm_cfg_prefix + "dump") == False:
                os.mkdir(simpoint_warm_cfg_prefix + "dump")
            perf_cfg["image_dir"] = os.path.join(
                cur_dir, simpoint_warm_cfg_prefix + "dump")
            perf_cfg["process"] = process_cfg
            perf_cfg["loop"] = loop_cfg
            perf_cfg["simpoint"] = simpoint_cfg

            with open(simpoint_warm_cfg_prefix + "loop_cfg.json", 'w') as f:
                f.write(json.dumps(perf_cfg, indent=4))

            os.chdir("..")
        os.chdir("..")
    return