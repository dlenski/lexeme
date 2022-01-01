# Test with nose2 -v

from lexeme import stats_of_guess, update_stats_from_guess, StatColors, LETTERS

U = StatColors.Unknown
A = StatColors.Absent
WP = StatColors.WrongPosition
RP = StatColors.RightPosition


def test_guess_stats_easy():
    # Easy one
    assert stats_of_guess('SWEAT', 'FLEAS') == [WP, A, RP, RP, A]


def test_guess_stats_letter_repeated_after_RP():
    # Extra 'E' after the RP 'E' should be marked A (see https://twitter.com/moxfyre/status/1477320939520020484)
    assert stats_of_guess('REELS', 'REBUS') == [RP, RP, A, A, RP]


def test_guess_stats_letter_repeated_before_RP():
    # Extra 'R' before the RP 'R' should be marked A
    assert stats_of_guess('ROARS', 'BEARS') == [A, A, RP, RP, RP]


def test_guess_stats_duplicate_letter_one_RP_after_one_WP():
    # Extra 'A' before the RP 'A' should be marked WP
    assert stats_of_guess('ARIAS', 'PAPAS') == [WP, A, A, RP, RP]


def test_guess_stats_duplicate_letter_one_RP_before_one_WP():
    # Extra 'A' after the RP 'A' should be marked WP
    assert stats_of_guess('ALAMO', 'ARIAS') == [RP, A, WP, A, A]


def test_update_stats_from_guess():
    # Check that moving a letter from RP->WP, and removing an RP letter, doesn't affect stats
    stats = {l: StatColors.Unknown for l in LETTERS}
    update_stats_from_guess(stats, 'SWEAT', 'FLEAS')
    assert [stats[l] for l in 'SWEAT'] == [WP, A, RP, RP, A]
    update_stats_from_guess(stats, 'FLAKS', 'FLEAS')
    assert [stats[l] for l in 'FLK'] == [RP, RP, A]
    assert stats['S'] == RP   # removed RP letter from guess
    assert stats['A'] == stats['E'] == RP   # moved from RP to WP
