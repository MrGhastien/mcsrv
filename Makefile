SRC_DIR = ./src
INC_DIR = ./include
OBJ_DIR = ./obj

CC = gcc
CFLAGS = -Wall -Wextra -Wreturn-type -Werror #-fsanitize=address
CPPFLAGS = -I$(SRC_DIR) -DMC_PLATFORM_LINUX
#LDFLAGS = -fsanitize=address
LDLIBS := -lcrypto -lz


include sources.mk
include headers.mk

OBJS = $(patsubst %.c,%.o,$(SRCS))

TARGET = mcsrv

.PHONY: all clean

debug: CFLAGS += -O0 -g -DDEBUG 
debug: $(TARGET)

trace: CFLAGS += -O0 -g -DDEBUG -DTRACE
trace: $(TARGET)

release: CFLAGS += -O2
release: $(TARGET)

$(TARGET): $(OBJS) $(HDRS)
	$(CC) $(LDFLAGS) $(LDLIBS) -o $@ $(OBJS)

%.o: %.c $(HDRS)
	$(CC) -c $(CPPFLAGS) $(CFLAGS) -o $@ $<

clean:
	rm $(OBJS)
	rm $(TARGET)
