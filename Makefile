# For macOS: `brew install gcc@11` and then
# set CC = gcc-11

CC = gcc
CFLAGS = -std=c11 -O3 -flto=auto -fwhole-program -Wall
FULL_WORDLIST = /usr/share/dict/words
#FULL_WORDLIST = wordle_answerlist.txt

best_guess: best_guess.c
	${CC} $(CFLAGS) best_guess.c -o best_guess

clean:
	-@rm best_guess 2>/dev/null || true
