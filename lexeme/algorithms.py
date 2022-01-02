from enum import Enum
from colorama import Back, Style


LETTERS = 'ABCDEFGHIJKLMNOPQRSTUVWXYZ'


StatColors = Enum('StatColors', {
    'Unknown': Style.RESET_ALL,
    'Absent': Back.RED,
    'WrongPosition': Back.YELLOW,
    'RightPosition': Back.GREEN,
})


def stats_of_guess(guess, target):
    stats = []
    leftovers = []

    # Two-pass so that we don't overcount WrongPos.
    # https://twitter.com/moxfyre/status/1477321560927129604
    for gl, tl in zip(guess, target):
        if gl == tl:
            stats.append(StatColors.RightPosition)
        else:
            stats.append(StatColors.Absent)
            leftovers.append(tl)
    for ii, (gl, tl) in enumerate(zip(guess, target)):
        if gl == tl:
            pass  # Don't change
        elif gl in leftovers:
            leftovers.remove(gl)
            stats[ii] = StatColors.WrongPosition

    return stats


def update_stats_from_guess(stats, guess, target):
    for gl, tl in zip(guess, target):
        if gl == tl:
            stats[gl] = StatColors.RightPosition
        elif gl in target:
            if stats[gl] in (StatColors.Unknown, StatColors.Absent):
                stats[gl] = StatColors.WrongPosition
        else:
            stats[gl] = StatColors.Absent


def remove_words_using_guess(guess, target, words):
    stats = stats_of_guess(guess, target)
    # print(stats)
    for word in words:
        wp_guess = {}
        a_guess = {}
        left_word = {}
        for ii, (gl, wl, s) in enumerate(zip(guess, word, stats)):
            # print(gl, wl, s)
            if (gl == wl) and (s != StatColors.RightPosition):
                break  # Guess and word share a letter which is NOT marked as RP in the guess
            elif (gl != wl) and (s == StatColors.RightPosition):
                break  # Guess and word differ in a letter which IS marked as RP in the guess
            else:
                # Count number of leftover (non-RP) letters in the word
                if s != StatColors.RightPosition:
                    left_word[wl] = left_word.get(wl, 0) + 1

                # Count number of A/WP letters in the guess
                if s == StatColors.WrongPosition:
                    wp_guess[gl] = wp_guess.get(gl, 0) + 1
                elif s == StatColors.Absent:
                    a_guess[gl] = a_guess.get(gl, 0) + 1
        else:
            # Make sure there are enough of the WP letters from the guess in the word
            for l, n in wp_guess.items():
                if left_word.get(l, 0) < n:
                    break  # Guess has more of these letters as WP than the word does
            else:
                # Make sure there aren't any of the A letters from the guess in the word, after discounting
                # the WP letters from the guess.
                for l, n in a_guess.items():
                    if left_word.get(l, 0) > wp_guess.get(l, 0):
                        break  # Guess has this letter as A, and word still has some left
                else:
                    yield word  # It's (still) a possible match
