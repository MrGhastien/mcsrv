TARGET := test_json

$(TARGET): test_json.c $(CORE_LIB)
	@echo -e "\e[33mT	\e[36mCCLD	$(notdir $@)\e[0m"
	@$(CC) $(LDFLAGS) $(LDLIBS) $(CFLAGS) $(CPPFLAGS) -o $@ $^ ../../libs/libunity.a
