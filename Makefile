CC ?= gcc
CFLAGS ?= -std=c11 -O2 -Wall -Wextra -Wpedantic -Wshadow -Wstrict-prototypes
BIN = samath
LIB = getQuestion.o checkAnswer.o input.o options.o question.o report.o
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

getQuestion.o: getQuestion.c getQuestion.h options.h question.h
checkAnswer.o: checkAnswer.c checkAnswer.h
input.o: input.c input.h
options.o: options.c options.h
question.o: question.c question.h
report.o: report.c report.h
samath.o: samath.c checkAnswer.h getQuestion.h input.h options.h question.h report.h

# Run `make clean check`: the objects are rebuilt with warnings as errors.
check: CFLAGS += -Werror
check: $(BIN) $(UNIT)
	./$(UNIT)
	sh test/e2e.sh
	@if command -v python3 >/dev/null 2>&1; then python3 test/terminal.py ./$(BIN) && python3 test/realcurl.py ./$(BIN); else echo "python3 not found: the terminal and server checks were skipped"; fi

$(UNIT): test/unit.c $(LIB)
	$(CC) $(CFLAGS) test/unit.c $(LIB) -o $@

clean:
	rm -f $(BIN) $(OBJ) $(UNIT)
	@echo Done! :3
