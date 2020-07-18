
target: perf_ov_dump perf_ov_restore


all: perf_ov_fd perf_ov_sig perf_count criu_dump criu_restore perf_ov_dump perf_ov_restore

perf_ov_dump:perf_ov_dump.c criu_util.c test
	gcc -o $@ $^ -lcriu

perf_ov_restore:perf_ov_restore.c criu_util.c test
	gcc -o $@ $^ -lcriu

perf_ov_sig:perf_ov_sig.c test
	gcc -o $@ $^

perf_ov_fd:perf_ov_fd.c test
	gcc -o $@ $^

perf_count:perf_count.c
	gcc -o $@ $^

criu_dump:criu_dump.c criu_util.c
	gcc -o $@ $^ -lcriu

criu_restore:criu_restore.c criu_util.c
	gcc -o $@ $^ -lcriu

test:test.c
	gcc -o $@ -g $^
	objdump -alDS $@ > test.s

# .PHONY
# clean:
# 	rm