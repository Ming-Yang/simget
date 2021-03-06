SRC_DIR = src/
INC_DIR = include/
OBJ_DIR = bin/

CC = gcc

CC_OPT = -O3 -std=gnu99
LD_OPT = -lcriu
# available macros: _DEBUG
DEF_OPT = -D_DEBUG

CJSON = ${SRC_DIR}cJSON.c

target: perf_loop_dump perf_restore_cnt

extra: perf_ov_dump perf_ov_restore perf_ov_loop perf_count test 

all: target extra criu_dump criu_restore ptrace

perf_ov_dump:$(addprefix ${SRC_DIR},perf_ov_dump.c criu_util.c util.c) ${CJSON}
	${CC} -o ${OBJ_DIR}$@ $^ ${CC_OPT} -I${INC_DIR} ${DEF_OPT} ${LD_OPT}

perf_ov_restore:$(addprefix ${SRC_DIR},perf_ov_restore.c criu_util.c util.c) ${CJSON}
	${CC} -o ${OBJ_DIR}$@ $^ ${CC_OPT} -I${INC_DIR} ${DEF_OPT} ${LD_OPT}

perf_restore_cnt:$(addprefix ${SRC_DIR},perf_restore_cnt.c criu_util.c util.c) ${CJSON}
	${CC} -o ${OBJ_DIR}$@ $^ ${CC_OPT} -I${INC_DIR} ${DEF_OPT} ${LD_OPT}

perf_ov_sig:perf_ov_sig.c test
	${CC} -o $@ $^

perf_ov_fd:perf_ov_fd.c test
	${CC} -o $@ $^

perf_count:$(addprefix ${SRC_DIR},perf_count.c util.c) ${CJSON}
	${CC} -o ${OBJ_DIR}$@ $^ ${CC_OPT} -I${INC_DIR} ${DEF_OPT}

perf_ov_loop:$(addprefix ${SRC_DIR},perf_ov_loop.c util.c) ${CJSON}
	${CC} -o ${OBJ_DIR}$@ $^ ${CC_OPT} -I${INC_DIR} ${DEF_OPT}

perf_loop_dump:$(addprefix ${SRC_DIR},perf_loop_dump.c criu_util.c util.c) ${CJSON}
	${CC} -o ${OBJ_DIR}$@ $^ ${CC_OPT} -I${INC_DIR} ${DEF_OPT} ${LD_OPT}

perf_loop_nodump:$(addprefix ${SRC_DIR},perf_loop_nodump.c criu_util.c util.c) ${CJSON}
	${CC} -o ${OBJ_DIR}$@ $^ ${CC_OPT} -I${INC_DIR} ${DEF_OPT} ${LD_OPT}

perf_example:$(addprefix ${SRC_DIR},perf_example.c) 
	${CC} -o ${OBJ_DIR}$@ $^ ${CC_OPT} -I${INC_DIR} ${DEF_OPT}

criu_dump:$(addprefix ${SRC_DIR},criu_dump.c criu_util.c util.c) ${CJSON}
	${CC} -o ${OBJ_DIR}$@ $^  ${CC_OPT} -I${INC_DIR} ${DEF_OPT} ${LD_OPT}

criu_restore:$(addprefix ${SRC_DIR},criu_restore.c criu_util.c)
	${CC} -o ${OBJ_DIR}$@ $^  ${CC_OPT} -I${INC_DIR} ${DEF_OPT} ${LD_OPT}

test:$(addprefix ${SRC_DIR},test.c util.c) ${CJSON}
	${CC} -o ${OBJ_DIR}$@ ${CC_OPT} -g $^ -I${INC_DIR}

ptrace:$(addprefix ${SRC_DIR},ptrace_test.c)
	${CC} -o ${OBJ_DIR}$@ ${CC_OPT} $^

.PHONY:clean
clean:
	sudo rm -r bin
