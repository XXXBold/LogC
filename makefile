CC=gcc
LINK=gcc

FILES=LogC LogC_Test
OBJ=$(addsuffix .o,$(FILES))

CFLAGS=-ansi -Wextra -Wall
LFLAGS=

%.o: %.c
		$(CC) -c -o $@ $< $(CFLAGS)

LogC_Test: $(OBJ)
		$(LINK) -o $@.exe $^ $(LFLAGS)
		
.PHONY: clean		
		
clean:
		rm  *.o *.exe 2>nul
