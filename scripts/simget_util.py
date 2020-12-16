import json
import os
import sys

spec2k_int = ["164.gzip", "181.mcf", "252.eon", "255.vortex", "175.vpr", "186.crafty",
              "253.perlbmk", "256.bzip2", "176.gcc", "197.parser", "254.gap", "300.twolf"]
spec2k_fp = ["168.wupwise", "178.galgel", "189.lucas", "171.swim", "179.art", "191.fma3d", "172.mgrid",
             "183.equake", "200.sixtrack", "173.applu", "187.facerec", "301.apsi", "177.mesa", "188.ammp"]
spec2k_all = spec2k_int+spec2k_fp


def get_test_list(top_cfg):
    if top_cfg["partial_test"] == True:
        return top_cfg["user_test_list"]
    elif top_cfg["spec_version"] == 2000:
        return spec2k_all


def get_test_path(top_cfg, testname):
    # test path specificed by testset

    int_path = os.path.join(top_cfg["spec_home"], "benchspec/CINT2000/")
    fp_path = os.path.join(top_cfg["spec_home"], "benchspec/CFP2000/")

    if testname in spec2k_int:
        return int_path
    elif testname in spec2k_fp:
        return fp_path


def get_simpoint_cfg_prefix(top_cfg, warmup=False):
    # get simpoint cfg prefix use intervals and maxK
    if warmup == True:
        return str(int(top_cfg["interval_size"]/1000000)) + \
            "M_max" + str(top_cfg["simpoint"]["maxK"]) + \
            "_warm" + str(int(top_cfg["warmup_ratio"])) + '_'
    else:
        return str(int(top_cfg["interval_size"]/1000000)) + \
            "M_max" + str(top_cfg["simpoint"]["maxK"]) + '_'


def print_fixed_result(top_cfg):
    filename = get_simpoint_cfg_prefix(top_cfg)+"fix_res.log"
    outfile = open(os.path.join(top_cfg["dir_out"], filename), 'a')
    print_head = True
    with open(os.path.join(top_cfg["dir_out"], "fix_test_res.json"), 'r') as f:
        res_dict = json.load(f)
        print(res_dict["time"], res_dict["platform"], file=outfile)
        for case in res_dict:
            if case[0].isdigit() == False:
                continue
            for run in res_dict[case]:
                if print_head:
                    print("testcase", end='\t', file=outfile)
                    for ele in res_dict[case][run]:
                        for item in res_dict[case][run][ele]:
                            print(ele+':'+item, end='\t', file=outfile)
                    print(file=outfile)
                    print_head = False
                print(case+'\\'+run, end='\t', file=outfile)
                for ele in res_dict[case][run]:
                    for res in res_dict[case][run][ele]:
                        print(round(res_dict[case][run][ele]
                                    [res], 3), end='\t', file=outfile)
                print(file=outfile)
    print(file=outfile)


def print_criu_result(top_cfg, json_filename):
    filename = get_simpoint_cfg_prefix(top_cfg)+"criu_res.log"
    outfile = open(os.path.join(top_cfg["dir_out"], filename), 'a')
    with open(os.path.join(top_cfg["dir_out"], json_filename), 'r') as f:
        res_dict = json.load(f)
        print(res_dict["time"], "interval size:", res_dict["interval_size"],
              "warmup_ratio:", res_dict["warmup_ratio"], "maxK:", res_dict["maxK"], file=outfile)
        print("testcase\tcriu:points\tcriu:image size\tcriu:ipc\tcriu:insts\tcriu:weighted cycles", file=outfile)
        for case in res_dict:
            if case[0].isdigit() == False:
                continue
            for run in res_dict[case]:
                print(case+'\\'+run, end='\t', file=outfile)
                if "points" in res_dict[case][run]:
                    print(res_dict[case][run]["points"], '\t',
                          res_dict[case][run]["size"], end='\t', file=outfile)
                if "ipc" in res_dict[case][run]:
                    print(res_dict[case][run]["ipc"], '\t', res_dict[case][run]["insts"],
                          '\t', res_dict[case][run]["cycles"], end='', file=outfile)
                print(file=outfile)
    print(file=outfile)
