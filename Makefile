build:
	clear &&  gcc getQuestion.c checkAnswer.c samath.c -o samath && echo Done! I could build it! :3
clean:
	clear && rm samath && echo Done! :3