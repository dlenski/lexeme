from lexeme.algorithms import LETTERS, ClueColors, clues_of_guess, update_clues_from_guess, remove_words_using_guess

_clues_abbrev = {
    ClueColors.Unknown: 'U',
    ClueColors.Absent: 'A',
    ClueColors.WrongPosition: 'W',
    ClueColors.RightPosition: 'R',
}


def _check_clues_array(actual, expected):
    actual = ''.join(a.name[0] for a in actual)
    assert expected == actual


def _check_clues_of_guess(guess, target, expected_clues):
    return _check_clues_array(clues_of_guess(guess, target), expected_clues)


def _check_clues_of_letters(letter_clues, which_letters, expected_clues):
    return _check_clues_array([letter_clues[l] for l in which_letters], expected_clues)


def test_guess_clues():
    # Easy one
    yield _check_clues_of_guess, 'SWEAT', 'FLEAS', 'WARRA'
    # Extra 'E' after the RP 'E' should be marked A (see https://twitter.com/moxfyre/status/1477320939520020484)
    yield _check_clues_of_guess, 'REELS', 'REBUS', 'RRAAR'
    # Extra 'R' before the RP 'R' should be marked A
    yield _check_clues_of_guess, 'ROARS', 'BEARS', 'AARRR'
    # Extra 'A' before the RP 'A' should be marked WP
    yield _check_clues_of_guess, 'ARIAS', 'PAPAS', 'WAARR'
    # Extra 'A' after the RP 'A' should be marked WP
    yield _check_clues_of_guess, 'ALAMO', 'ARIAS', 'RAWAA'
    # Longer word
    yield _check_clues_of_guess, 'REVILE', 'SEVENS', 'ARRAAW'
    yield _check_clues_of_guess, 'EVENER', 'SEVENS', 'WWWWAA'
    # This was a bug in Py->C conversion
    yield _check_clues_of_guess, 'AAHED', 'ABEAM', 'RWAWA'


def test_update_clues_from_guess():
    clues = {l: ClueColors.Unknown for l in LETTERS}

    # First we guess SWEAT and verify clues are correct
    update_clues_from_guess(clues, 'SWEAT', 'FLEAS')
    _check_clues_of_letters(clues, 'SWEAT', 'WARRA')

    # Next we guess FLAKS, which moves 'A', 'E' from RP->WP and removes 'S' (WP) altogether, and
    # verify that those letters keep their best previous status.
    update_clues_from_guess(clues, 'FLAKS', 'FLEAS')
    _check_clues_of_letters(clues, 'FLKSAE', 'RRARRR')


def test_remove_words_using_guess():
    words = {'ADDUCE', 'ADVICE', 'ADVISE', 'DEDUCE', 'DELVES', 'DEVILS', 'DEVICE', 'DEVISE', 'ELVISH', 'EVENER', 'EVOLVE',
             'LEAVES', 'LOAVES', 'REVEAL', 'REVELS', 'REVILE', 'REVISE', 'REVIVE', 'SEVENS', 'VESSEL', 'VIOLAS', 'VIOLIN'}

    # Specific cases
    _check_clues_of_guess('VXXXXX', 'ADDUCE', 'AAAAAA')
    assert {'ADDUCE', 'DEDUCE'} == set(remove_words_using_guess('VXXXXX', 'ADDUCE', words))            # 'AAAAAA'
    _check_clues_of_guess('XXXXXV', 'VIOLAS', 'AAAAAW')
    assert {'VESSEL', 'VIOLAS', 'VIOLIN'} == set(remove_words_using_guess('VXXXXX', 'VIOLAS', words))  # 'AAAAAW'
    _check_clues_of_guess('ADVICE', 'EVENER', 'AAWAAW')
    assert {'EVENER', 'VESSEL'} == set(remove_words_using_guess('ADVICE', 'EVENER', words))            # 'AAWAAW'

    # Exhaustive
    for target in words:
        # Target should always remain after guess
        for guess in words - {target}:
            assert target in remove_words_using_guess(guess, target, words)

        # Only target should remain after guessing target
        assert {target} == set(remove_words_using_guess(target, target, words))

        # A guess with no overlapping letters should eliminate nothing
        assert words == set(remove_words_using_guess('XXXXXX', target, words))
