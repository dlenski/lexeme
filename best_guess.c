/*
 *  Copyright © 2022 Daniel Lenski <dlenski@gmail.com>
 *  Licensed under GPLv3.
 *
 *
 *  Wordle/Lexeme best-next-guess solver
 *  =====================================
 *
 *  Start with a dictionary containing N legal guess words of length
 *  L. Assume some subset of those words (M<=N) are equally likely as
 *  a target; M=N for the first guess of a game with no other
 *  additional information, but M<N if some words have already been
 *  ruled out by prior guesses.
 *
 *  Q: What is the optimal next guess? That is, what next guess will
 *     leave the fewest possible remaining words to guess in the worst
 *     case? (Average case... median case... Xth percentile?)
 *
 *  A: There are (3^L)-L distinct clue patterns, because each position
 *     in the word gets 3 possible clues, and all combinations are in
 *     theory possible EXCEPT that it's not possible to have (L-1)
 *     RightPosition clues and 1 WrongPosition clue. I call those
 *     distinct clue categories "cluniques". We can easily number
 *     these 0..(3^L-1), ignoring the L impossible cases for
 *     simplicity.
 *
 *     If the clues resulting from a specific guess against a specific
 *     target fall in clue category C, then the remaining possible
 *     words after that guess are in fact ALL OF THE TARGETS WHICH
 *     RESULT IN THE SAME CLUE CATEGORY.
 *
 *     So, to solve this, we iterate over the N possible guesses. For
 *     each guess, we run M iterations of clues_of_guess, one for each
 *     target. Each of those yields a clue category C. We simply count
 *     the number of target words that fall into each clue category
 *     into an array. Then, find whichever clue category had the
 *     largest count, and that count gives the worst-case number of
 *     targets remaining after this guess.
 *
 *     Do that for all N guesses, and record the results, and you've
 *     found the worst guess, with N*M iterations of clues_of_guess.
 *
 *     Calculating the average, or the median, or the full
 *     distribution of the number of remaining possible targets after
 *     a guess, turns out to be not much harder at all. See code
 *     below.
 *
 *  Original (very suboptimal) answer...
 *     We need to run (N*M) iterations of clues_of_guess, and (N*M^2)
 *     iterations of is_word_possible_after_guess. Each of those is
 *     about O(L) in runtime. Basically, we sum up the number of
 *     possible remaining targets after each (guess, target) combo,
 *     and then sum for the worst/maximum and average number over
 *     each guess.
 *
 *     Python is too damn slow, can't multithread itself out of a
 *     CPU-bound problem due to GIL, and would still be too slow if
 *     it could. Hence a C version :-)
 *
 *  usage: best_first_guess [wordlist.txt] [target_word_len] > results.csv
 *   e.g.: best_first_guess /usr/share/dict/american-english 5 > results.csv
 *
 *     The new O(N*M) version takes about 0.9 seconds to run on this
 *     dictionary (4,595 5-letter words) on one i7-8665U core at 1.9
 *     GHz.
 *
 *     The original O(N*M^2) version took about half an hour to run
 *     this, even after some grinding and complex optimizations and
 *     tuning in terms of how to store the words and the guesses. It
 *     was down to about 30 CPU cycles per evaluation of
 *     is_word_possible_after_guess, and clearly hitting a brick wall.
 *
 *  FIXME: This is embarrassingly parallelizable. With M CPU cores,
 *  just kick off M threads, have each run one possible guess, and
 *  gather the results. (If we want it to run even faster than it
 *  already is!)
 *
 */

#include <stdio.h>
#include <inttypes.h>
#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <time.h>

#define Unknown 'U'
#define Absent 'A'
#define WrongPosition 'W'
#define RightPosition 'R'


const int N_LETTERS = 26;

/**************************************************************************
 * High noise debugging options
 **************************************************************************/

#define DEBUG_STDERR(...) fprintf (stderr, __VA_ARGS__)
#define DEBUG_NOTHING(...) do{}while(0)

// Change any of these to DEBUG_STDERR(__VA_ARGS__) to enable, 0 to disable:
#define DEBUG_CLUES(...)
#define DEBUG_IS_WORD_POSSIBLE(...)
#define DEBUG_ELIGIBLE_WORDS(...)

/***************************************************************************/

