# Simget: 根据指令数进行用户态程序状态保存和恢复

## 依赖
* libcriu
* [cjson](https://github.com/DaveGamble/cJSON)

## 源文件
* perf_example.c:man手册中perf_event_open样例
* perf_count.c:统计一个子进程的指令数和周期数
* perf_ov_dump.c:根据指令数进行一次的CRIU save操作
* perf_ov_restore.c:根据恢复文件夹的位置进行一次CRIU restore操作
* perf_ov_loop.c:根据simpoint的interval，生成所有interval的insts,cycles，可用于手动选择目标片段计算，用于算法正确性的验证
* perf_loop_dump.c:根据simpoint的采样点，将每个采样点前的程序状态dump到各自的文件夹下
* perf_loop_nodump.c:同上，不进行dump操作
* perf_restore_cnt.c:根据恢复文件夹的位置进行一次CRIU restore操作，并进行预热和insts,cycles统计


## json配置文件
1. [perf_example.json](cfg/perf_example.json) 用于perf*二进制程序的输入配置
2. [top_cfg_example.json](script/top_cfg_example.json) 用于运行python脚本的必备配置文件


## python脚本
1. cfg_file_gen.py:
    * 根据spec提供的specinvoke程序和speccmds.cmd文件获得测试的原命令
    * 通过不同方式遍历测试点（valgrind、simpoint、perf、qemu-user、自定义指令）
    * 根据采样点生成cfg文件，可用于perf*程序
2. criu_calc.py:
    * 调用perf_loop_dump保存程序镜像点，并生成恢复用的配置文件
    * 调用perf_restore_cnt恢复镜像，并统计simpoint后的IPC结果
3. criu_file_size_check_remove.py:
    * 去掉criu中对文件大小和权限的检查
4. loop_test.py:
    * 使用perf_ov_loop进行计算simpoint的结果
5. simget_util.py:
    * 一些打印操作


## todo
unshare -p -m --fork --mount-proc 有些情况下，pid会被kthread占用，利用该命令可以新建一个pid namespace，避免冲突

## bugs
1. 文件size和mode检查的问题：
CRIU在恢复时会对文件size和mode进行检查，判断是否一致，对于某些输入输出文件，这个检查是不必要的，如：
* 187.facerec/run/00000002/hops.out
* 176.gcc/run/00000002/166.s integrate.s 200.s scilab.s expr.s
* 177.mesa/run/00000002/mesa.lo
* 300.twolf/run/00000002/ref.out
* 301.apsi/run/00000002/APV
* 191.fma3d/run/00000002/fmaelo.out
* 200.sixtrack/run/00000002/fort.31
* 171.swim/run/00000002/SWIM7
CRIU恢复时会检查所有相关文件，在不同机器上进行恢复时，其权限不同的概率很大。因此当前的脚本中利用crit decode和encode将所有的文件size和mode记录都去掉了（即当前igore_list不起效果，所有测试的size和mode权限都被ignore了）。但是需要注意，如果恢复时用到的执行文件没有运行权限的话，将会导致报错：sys_prctl(PR_SET_MM, PR_SET_MM_MAP) failed with 13。此时需要对可执行文件chmod +x
2. 3A4000上如果用FIFO调度，perf计算的时钟周期比普通要高，但是总体的运行时间要低，考虑到调度本身并不会引起计数误差，目前没有采用FIFO调度
3. valgrind的计数结果和perf有出入，有多有少，且差距很大，整体1e-5以上,PIN差距更大。不同x86机器上同一个二进制用valgrind跑的结果也有差距，1e-8
