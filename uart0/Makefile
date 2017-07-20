SRC = $(wildcard *.c)
OBJ = $(SRC:.c=.o)
DEP = $(SRC:.c=.d)

CFLAGS = -Wall -Wextra
LDFLAGS =

uart0: $(OBJ)
	@echo LD $@
	@gcc $(LDFLAGS) -o $@ $^

%.o: %.c
	@echo CC $<
	@$(CC) $(CFLAGS) -MD -MF $(@:.o=.d) -I. -c -o $@ $<

-include $(DEP)

clean:
	@rm -f uart0 $(OBJ) $(DEP)
