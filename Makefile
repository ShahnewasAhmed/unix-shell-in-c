
CC = gcc

CFLAGS = -std=c11 -Wall -Wextra -Werror -g
INCLUDES = -Iinclude

SRC = $(shell find src -name '*.c')
OBJ = $(SRC:.c=.o)
DEP = $(OBJ:.o=.d)

TARGET = csh
DEBUG_TARGET = csh-debug

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) $(CFLAGS) $(OBJ) -o $@

%.o: %.c
	$(CC) $(CFLAGS) -MMD -MP $(INCLUDES) -c $< -o $@

-include $(DEP)

clean:
	rm -f $(OBJ) $(DEP) $(TARGET) $(DEBUG_TARGET)

re: clean all

test:
	$(MAKE) -C tests

debug:
	$(CC) $(CFLAGS) -fsanitize=address,undefined \
		-fno-omit-frame-pointer $(SRC) $(INCLUDES) -o $(DEBUG_TARGET)

.PHONY: all clean re test debug

CFLAGS = -std=c11 -D_POSIX_C_SOURCE=200809L -Wall -Wextra -Werror -g
