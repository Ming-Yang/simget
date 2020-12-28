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
unshare -p -m --fork --mount-proc 有些情况下，pid会被kthread占用，利用该命令可以新建一个pid namespace，避免冲突

## bugs
1. spec2k 187.facerec 程序执行过程中会不断输出到文件。restore和save的时候，文件大小是不一样的。其他测试可能有一样的问题。最好的方式是每次dump的同时保存硬盘下的文件镜像\
相同问题的文件还有：
* 187.facerec/run/00000002/hops.out
* 176.gcc/run/00000002/166.s integrate.s 200.s scilab.s expr.s
* 177.mesa/run/00000002/mesa.lo
* 300.twolf/run/00000002/ref.out
* 301.apsi/run/00000002/APV
* 191.fma3d/run/00000002/fmaelo.out
* 200.sixtrack/run/00000002/fort.31
* 171.swim/run/00000002/SWIM7
2. 如果用FIFO调度，perf计算的时钟周期比普通要高，但是总体的运行时间要低
3. valgrind的计数结果和perf有出入，有多有少，且差距很大，1e-5以上。不同x86机器上同一个二进制用valgrind跑的结果也有差距，1e-8