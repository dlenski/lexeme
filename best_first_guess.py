#!/usr/bin/python3

'''
Wordle/Lexeme best-first-guess solver
=====================================

Start with a dictionary containing N eligible words of length L.
Assume all N words are equally likely as a target.

Q: What is the optimal first guess? That is, what first guess will
   ON AVERAGE leave the fewest possible remaining words to guess?
A: We need to run O(N^2) iterations of stats_of_guess, and O(N^3)
   iterations of is_word_possible_after_guess. Each is about
   O(L) in runtime. Memory requirements are trivial.

   Python is too damn slow, can't multithread itself out of a
   CPU-bound problem due to GIL, and would still be too slow if
   it could. See the C version instead :-)

usage: best_first_guess.py [wordlist.txt] [target_word_len] > results.csv
 e.g.: best_first_guess.py /usr/share/dict/american-english 5 > results.csv
'''

from lexeme.__main__ import eligible_words
from lexeme.algorithms import is_word_possible_after_guess, stats_of_guess
import sys

if len(sys.argv) == 3:
    dictfn = sys.argv[1]
    targetlen = int(sys.argv[2])
else:
    raise SystemExit(f"usage: {sys.argv[0]} [wordlist] [wordlen]")

words = list(eligible_words(open(dictfn), targetlen))
print('guess,avg_words_left_after_first_guess\n')
for guess in words:
    print(f"Trying {guess}...", file=sys.stderr)
    sys.stdout.flush()
    acc = 0
    for target in words:
        stats = stats_of_guess(guess, target)
        acc += sum(is_word_possible_after_guess(guess, w, stats) for w in words)
    print(f'"{guess}",{acc / len(words)}')
