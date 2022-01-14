# For macOS: `brew install gcc@11` and then
# set CC = gcc-11

CC = gcc
CFLAGS = -std=c11 -O3

best_guess: best_guess.c
	${CC} $(CFLAGS) best_guess.c -o best_guess

clean:
	rm best_guess
