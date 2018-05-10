CC=gcc
LINK=gcc

FILES=LogC LogC_Test
OBJ=$(addsuffix .o,$(FILES))

CFLAGS=-Wextra -Wall -Wformat=2
LFLAGS=

%.o: %.c
		$(CC) -c -o $@ $< $(CFLAGS)

LogC_Test: $(OBJ)
		$(LINK) -o $@.exe $^ $(LFLAGS)
		
.PHONY: clean		
		
clean:
		rm  *.o *.exe 2>nul
