import os
import sys
import json
import argparse
from simget_util import *
from cfg_file_gen import gen_run_cmd_list, run_valgrind, run_simpoint, gen_perf_loop_cfg_file
from loop_test import run_loop_test, calc_loop_result
from criu_file_size_check_remove import gen_ignore_list, rm_criu_file_size_check_all
from criu_calc import dump_criu_all, calc_criu_all, calc_criu_simpoint

parser = argparse.ArgumentParser()
parser.add_argument("-top", help="top config", required=True)
parser.add_argument("-ignore", help="criu file check ignore list config")
parser.add_argument("-local", help="local config for perf* binary")
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

# # full flow:


# cmd_list = gen_run_cmd_list(top_cfg, get_test_list(top_cfg))
# run_valgrind(top_cfg, cmd_list, True)
# run_simpoint(top_cfg, True)
# gen_perf_loop_cfg_file(top_cfg, cmd_list)

# run_loop_test(top_cfg, True)
# calc_loop_result(top_cfg)

# dump_criu_all(top_cfg, True)
# ignore_list = gen_ignore_list(top_cfg, criu_rm_cfg)
# rm_criu_file_size_check_all(top_cfg, ignore_list)
calc_criu_all(top_cfg, True, True)