// C version of lexeme.algorithms.clues_of_guess
char *clues_of_guess(int len, const char *guess, const char *target, char *clues, int *clunique) {
    char leftovers[len];

    for (int ii=0; ii < len; ii++) {
        char gl = guess[ii], tl = target[ii];
        leftovers[ii] = (gl == tl ? '\0' : tl);
    }

    int cl = 0;
    for (int ii=0; ii < len; ii++) {
        char gl = guess[ii], tl = target[ii], *lo;
        int c, trit;
        if (gl == tl) {
            DEBUG_CLUES("%s %s %c -> RP\n", guess, target, gl);
            trit = 2;
            c = RightPosition;
        } else if ((lo = memchr(leftovers, gl, len)) != NULL) {
            DEBUG_CLUES("%s %s %c -> WP\n", guess, target, gl);
            c = WrongPosition;
            trit = 1;
            *lo = '\0'; // remove it
        } else {
            DEBUG_CLUES("%s %s %c -> A\n", guess, target, gl);
            c = Absent;
            trit = 0;
        }
        cl *= 3;
        cl += trit;
        if (clues)
            clues[ii] = c;
    }

    if (clunique)
        *clunique = cl;
    if (clues)
        clues[len] = '\0';
    return clues ?: NULL;
}

void test_guess_clues() {
    char clues[7];

    assert(!strcmp(clues_of_guess(5, "SWEAT",  "FLEAS",  clues, NULL), "WARRA"));
    assert(!strcmp(clues_of_guess(5, "REELS",  "REBUS",  clues, NULL), "RRAAR"));
    assert(!strcmp(clues_of_guess(5, "ROARS",  "BEARS",  clues, NULL), "AARRR"));
    assert(!strcmp(clues_of_guess(5, "ARIAS",  "PAPAS",  clues, NULL), "WAARR"));
    assert(!strcmp(clues_of_guess(5, "ALAMO",  "ARIAS",  clues, NULL), "RAWAA"));
    assert(!strcmp(clues_of_guess(6, "REVILE", "SEVENS", clues, NULL), "ARRAAW"));
    assert(!strcmp(clues_of_guess(6, "EVENER", "SEVENS", clues, NULL), "WWWWAA"));
    assert(!strcmp(clues_of_guess(5, "AAHED",  "ABEAM",  clues, NULL), "RWAWA")); // <-- Was a Py->C bug!
}

// C version of lexeme.algorithms.is_word_possible_after_guess
inline static int is_word_possible_after_guess(int len, const char *guess, const char *word, const char *clues) {
    int wp_guess[N_LETTERS];
    int a_guess[N_LETTERS];
    int left_word[N_LETTERS];

    bzero(wp_guess, N_LETTERS*sizeof(int));
    bzero(a_guess, N_LETTERS*sizeof(int));
    bzero(left_word, N_LETTERS*sizeof(int));

    for (int ii=0; ii < len; ii++) {
        char gl = guess[ii], wl = word[ii], gl_off = gl-'A', wl_off = wl-'A', s = clues[ii];
        if ((gl == wl) && s != RightPosition) {
            DEBUG_IS_WORD_POSSIBLE("1) %d %c==%c but %c!=R\n", ii, gl, wl, s);
            return 0;   // Guess and word share a letter which is NOT marked as RP in the guess
        } else if ((gl != wl) && s == RightPosition) {
            DEBUG_IS_WORD_POSSIBLE("2) %d %c!=%c but %c==R\n", ii, gl, wl, s);
            return 0;   // Guess and word differ in a letter which IS marked as RP in the guess
        } else {
            // Count number of leftover (non-RP) letters in the word
            if (s != RightPosition)
                left_word[wl_off]++;

            // Count number of A/WP letters in the guess
            if (s == WrongPosition)
                wp_guess[gl_off]++;
            else if (s == Absent)
                a_guess[gl_off]++;
        }
    }

    // 1. Make sure there are enough of the WP letters from the guess in the word
    // 2. Make sure there aren't any of the A letters from the guess in the word, after discounting
    //    the WP letters from the guess.
    for (int ii=0; ii < N_LETTERS; ii++) {
        if (left_word[ii] < wp_guess[ii]) {
            DEBUG_IS_WORD_POSSIBLE("3) %d '%c' counts %d<%d\n", ii, ii+'AA', left_word[ii], wp_guess[ii]);
            return 0; // Guess has more of these letters as WP than the word has leftover
        }
        if (a_guess[ii] && (left_word[ii] > wp_guess[ii])) {
            DEBUG_IS_WORD_POSSIBLE("4) %d '%c' counts (%d!=0 && %d>%d)\n", ii, ii+'AA', a_guess[ii], left_word[ii], wp_guess[ii]);
            return 0; // Guess has this letter as A, and word still has some left
        }
    }

    // It's (still) a possible match
    return 1;
}

