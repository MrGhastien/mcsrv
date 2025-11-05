TARGET := test_dynvector

$(TARGET): dynvector.c $(CORE_LIB) $(PLATFORM_LIB)
	@echo -e "\e[33mT	\e[36mCCLD	$(notdir $@)\e[0m"
	@$(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) -Wl,--start-group $(LDLIBS) $^ -Wl,--end-group -o $@
