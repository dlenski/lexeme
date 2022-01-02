#!/usr/bin/python3

from enum import Enum
import argparse
import colorama
import os
import random

from .algorithms import LETTERS, StatColors, stats_of_guess, update_stats_from_guess, remove_words_using_guess

LETTERS = 'ABCDEFGHIJKLMNOPQRSTUVWXYZ'


StatColors = Enum('StatColors', {
    'Unknown': colorama.Style.RESET_ALL,
    'Absent': colorama.Back.RED,
    'WrongPosition': colorama.Back.YELLOW,
    'RightPosition': colorama.Back.GREEN,
})


def colored_letters(stats):
    result = ''
    last_stat = None
    for l in LETTERS:
        next_stat = stats[l]
        if next_stat != last_stat:
            result += next_stat.value
            last_stat = next_stat
        result += l

    return result + colorama.Style.RESET_ALL


def colored_guess(guess, target):
    return ''.join(s.value + l for s, l in zip(stats_of_guess(guess, target), guess)) + colorama.Style.RESET_ALL


def eligible_words(df, length):
    for line in df:
        word = line.strip()
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
    p.add_argument('--test', help=argparse.SUPPRESS)
    args = p.parse_args(args)
    return p, args


def main(args=None):
    p, args = parse_args(args)

    words = list(eligible_words(args.dict, args.length))
    narrow_words = words

    if args.test:
        target = args.test.strip().upper()
        if target not in words:
            p.error(f"Need a known {args.length}-letter word to test, not {target!r}")
        print(f"I've chosen the word {target} which you specified to test with!")
    else:
        target = random.choice(words)
        print(f"I've chosen a {args.length}-letter word from {len(words)} possibilities.")
    print(f"You have {args.guesses} guesses to guess it correctly.")
    if not args.nonsense:
        print(f"All your guesses must be words that I know!")
    print()

    guesses = []
    stats_so_far = {l: StatColors.Unknown for l in LETTERS}
    while len(guesses) < args.guesses:
        # Ask for next guess
        print(f"Letters: {colored_letters(stats_so_far)}")
        if args.analyzer == 1 or (args.analyzer == 2 and len(narrow_words) >= 100):
            print(f"There are {len(narrow_words)} possible words remaining.")
        elif args.analyzer >= 2:
            print(f"There are {len(narrow_words)} possible words remaining: {', '.join(narrow_words)}")

        try:
            while True:
                guess = input("Your guess? ").strip().upper()
                if len(guess) != args.length or any(c not in LETTERS for c in guess):
                    print(f"Must be a word consisting of exactly {args.length} letters. Try again.")
                elif not args.nonsense and guess not in words:
                    print(f"Hmmm, I don't know the word {guess}. Try again.")
                else:
                    break
        except (KeyboardInterrupt, EOFError):
            print()
            print("Interrupted, giving up...")
            break

        # Update stats with guess
        update_stats_from_guess(stats_so_far, guess, target)
        guesses.append(guess)

        # Show guesses so far
        print()
        for ii, g in enumerate(guesses, 1):
            print(f"Guess {ii}: {colored_guess(g, target)}")
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


if __name__ == '__main__':
    main()