import os

from lexeme.__main__ import eligible_words

_dd = '/usr/share/dict/words'


def test_default_dict_exists():
    assert os.access(_dd, os.R_OK)


def test_default_dict_has_words():
    with open(_dd) as df:
        for length in (4, 5, 6, 7, 8):
            assert next(eligible_words(df, length), None) is not None
        df.seek(0)
