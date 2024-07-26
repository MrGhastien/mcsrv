SRC_DIR = ./src
INC_DIR = ./include
OBJ_DIR = ./obj
TEST_DIR = ./test

CC = gcc
CFLAGS = -Wall -Wextra -Wreturn-type -Werror #-fsanitize=address
CPPFLAGS = -I$(SRC_DIR) -DMC_PLATFORM_LINUX
#LDFLAGS = -fsanitize=address
LDLIBS := -lcrypto -lz -lcurl

include sources.mk
include headers.mk
include tests.mk

OBJS = $(patsubst %.c,%.o,$(SRCS))
MAIN_OBJ := $(patsubst %.c,%.o,$(MAIN_SRC))
TEST_TARGETS := $(patsubst %.c,%,$(MAIN_TESTS))
TEST_OBJS := $(patsubst %.c,%.o,$(TESTS))

MAIN_TARGET = mcsrv

.PHONY: all clean

debug: CFLAGS += -O0 -g -DDEBUG 
debug: $(TARGET)

trace: CFLAGS += -O0 -g -DDEBUG -DTRACE
trace: $(TARGET)

release: CFLAGS += -O2
release: $(TARGET)

$(MAIN_TARGET): $(MAIN_OBJ) $(OBJS) $(HDRS)
	$(CC) $(LDFLAGS) $(LDLIBS) -o $@ $(OBJS)

$(TEST_TARGETS): $(TEST_OBJS) $(OBJS) $(HDRS)
	$(MAKE) 

%.o: %.c $(HDRS)
	$(CC) -c $(CPPFLAGS) $(CFLAGS) -o $@ $<

clean:
	rm $(OBJS)
	rm $(TARGET)
