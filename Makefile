MAKEFLAGS += --no-print-directory

#OS detection
ifeq ($(OS),Windows_NT)
	detected_os := WINDOWS
else
	detected_os := $(shell uname | tr '[:lower:]' '[:upper:]')
endif

# Basic directories
export SRC_DIR := $(CURDIR)/src
export INC_DIR := $(CURDIR)/include
export OBJ_DIR := $(CURDIR)/obj
export TEST_DIR := $(CURDIR)/test
export LIBDIR := $(CURDIR)/libs

# Common flags
export CC = gcc
export CFLAGS = -Wall -Wextra -Wreturn-type -Werror -pedantic -Wcast-align -Wpointer-to-int-cast -Wint-to-pointer-cast -Winit-self #-fsanitize=address
export CPPFLAGS = -I$(SRC_DIR) -DMC_PLATFORM_$(detected_os)
export LDFLAGS = -L"$(LIBDIR)" #-fsanitize=address
export LDLIBS := -lcrypto -lcurl

# Platform-specific flags
ifeq ($(detected_os),WINDOWS)
	CPPFLAGS += -IC:\msys64\ucrt64\include
	LDLIBS += -lws2_32 -lwsock32 -L$(CURDIR)/lib/zlib-1.3.1 -lzlib1
	MAIN_TARGET = mcsrv.exe
else
	LDLIBS += -lz
	MAIN_TARGET = mcsrv
endif

# Lists of source files
include sources.mk
include headers.mk
include tests.mk

export OBJS := $(patsubst %.c,%.o,$(SRCS))
export MAIN_OBJ := $(patsubst %.c,%.o,$(MAIN_SRC))
export TEST_TARGETS := $(patsubst %.c,%,$(MAIN_TESTS))
export TEST_OBJS := $(patsubst %.c,%.o,$(TESTS))

export CORE_LIB := $(CURDIR)/libcore.a
export PLATFORM_LIB := $(SRC_DIR)/platform/libplatform.a

.PHONY: all clean $(TEST_TARGETS) docs

all: debug

docs: $(SRCS) $(HDRS)
	doxygen $(CURDIR)/doxygen-config.conf

#debug: LDFLAGS += -rdynamic
debug: CFLAGS += -O0 -g -DDEBUG 
debug: LDLIBS += -lunwind -ldw
debug: $(MAIN_TARGET)

trace: CFLAGS += -O0 -g -DDEBUG -DTRACE
#trace: LDFLAGS += -rdynamic
trace: LDLIBS += -lunwind -ldw
trace: $(MAIN_TARGET)

release: CFLAGS += -O2
release: $(MAIN_TARGET)

test: CFLAGS += -O0 -g -DDEBUG
test: LDLIBS += -lunity
test: LDFLAGS += -rdynamic 
test: CPPFLAGS += -I"$(LIBDIR)/unity-2.6.1/src"
test: $(TEST_TARGETS)

$(CORE_LIB): $(OBJS) $(HDRS)
	@echo -e "	\e[35mAR	$(notdir $@)\e[0m"
	@$(AR) rc $@ $(OBJS)

$(PLATFORM_LIB):
	@echo -e "\e[32m>>	MAKE	$(SRC_DIR)/platform\e[0m"
	@$(MAKE) -C $(SRC_DIR)/platform
	@echo -e "\e[32m<<	. . .\e[0m"

$(MAIN_TARGET): $(MAIN_OBJ) $(CORE_LIB) $(PLATFORM_LIB) 
	@echo -e "	\e[34mLD	$(notdir $@)\e[0m"
	@$(CC) $(LDFLAGS)  -o $@ -Wl,--start-group $^ -Wl,--end-group $(LDLIBS)

$(TEST_TARGETS): $(CORE_LIB) $(LIBDIR)/libunity.a
	@echo -e "\e[32m>>	MAKE	$@\e[0m"
	@$(MAKE) -C $@
	@echo -e "\e[32m<<	. . .\e[0m"

%.o: %.c $(HDRS)
	@echo -e "	\e[36mCC	$(notdir $@)\e[0m"
	@$(CC) -c $(CPPFLAGS) $(CFLAGS) -o $@ $<

$(LIBDIR)/libunity.a:
	@echo -e "\e[32m>>	MAKE	$(LIBDIR)\e[0m"
	@$(MAKE) -C $(LIBDIR)
	@echo -e "\e[32m<<	. . .\e[0m"

clean:
	rm -f $(OBJS)
	rm -f $(MAIN_TARGET)
	rm -f $(CORE_LIB)
	@echo -e "\e[32m>>	MAKE	$(SRC_DIR)/platform\e[0m"
	@$(MAKE) -C $(SRC_DIR)/platform clean
	@echo -e "\e[32m<<	. . .\e[0m"
