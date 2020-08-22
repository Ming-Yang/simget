# Simget: 根据指令数进行用户态程序状态保存和恢复

## 依赖
* libcriu
* [cjson](https://github.com/DaveGamble/cJSON)

## 文件
* perf_example.c:手册中perf_event_open样例
* perf_count.c:统计一个子进程的指令数
* perf_ov_dump.c:根据指令数进行一次的CRIU save操作
* perf_ov_restore.c:根据恢复文件夹的位置进行一次CRIU restore操作
* perf_ov_loop.c:根据simpoint的interval，生成所有interval的insts,cycles
* perf_loop_dump.c:根据simpoint的采样点，将每个采样点前的程序状态dump到各自的文件夹下
* perf_restore_cnt.c:根据恢复文件夹的位置进行一次CRIU restore操作，并进行预热和insts,cycles统计

## 脚本
* gen_cfg_file.py:包含几个功能:
    1. valgrind获取bbv
    2. simpoint提取采样点
    3. 根据采样点生成cfg文件，可用于perf_ov_loop.c和perf_loop_dump.c
* loop_test.py:使用perf_ov_loop进行计算simpoint的结果


## todo

