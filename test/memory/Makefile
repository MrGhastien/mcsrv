TARGETS := test_basic_pool test_pool_alloc

all: $(TARGETS)

test_basic_pool: basic_pool.c $(CORE_LIB)
	@echo -e "\e[33mT	\e[36mCCLD	$(notdir $@)\e[0m"
	@$(CC) $(LDFLAGS)  $(LDLIBS) $(CFLAGS) $(CPPFLAGS) -o $@ $^ ../../libs/libunity.a

test_pool_alloc: pool_alloc.c $(CORE_LIB)
	@echo -e "\e[33mT	\e[36mCCLD	$(notdir $@)\e[0m"
	@$(CC) $(LDFLAGS) $(LDLIBS) $(CFLAGS) $(CPPFLAGS) -o $@ $^ ../../libs/libunity.a

.PHONY: all
