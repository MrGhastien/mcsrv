TARGETS := test_basic_pool test_pool_alloc test_arena

all: $(TARGETS)

test_arena: arena.c $(CORE_LIB) $(PLATFORM_LIB)
	@echo -e "\e[33mT	\e[36mCCLD	$(notdir $@)\e[0m"
	@$(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) -Wl,--start-group $(LDLIBS) $^ -Wl,--end-group -o $@

test_basic_pool: basic_pool.c $(CORE_LIB) $(PLATFORM_LIB)
	@echo -e "\e[33mT	\e[36mCCLD	$(notdir $@)\e[0m"
	@$(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) -Wl,--start-group $(LDLIBS) $^ -Wl,--end-group -o $@

test_pool_alloc: pool_alloc.c $(CORE_LIB) $(PLATFORM_LIB)
	@echo -e "\e[33mT	\e[36mCCLD	$(notdir $@)\e[0m"
	@$(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) -Wl,--start-group $(LDLIBS) $^ -Wl,--end-group -o $@

.PHONY: all
