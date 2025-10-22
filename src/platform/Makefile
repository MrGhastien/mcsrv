ifeq ($(detected_os),WINDOWS)
	PLATFORM_DIR := windows
else
	PLATFORM_DIR := linux
endif

PLATFORM_SRCS := $(wildcard $(SRC_DIR)/platform/$(PLATFORM_DIR)/*.c)
PLATFORM_HDRS := $(wildcard $(SRC_DIR)/platform/$(PLATFORM_DIR)/*.h)

PLATFORM_OBJS := $(patsubst %.c,%.o,$(PLATFORM_SRCS))
PLATFORM_LIB := libplatform.a

$(PLATFORM_LIB): $(PLATFORM_OBJS)
	@echo -e "	\e[35mAR	$(notdir $@)\e[0m"
	@$(AR) rc $@ $(PLATFORM_OBJS)

%.o: %.c $(PLATFORM_HDRS) $(HDRS)
	@echo -e "	\e[36mCC	$(notdir $@)\e[0m"
	@$(CC) -c $(CPPFLAGS) $(CFLAGS) -o $@ $<

clean:
	rm -f $(PLATFORM_OBJS)
	rm -f $(PLATFORM_LIB)
