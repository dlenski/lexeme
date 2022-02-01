#!/usr/bin/python3

import argparse
import os
import random
import time
from colorama import Fore, Style
try:
    from unidecode import unidecode
except:
    unidecode = None

from .algorithms import LETTERS, ClueColors, clues_of_guess, update_clues_from_guess, remove_words_using_guess


EMPH = Fore.BLUE + Style.BRIGHT
RESET = Style.RESET_ALL


def colored_letters(clues):
    result = ''
    last_stat = None
    for l in LETTERS:
        next_stat = clues[l]
        if next_stat != last_stat:
            result += next_stat.value
            last_stat = next_stat
        result += l

    return result + RESET


def colored_guess(guess, target):
    return ''.join(s.value + l for s, l in zip(clues_of_guess(guess, target), guess)) + RESET


def eligible_words(df, length, strip_diacritics=False):
    if strip_diacritics and not unidecode:
        raise NotImplementedError("unidecode module required for strip_diacritics")
    for line in df:
        word = line.strip()

        # Need to do this before checking length, because unidecode can change it,
        # as in unidecode('buß') -> 'buss'. I wish unidecode('König') -> 'koenig',
        # but it doesn't currently do that.
        if strip_diacritics:
            word = unidecode(word)

        if len(word) == length:
            # No non-letter characters, or mixed case (latter are likely proper nouns)
            if all(c.upper() in LETTERS for c in word) and word in (word.upper(), word.lower()):
                yield word.upper()


def parse_args(args=None):
    p = argparse.ArgumentParser()
    p.add_argument('-d', '--dict', default='/usr/share/dict/words', type=lambda fn: argparse.FileType()(os.path.join('/usr/share/dict', fn)),
                   help='Wordlist to use, either an absolute path or a path relative to /usr/share/dict. Default %(default)s.')
    p.add_argument('-g', '--guesses', default=6, type=int,
                   help='Maximum number of guesses to allow')
    p.add_argument('-l', '--length', default=5, type=int,
                   help='Length of word to guess')
    p.add_argument('-n', '--nonsense', action='store_true',
                   help='Allow nonsense guesses. (Default is to only allow known words.)')
    p.add_argument('-a', '--analyzer', action='count', default=0,
                   help='Analyze remaining possible words, and show their number after each guess. If repeated (cheater mode!), it will show you all the remaining possible words when there are fewer than 100')
    p.add_argument('-t', '--timer', action='store_true',
                   help='Show time taken after every guess.')
    p.add_argument('--test', help=argparse.SUPPRESS)
    if unidecode:
        p.add_argument('-D', '--strip-diacritics', action='store_true',
                       help='EXPERIMENTAL: Strip diacritics from words (should allow playing with Spanish/French wordlists)')
    args = p.parse_args(args)
    if not unidecode:
        args.strip_diacritics = False
    return p, args


def main(args=None):
    p, args = parse_args(args)

    words = list(eligible_words(args.dict, args.length, args.strip_diacritics))
    narrow_words = words

    if args.test:
        target = args.test.strip().upper()
        if target not in words:
            p.error(f"Need a known {args.length}-letter word to test, not {target!r}")
        print(f"I've chosen the word {target} which you specified to test with!")
    else:
        target = random.choice(words)
        print(f"I've chosen a {EMPH}{args.length}{RESET}-letter word from {EMPH}{len(words)}{RESET} possibilities.")
    print(f"You have {EMPH}{args.guesses}{RESET} guesses to guess it correctly.")
    if not args.nonsense:
        print(f"All your guesses must be words that I know!")
    print()

    guesses = []
    letter_clues = {l: ClueColors.Unknown for l in LETTERS}
    start_at = last_at = time.time()
    while len(guesses) < args.guesses:
        # Ask for next guess
        print(f"Letters: {colored_letters(letter_clues)}")
        if args.analyzer == 1 or (args.analyzer == 2 and len(narrow_words) >= 100):
            print(f"There are {EMPH}{len(narrow_words)}{RESET} possible words remaining.")
        elif args.analyzer >= 2:
            print(f"There are {EMPH}{len(narrow_words)}{RESET} possible words remaining: {', '.join(narrow_words)}")

        try:
            while True:
                guess = input("Your guess? ").strip().upper()
                if len(guess) != args.length or any(c not in LETTERS for c in guess):
                    print(f"Must be a word consisting of exactly {EMPH}{args.length}{RESET} letters. Try again.")
                elif not args.nonsense and guess not in words:
                    print(f"Hmmm, I don't know the word {EMPH}{guess}{RESET}. Try again.")
                else:
                    break  # Okay
        except (KeyboardInterrupt, EOFError):
            print()
            print("Interrupted, giving up...")
            break

        # Update clues with guess
        update_clues_from_guess(letter_clues, guess, target)
        guesses.append(guess)

        now = time.time()
        total = now - start_at
        last = now - last_at
        last_at = now

        # Show guesses so far
        print()
        for ii, g in enumerate(guesses, 1):
            print(f"Guess {ii}: {colored_guess(g, target)}")
        if args.timer:
            print(f"Last guess took {EMPH}{last:.2f}{RESET} seconds, {EMPH}{len(guesses)}{RESET} guess{'es' if len(guesses)!=1 else ''}"
                  f" in {EMPH}{total:.2f}{RESET} s ({EMPH}{total/len(guesses):.2f} s/guess{RESET}).")
        print()

        if guess == target:
            break

        # Narrow possible words from guess
        if args.analyzer:
            narrow_words = list(remove_words_using_guess(guess, target, narrow_words))

    if guesses and guesses[-1] == target:
        print(f"Correct! {colored_guess(target, target)}")
    else:
        print(f"Sorry, the correct word was: {colored_guess(target, target)}")
    print()


if __name__ == '__main__':
    main()
