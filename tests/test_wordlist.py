import os

from lexeme.__main__ import eligible_words

_dd = '/usr/share/dict/words'
_ds = '/usr/share/dict/spanish'


def test_default_dict_exists():
    assert os.access(_dd, os.R_OK)


def test_default_dict_has_words():
    with open(_dd) as df:
        for length in (4, 5, 6, 7, 8):
            assert next(eligible_words(df, length), None) is not None
            df.seek(0)


def test_spanish_dict_has_accented_words():
    with open(_ds) as df:
        for length, word in (
                (4, 'SIJU'),      # sijú
                (5, 'ABACO'),     # ábaco
                (6, 'ABANAR'),    # abañar
                (7, 'SIMBOLO'),   # símbolo
                (8, 'SIMETRIA'),  # simetría
        ):
            assert word in eligible_words(df, length, strip_diacritics=True)
            df.seek(0)
