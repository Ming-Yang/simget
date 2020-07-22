SRC_DIR = src/
INC_DIR = include/
OBJ_DIR = bin/

LD_OPT = -lcriu -lcjson

target: perf_ov_dump perf_ov_restore test

all: perf_ov_fd perf_ov_sig perf_count criu_dump criu_restore perf_ov_dump perf_ov_restore

perf_ov_dump:$(addprefix ${SRC_DIR},perf_ov_dump.c criu_util.c util.c) 
	gcc -o ${OBJ_DIR}$@ $^ -I${INC_DIR} ${LD_OPT}

perf_ov_restore:$(addprefix ${SRC_DIR},perf_ov_restore.c criu_util.c util.c) 
	gcc -o ${OBJ_DIR}$@ $^ -I${INC_DIR}  ${LD_OPT}

perf_ov_sig:perf_ov_sig.c test
	gcc -o $@ $^

perf_ov_fd:perf_ov_fd.c test
	gcc -o $@ $^

perf_count:$(addprefix ${SRC_DIR},perf_count.c util.c) 
	gcc -o ${OBJ_DIR}$@ $^ -I${INC_DIR}

criu_dump:criu_dump.c criu_util.c
	gcc -o $@ $^ -lcriu

criu_restore:criu_restore.c criu_util.c
	gcc -o $@ $^ -lcriu

test:${SRC_DIR}test.c
	gcc -o ${OBJ_DIR}$@ -g $^
	objdump -alDS ${OBJ_DIR}$@ > ${OBJ_DIR}test.s

# .PHONY
# clean:
# 	rm