export SRC_DIR := $(CURDIR)/src
export INC_DIR := $(CURDIR)/include
export OBJ_DIR := $(CURDIR)/obj
export TEST_DIR := $(CURDIR)/test

ifeq ($(OS),Windows_NT)
	detected_os := WINDOWS
else
	detected_os := $(shell uname | tr '[:lower:]' '[:upper:]')
endif

export CC = gcc
export CFLAGS = -Wall -Wextra -Wreturn-type -Werror #-fsanitize=address
export CPPFLAGS = -I$(SRC_DIR) -DMC_PLATFORM_$(detected_os)
#export LDFLAGS = -fsanitize=address
export LDLIBS := -lcrypto -lz -lcurl

ifeq ($(detected_os),WINDOWS)
	CPPFLAGS += -IC:\msys64\ucrt64\include
	LDLIBS += -lws2_32 -lwsock32
endif

include sources.mk
include headers.mk
include tests.mk

export OBJS := $(patsubst %.c,%.o,$(SRCS))
export MAIN_OBJ := $(patsubst %.c,%.o,$(MAIN_SRC))
export TEST_TARGETS := $(patsubst %.c,%,$(MAIN_TESTS))
export TEST_OBJS := $(patsubst %.c,%.o,$(TESTS))

export CORE_LIB := $(CURDIR)/libsrv.a

MAIN_TARGET = mcsrv

.PHONY: all clean $(TEST_TARGETS)

all: debug

debug: CFLAGS += -O0 -g -DDEBUG 
debug: $(MAIN_TARGET)

trace: CFLAGS += -O0 -g -DDEBUG -DTRACE
trace: $(MAIN_TARGET)

release: CFLAGS += -O2
release: $(MAIN_TARGET)

test: CFLAGS += -O0 -g -DDEBUG
test: $(TEST_TARGETS)

$(CORE_LIB): $(OBJS) $(HDRS)
	$(AR) rc $@ $(OBJS)

$(MAIN_TARGET): $(MAIN_OBJ) $(CORE_LIB)
	$(CC) $(LDFLAGS)  -o $@ $^ $(LDLIBS)

$(TEST_TARGETS): $(CORE_LIB)
	$(MAKE) -C $(dir $@)

%.o: %.c $(HDRS)
	$(CC) -c $(CPPFLAGS) $(CFLAGS) -o $@ $<

clean:
	rm $(OBJS)
	rm $(MAIN_TARGET)
	rm libsrv.a
