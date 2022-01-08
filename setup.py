#!/usr/bin/env python3

import sys

try:
    from setuptools import setup
except ImportError:
    from distutils.core import setup

if sys.version_info[0] < 3:
    sys.exit("Python 3.x is required; you are using %s" % sys.version)

setup(
    name="lexeme",
    version="0.1",
    description=("Play and analyze a word-guessing game like Wordle"),
    long_description=open('README.md').read(),
    long_description_content_type='text/markdown',
    author="Daniel Lenski",
    author_email="dlenski@gmail.com",
    license='GPL v3 or later',
    url="https://github.com/dlenski/lexeme",
    install_requires=open('requirements.txt').readlines(),
    extras_require={'unidecode': ['unidecode']},
    packages=["lexeme"],
    entry_points={'console_scripts': ['lexeme=lexeme.__main__:main']},
    tests_require=open('requirements-test.txt').readlines(),
    test_suite='nose2.collector.collector',
    classifiers={
        'Environment :: Console',
        'Topic :: Games/Entertainment :: Puzzle Games',
        'Operating System :: POSIX',
        'License :: OSI Approved :: GNU General Public License v3 or later (GPLv3+)',
    },
)
