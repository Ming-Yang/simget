import json
import sys


cfg = {}
with open(sys.argv[1], 'r') as cfg_file:
    cfg = json.load(cfg_file)
    # print(cfg)


points = []
weights = []
intervals = []

with open(cfg["loop"]["out_file"]) as cfg_file:
    for line in cfg_file.readlines():
        data = line.strip().split(" ")
        data = [int(i) for i in data]
        intervals.append(data)

with open(cfg["simpoint"]["point_file"]) as cfg_file:
    for line in cfg_file.readlines():
        data = line.strip().split(" ")
        data = [int(i) for i in data]
        points.append(data)

with open(cfg["simpoint"]["weight_file"]) as cfg_file:
    for line in cfg_file.readlines():
        data = line.strip().split(" ")
        data = [float(i) for i in data]
        weights.append(data)

total_ipc = intervals[-1][0]/intervals[-1][1]

insts = 0
cycles = 0
for data in intervals[0:-1]:
    insts += data[0]
    cycles += data[1]
avg_interval_ipc = insts/cycles

insts = 0
cycles = 0
for point, weight in zip(points, weights):
    insts += intervals[point[0]][0]*weight[0]
    cycles += intervals[point[0]][1]*weight[0]
simpoint_ipc = insts/cycles

print("simpoins:",len(points))
print("total ip / avg interval ipc / simpoint ipc")
print(total_ipc, avg_interval_ipc, simpoint_ipc)
print("error:")
print((avg_interval_ipc-total_ipc)/total_ipc, (simpoint_ipc-total_ipc)/total_ipc)