void test_word_possible_after_guess() {
    char clues[7];

    assert(!strcmp(clues_of_guess       (6, "VXXXXX", "ADDUCE", clues, 0), "AAAAAA"));
    assert( is_word_possible_after_guess(6, "VXXXXX", "ADDUCE", clues));
    assert( is_word_possible_after_guess(6, "VXXXXX", "DEDUCE", clues));
    assert(!is_word_possible_after_guess(6, "VXXXXX", "ADVICE", clues));

    assert(!strcmp(clues_of_guess       (6, "XXXXXV", "VIOLAS", clues, 0), "AAAAAW"));
    assert( is_word_possible_after_guess(6, "XXXXXV", "VESSEL", clues));
    assert( is_word_possible_after_guess(6, "XXXXXV", "VIOLAS", clues));
    assert( is_word_possible_after_guess(6, "XXXXXV", "VIOLIN", clues));
    assert(!is_word_possible_after_guess(6, "XXXXXV", "ADDUCE", clues));

    assert(!strcmp(clues_of_guess       (6, "ADVICE", "EVENER", clues, 0), "AAWAAW"));
    assert( is_word_possible_after_guess(6, "ADVICE", "EVENER", clues));
    assert( is_word_possible_after_guess(6, "ADVICE", "VESSEL", clues));
    assert(!is_word_possible_after_guess(6, "ADVICE", "DEVILS", clues));

    assert(!strcmp(clues_of_guess       (5, "AAHED",  "ABEAM",  clues, 0), "RWAWA")); // <-- Was a Py->C bug
    assert( is_word_possible_after_guess(5, "AAHED",  "ABASE",  clues));

    assert(!strcmp(clues_of_guess       (5, "AAEHD",  "AAHED",  clues, 0), "RRWWR"));
    assert( is_word_possible_after_guess(5, "AAEHD",  "AAHED",  clues));

    assert(!strcmp(clues_of_guess       (5, "NORAD",  "BERET",  clues, 0), "AARAA")); // <-- Was a chopper bug
    assert(!is_word_possible_after_guess(5, "NORAD",  "ACRES",  clues));
}

// C version of lexeme.__main__.eligible_words
//
// Returns: number of eligible words (N)
//
// (*output) points to a (char *wordlist[N])
// Each word in that list is malloc()'ed, and so is the list
int eligible_words(FILE *f, int len, char ***output) {
    const int MAX_LINE_LENGTH=128;
    char line[MAX_LINE_LENGTH]; line[0] = 0;

    int bufsize = 256, n = 0;
    char **buf = calloc(bufsize, sizeof(**output));
    assert(buf != NULL);

    for (;;) {
        DEBUG_ELIGIBLE_WORDS("*** lastline: %s\n", line);
        if (!fgets(line, MAX_LINE_LENGTH, f))
            break;

        int ll = strlen(line);
        assert(ll < MAX_LINE_LENGTH - 1);

        // Trim leading space, then trailing space
        char *start = line, *end = line + ll;
        while (*start && isspace(*start))
            start++;
        while (end[-1] && isspace(end[-1]))
            end--;
        *end = '\0';

        // Does length match?
        if (end - start != len)
            continue;

        // Make uppercase, count uppercase/lowercase
        int gotlower = 0, gotupper = 0;
        for (char *p = start; *p; *p++) {
            if (*p >= 'a' && *p <= 'z') {
                gotlower++;
                *p = toupper(*p);
            } else if (*p >= 'A' && *p <= 'Z')
                gotupper++;
            else
                goto not_a_word;  // Non-letter character
        }
        if (gotupper && gotlower)
            goto not_a_word;  // Mixed case, proper noun?

        // It's a word!
        if (n >= bufsize) {
            DEBUG_ELIGIBLE_WORDS("Realloc buf %d -> %d\n", bufsize, bufsize<<1);
            bufsize <<= 1;
            buf = realloc(buf, bufsize * sizeof(**output));
            assert(buf != NULL);
            assert(buf[0] != NULL); assert(buf[n-1] != NULL); // I don't understand realloc
        }
        DEBUG_ELIGIBLE_WORDS("buf[%d] = \"%s\"\n", n, start);
        buf[n++] = strdup(start);

    not_a_word:
        continue;
    }

    *output = buf;
    return n;
}

// https://stackoverflow.com/a/101613
int ipow(int base, int exp)
{
    int result = 1;
    for (;;) {
        if (exp & 1)
            result *= base;
        exp >>= 1;
        if (!exp)  break;
        base *= base;
    }
    return result;
}

int comp_int_rev(const void *_a, const void *_b)
{
    int a = *((int*)_a);
    int b = *((int*)_b);
    return b - a;
}

