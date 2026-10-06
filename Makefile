CC ?= gcc
CFLAGS ?= -std=c11 -O2 -Wall -Wextra -Wpedantic
BIN = samath
OBJ = getQuestion.o checkAnswer.o samath.o

.PHONY: all build clean

all: $(BIN)

build: $(BIN)

$(BIN): $(OBJ)
	$(CC) $(CFLAGS) $(OBJ) -o $@
	@echo Done! I could build it! :3

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

getQuestion.o: getQuestion.c getQuestion.h checkAnswer.h
checkAnswer.o: checkAnswer.c checkAnswer.h
samath.o: samath.c getQuestion.h

clean:
	rm -f $(BIN) $(OBJ) .tmp.*
	@echo Done! :3
