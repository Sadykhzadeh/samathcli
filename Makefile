CC ?= gcc
CFLAGS ?= -std=c11 -O2 -Wall -Wextra -Wpedantic -Wshadow -Wstrict-prototypes
BIN = samath
LIB = getQuestion.o checkAnswer.o question.o
OBJ = $(LIB) samath.o
UNIT = test/unit

.PHONY: all build check clean

all: $(BIN)

build: $(BIN)

$(BIN): $(OBJ)
	$(CC) $(CFLAGS) $(OBJ) -o $@
	@echo Done! I could build it! :3

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

getQuestion.o: getQuestion.c getQuestion.h checkAnswer.h question.h
checkAnswer.o: checkAnswer.c checkAnswer.h
question.o: question.c question.h
samath.o: samath.c getQuestion.h

# Run `make clean check`: the objects are rebuilt with warnings as errors.
check: CFLAGS += -Werror
check: $(BIN) $(UNIT)
	./$(UNIT)

$(UNIT): test/unit.c checkAnswer.o question.o
	$(CC) $(CFLAGS) test/unit.c checkAnswer.o question.o -o $@

clean:
	rm -f $(BIN) $(OBJ) $(UNIT) .tmp.*
	@echo Done! :3
