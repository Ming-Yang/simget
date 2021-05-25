import os
import sys
import json
import argparse
from simget_util import *
from cfg_file_gen import gen_run_cmd_list, traverse_raw_cmd, run_simpoint, gen_perf_loop_cfg_file
from loop_test import run_loop_test, save_fixed_result
from criu_file_size_check_remove import gen_ignore_list, rm_criu_file_size_check_all
from criu_calc import dump_criu_all, calc_criu_all, calc_criu_simpoint

parser = argparse.ArgumentParser()
parser.add_argument("-top", help="top config", required=True)
parser.add_argument("-ignore", help="criu file check ignore list config")
parser.add_argument("-local", help="local config for perf* binary")
parser.add_argument("-times", help="loop run times")
parser.add_argument("-cmd", help="spec raw cmd json file")
args = parser.parse_args()

top_cfg = {}
criu_rm_cfg = {}
local_cfg = {}

with open(args.top, 'r') as cfg_file:
    top_cfg = json.load(cfg_file)

if args.ignore != None:
    cfg_file = open(args.ignore, 'r')
    criu_rm_cfg = json.load(cfg_file)

if args.local != None:
    cfg_file = open(args.local, 'r')
    local_cfg = json.load(cfg_file)

if args.times != None:
    loop_times = int(args.times)
else:
    loop_times = 1

if args.cmd == None:
    cmd_list = gen_run_cmd_list(top_cfg, get_test_list(top_cfg))
    with open("cmd_list.json", "w") as cmd_file:
        json.dump(cmd_list, cmd_file, indent=4)
else:
    with open(args.cmd, "r") as cmd_file:
        cmd_list = json.load(cmd_file)

# full flow example:


print("\n\n\n+++++++++++++++++++++control group+++++++++++++++++++++++++++++")
traverse_raw_cmd(top_cfg, cmd_list, "perf", False)
traverse_raw_cmd(top_cfg, cmd_list, "perf", True)
save_fixed_result(top_cfg)
print_fixed_result(top_cfg)

print("\n\n\n+++++++++++++++++++++designed platform+++++++++++++++++++++++++++++")
# print("\n++++++++++++++++++++++++pre++++++++++++++++++++++++++++++++++++++++")
# traverse_raw_cmd(top_cfg, cmd_list, "valgrind", False)
# traverse_raw_cmd(top_cfg, cmd_list, "valgrind", True)
# run_simpoint(top_cfg, True)
# gen_perf_loop_cfg_file(top_cfg, cmd_list)
# run_loop_test(top_cfg, False)

print("\n++++++++++++++++++++++++front end++++++++++++++++++++++++++++++++++")
dump_criu_all(top_cfg, run=True, bias_clean=False)
ignore_list = []
rm_criu_file_size_check_all(top_cfg, ignore_list)

print("\n++++++++++++++++++++++++back end++++++++++++++++++++++++++++++++++")
for i in range(1, loop_times+1):
    print("@run",i)
    calc_criu_all(top_cfg, True)
