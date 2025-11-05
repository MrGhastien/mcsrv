
# libunity.so: unity-2.6.1/src/unity.o
# 	@echo -e "\e[33mL	\e[35mLD	$(notdir $@)\e[0m"
# 	$(CC) $(CPPFLAGS) $(CFLAGS) -shared -fpic -o $@ $^

libunity.a: unity-2.6.1/src/unity.o
	@echo -e "\e[33mL	\e[35mAR	$(notdir $@)\e[0m"
	@$(AR) rc $@ $^

unity-2.6.1/src/unity.o: unity-2.6.1/src/unity.c
	@echo -e "\e[33mL	\e[36mCC	$(notdir $@)\e[0m"
	@$(CC) -c $(CPPFLAGS) $(CFLAGS) -fpic -o $@ $<
