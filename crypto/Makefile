SRC = bigint.c
OBJ = $(SRC:.c=.o)
DEP = $(SRC:.c=.d)

CFLAGS = -Wall -Wextra -DSTANDALONE
LDFLAGS = 

verify: tests
	@./tests > tests.txt
	@./verify.py > verify.txt
	@cmp tests.txt verify.txt

tests: $(OBJ) tests.o
	@echo LD $@
	@$(CC) $(LDFLAGS) -o $@ $^

tests.c: write-tests.py
	@./write-tests.py > tests.c

%.o: %.c
	@echo CC $<
	@$(CC) $(CFLAGS) -MD -MF $(@:.o=.d) -c -o $@ $<

-include $(DEP)

clean:
	@rm -f tests.txt verify.txt tests tests.c $(OBJ) $(DEP)