int main(int argc, char **argv) {
    if (argc != 3 && argc != 4) {
        fprintf(stderr,
                "usage: %1$s [wordlist] [wordlen]\n"
                "       %1$s [targetlist] [wordlen] [guesslist]\n"
                "\n"
                "Use the first form when the list of possible target words and allowed guesses\n"
                "are one and the same.\n"
                "\n"
                "Use the second form when there are legal guesses that are not possible targets,\n"
                "as when playing Wordle (which has this asymmetry even on the first guess), or\n"
                "when some words have already been eliminated from the possible targets (by\n"
                "prior guesses). The guesslist file should be a SUPERSET of the answerlist file.\n"
                "\n"
                "The wordlist files should have one word per line, with leading and trailing\n"
                "space ignored. Words must either be all-uppercase [A-Z] or all-lowercase [a-z],\n"
                "but not mixed-case, otherwise they will be ignored.\n",
                argv[0]);
        exit(1);
    }
    int extra_guesses = (argc == 4);
    char *targetfn = argv[1];
    int targetlen = atoi(argv[2]);
    char *guessfn = extra_guesses ? argv[3] : NULL;

    // Check sanity of algorithms
    test_guess_clues();
    test_word_possible_after_guess();

    // Load all the eligible words
    char **targets = NULL, **guesses = NULL;
    FILE *f = fopen(targetfn, "r");
    assert(f);
    int ntw = eligible_words(f, targetlen, &targets), ngw;
    assert(ntw);
    if (!extra_guesses) {
        guesses = targets;
        ngw = ntw;
        fprintf(stderr, "Loaded list of %d target/guess words of length %d from \"%s\"\n", ntw, targetlen, targetfn);
    } else {
        f = fopen(guessfn, "r");
        assert(f);
        ngw = eligible_words(f, targetlen, &guesses);
        assert(ngw);
        fprintf(stderr, "Loaded list of %d target words of length %d from \"%s\"\n", ntw, targetlen, targetfn);
        fprintf(stderr, "Loaded list of %d guess words of length %d from \"%s\"\n", ngw, targetlen, guessfn);
    }

    //printf("guess,target,words_left_after_first_guess\n");
    printf("guess,avg_targets_left_after_guess,median_targets_left_after_guess,max_targets_left_after_guess,n_possible_cluniques_after_guess\n");

    int n_cluniques = ipow(3, targetlen);
    char clues[targetlen + 1];
    time_t tstart = time(NULL);

    // Try each guess word...
    for (int ii=0; ii<ngw; ii++) {
        const char *guess = guesses[ii];
        int cluniques[n_cluniques];
        bzero(cluniques, sizeof(int) * n_cluniques);

        // Try each target word...
        for (int jj=0; jj<ntw; jj++) {
            const char *target = targets[jj];
            int clunique;

            // Figure out the clues, and the "clunique" category of that guess+target combo
            char clues[targetlen];
            clues_of_guess(targetlen, guess, target, clues, &clunique);
            cluniques[clunique]++;
        }

        // Sort into reverse order
        qsort(cluniques, n_cluniques, sizeof(int), comp_int_rev);
        int worst_left = cluniques[0];

        // Each "clunique" category contains a certain number of target words. Each of those would leave
        // the self-same number of target words as remaining possibilities if we guessed any of them.
        // Thus, to calculate the average number of remaining words, we accumulate the square.
        // ("N target words, each of which would leave N remaining words if we made this guess")
        int nc = 0, acc = 0, acc2 = 0, ntw50 = (ntw / 2);
        double pct50 = 0;
        for (nc=0; nc<n_cluniques && cluniques[nc]; nc++) {
            int bucket = cluniques[nc];  // How many targets in this "clunique" category
            if ((acc < ntw50) && (acc + bucket >= ntw50)) {
                int last_bucket = nc > 0 ? cluniques[nc - 1] : 0;
                double weight = (ntw50 - acc)/((double)bucket);
                pct50 = bucket * weight + last_bucket * (1.0-weight);
            }
            acc += bucket;
            acc2 += bucket * bucket;
        }

        // Output results.
        double avg_left = ((double)acc2) / ((double)ntw);
        printf("\"%s\",%g,%g,%d,%d\n", guess, avg_left, pct50, worst_left, nc);
        fflush(stdout);

        fprintf(stderr, "(%d/%d) First guess of \"%s\" leaves %g/%d possible targets on average, %g median, %d at worst. Populates %d cluniques.\n",
                ii+1, ngw, guess, avg_left, ntw, pct50, worst_left, nc);
    }

    fprintf(stderr, "Crunched %d guesses in %ld seconds (%ld inner loops/second).\n",
            ngw, time(NULL)-tstart, (ngw*ntw)/((time(NULL)-tstart) ?: 1));
    return 0;
}
