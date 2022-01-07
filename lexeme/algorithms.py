from enum import Enum
from colorama import Back, Style


LETTERS = 'ABCDEFGHIJKLMNOPQRSTUVWXYZ'


ClueColors = Enum('ClueColors', {
    'Unknown': Style.RESET_ALL,
    'Absent': Back.RED,
    'WrongPosition': Back.YELLOW,
    'RightPosition': Back.GREEN,
})


def clues_of_guess(guess, target):
    clues = []
    leftovers = []

    # Two-pass so that we don't overcount WrongPos.
    # https://twitter.com/moxfyre/status/1477321560927129604
    for gl, tl in zip(guess, target):
        if gl == tl:
            clues.append(ClueColors.RightPosition)
        else:
            clues.append(ClueColors.Absent)
            leftovers.append(tl)
    for ii, (gl, tl) in enumerate(zip(guess, target)):
        if gl == tl:
            pass  # Don't change
        elif gl in leftovers:
            leftovers.remove(gl)
            clues[ii] = ClueColors.WrongPosition

    return clues


def update_clues_from_guess(clues, guess, target):
    for gl, tl in zip(guess, target):
        if gl == tl:
            clues[gl] = ClueColors.RightPosition
        elif gl in target:
            if clues[gl] in (ClueColors.Unknown, ClueColors.Absent):
                clues[gl] = ClueColors.WrongPosition
        else:
            clues[gl] = ClueColors.Absent


def is_word_possible_after_guess(guess, word, clues):
    wp_guess = {}
    a_guess = {}
    left_word = {}

    for ii, (gl, wl, s) in enumerate(zip(guess, word, clues)):
        # print(gl, wl, s)
        if (gl == wl) and (s != ClueColors.RightPosition):
            return False  # Guess and word share a letter which is NOT marked as RP in the guess
        elif (gl != wl) and (s == ClueColors.RightPosition):
            return False  # Guess and word differ in a letter which IS marked as RP in the guess
        else:
            # Count number of leftover (non-RP) letters in the word
            if s != ClueColors.RightPosition:
                left_word[wl] = left_word.get(wl, 0) + 1

            # Count number of A/WP letters in the guess
            if s == ClueColors.WrongPosition:
                wp_guess[gl] = wp_guess.get(gl, 0) + 1
            elif s == ClueColors.Absent:
                a_guess[gl] = a_guess.get(gl, 0) + 1

    # Make sure there are enough of the WP letters from the guess in the word
    for l, n in wp_guess.items():
        if left_word.get(l, 0) < n:
            return False  # Guess has more of these letters as WP than the word does

    # Make sure there aren't any of the A letters from the guess in the word, after discounting
    # the WP letters from the guess.
    for l, n in a_guess.items():
        if left_word.get(l, 0) > wp_guess.get(l, 0):
            return False  # Guess has this letter as A, and word still has some left

    return True  # It's (still) a possible match


def remove_words_using_guess(guess, target, words):
    clues = clues_of_guess(guess, target)
    return (word for word in words if is_word_possible_after_guess(guess, word, clues))
