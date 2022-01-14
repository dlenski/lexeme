# For macOS: `brew install gcc@11` and then
# set CC = gcc-11

CC = gcc
CFLAGS = -std=c11 -O3 -flto=auto -fwhole-program #-Wall
FULL_WORDLIST = /usr/share/dict/words
#FULL_WORDLIST = wordle_answerlist.txt

best_guess: best_guess.c
	${CC} $(CFLAGS) best_guess.c -o best_guess

clean:
	-@rm best_guess pgo-is-generated 50_5char_wordlist 500_5char_wordlist 500_6char_wordlist 500_7char_wordlist 1000_7char_wordlist 500_8char_wordlist 2>/dev/null || true

pgo: pgo-is-generated

pgo-is-generated: 50_5char_wordlist
	${CC} -fprofile-generate $(CFLAGS) best_guess.c  -o best_guess
	./best_guess 50_5char_wordlist 5 > /dev/null 2>&1
	./best_guess 50_5char_wordlist 5 550_5char_guesslist > /dev/null 2>&1
	${CC} -fprofile-use $(CFLAGS) best_guess.c -o best_guess
	touch pgo-is-generated

time: 500_5char_wordlist pgo-is-generated
	time ./best_guess 500_5char_wordlist 5 > /dev/null 2>&1

50_5char_wordlist:
	grep -E '^[[:alpha:]]{5}$$' /usr/share/dict/words | shuf -n50 > 50_5char_wordlist

500_5char_wordlist:
	grep -E '^[[:alpha:]]{5}$$' /usr/share/dict/words | shuf -n500 > 500_5char_wordlist

# almost 550
550_5char_guesslist: 50_5char_wordlist 500_5char_wordlist
	cat 50_5char_wordlist 500_5char_wordlist | sort | uniq > 550_5char_guesslist

500_6char_wordlist:
	grep -E '^[[:alpha:]]{6}$$' /usr/share/dict/words | shuf -n500 > 500_6char_wordlist

500_7char_wordlist:
	grep -E '^[[:alpha:]]{7}$$' /usr/share/dict/words | shuf -n500 > 500_7char_wordlist

1000_7char_wordlist:
	grep -E '^[[:alpha:]]{7}$$' /usr/share/dict/words | shuf -n1000 > 1000_7char_wordlist

500_8char_wordlist:
	grep -E '^[[:alpha:]]{8}$$' /usr/share/dict/words | shuf -n500 > 500_8char_wordlist
