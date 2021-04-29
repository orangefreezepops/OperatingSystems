OBJS	:= hello cs1550
DISK	:= .disk
MNTPNT	:= testmount
CFLAGS	:= -g3 -O0 -Wall -Wextra -Wno-unused-parameter $(shell pkg-config --cflags fuse)
LIBS	:= $(shell pkg-config --libs fuse)

.PHONY: all clean debug unmount example test

all: $(OBJS) $(DISK)

clean: unmount
	rm -rf $(OBJS) $(OBJS:=.d) $(DISK) $(MNTPNT)

debug: all $(MNTPNT) unmount
	./cs1550 -d $(MNTPNT)

unmount:
	-fusermount -u $(MNTPNT)

test1: clean all $(MNTPNT) unmount
	-./cs1550 $(MNTPNT)
	-./script-1.sh

test2: clean all $(MNTPNT) unmount
	-./cs1550 $(MNTPNT)
	-./script-2.sh

test3: clean all $(MNTPNT) unmount
	-./cs1550 $(MNTPNT)
	-./script-3.sh

test4: clean all $(MNTPNT) unmount
	-./cs1550 $(MNTPNT)
	-./script-4.sh

test: test1 test2 test3 test4

example: hello $(MNTPNT) unmount
	-./hello $(MNTPNT)

-include $(OBJS:=.d)

$(MNTPNT):
	-mkdir $(MNTPNT)

$(DISK):
	dd bs=1K count=5K if=/dev/zero of=$(DISK)

%: %.c
	$(CC) $< $(CFLAGS) $(LIBS) -MMD -o $@
