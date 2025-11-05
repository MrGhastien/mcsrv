TARGET := test_string

$(TARGET): $(CORE_LIB) $(PLATFORM_LIB) test_string.c 
	@echo -e "\e[33mT	\e[36mCCLD	$(notdir $@)\e[0m"
	@$(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) -Wl,--start-group $(LDLIBS) $^ -Wl,--end-group -o $@

