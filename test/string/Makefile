TARGET := test_string

$(TARGET): test_string.c $(CORE_LIB)
	@echo -e "\e[33mT	\e[36mCCLD	$(notdir $@)\e[0m"
	@$(CC) $(LDFLAGS)  $(LDLIBS) $(CFLAGS) $(CPPFLAGS) -o $@ $^
