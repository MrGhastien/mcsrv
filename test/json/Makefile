TARGET := test_json

$(TARGET): $(CORE_LIB) $(PLATFORM_LIB) test_json.c 
	@echo -e "\e[33mT	\e[36mCCLD	$(notdir $@)\e[0m"
	@$(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) -Wl,--start-group $(LDLIBS) $^ -Wl,--end-group -o $@
