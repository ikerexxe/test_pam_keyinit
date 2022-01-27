src = $(wildcard *.c)
obj = $(src:.c=.o)

CFLAGS = -g

testprog: $(obj)
	$(CC) -lpamtest -lcmocka -o $@ $^ $(LDFLAGS)

test:
	LD_PRELOAD=libpam_wrapper.so \
		PAM_WRAPPER=1 \
		PAM_WRAPPER_SERVICE_DIR=./myapp \
		PAM_WRAPPER_DEBUGLEVEL=9 \
		./testprog

.PHONY: clean
clean:
	rm -f $(obj) testprog
