# Target library
lib := libfs.a
objs := fs.o disk.o

CC		:= gcc
CFLAGS	:= -Wall -Wextra -Werror -MMD
ifneq ($(D),1)
CFLAGS	+= -O2
else
CFLAGS	+= -g
endif

ifneq ($(V),1)
Q = @
endif

all: $(lib) #original

deps := $(patsubst %.o,%.d,$(objs))
-include $(deps)

## TODO
libfs.a: $(objs)
	ar rcs libfs.a $(objs)

%.o: %.c %.h
	@echo "CC	$@"
	$(Q)$(CC) $(CFLAGS) -c -o $@ $<

clean: 
	@echo "CLEAN"
	$(Q)rm -f $(lib) $(objs) $(deps)

## TODO: Phase 1
