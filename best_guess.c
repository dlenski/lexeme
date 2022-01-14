/*
 *  Wordle/Lexeme best-first-guess solver
 *  =====================================
 *
 *  Start with a dictionary containing N eligible words of length L.
 *  Assume all N words are equally likely as a target.
 *
 *  Q: What is the optimal first guess? That is, what first guess will
 *     ON AVERAGE leave the fewest possible remaining words to guess?
 *  A: We need to run O(N^2) iterations of clues_of_guess, and O(N^3)
 *     iterations of is_word_possible_after_guess. Each is about
 *     O(L) in runtime. Memory requirements are trivial.
 *
 *     Python is too damn slow, can't multithread itself out of a
 *     CPU-bound problem due to GIL, and would still be too slow if
 *     it could. Hence a C version :-)
 *
 *  usage: best_first_guess [wordlist.txt] [target_word_len] > results.csv
 *   e.g.: best_first_guess /usr/share/dict/american-english 5 > results.csv
 *
 *     (That takes about 2 hours to run on one i7-8665U core
 *     at 1.9 GHz...)
 *
 *  FIXME: This is embarrassingly parallelizable. With M CPU cores,
 *  just kick off M threads, have each run one possible guess, and
 *  gather the results.
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


const char *LETTERS = "ABCDEFGHIJKLMNOPQRSTUVWXYZ";
const int N_LETTERS = 26;

#define DEBUG_STDERR(...) fprintf (stderr, __VA_ARGS__)
#define DEBUG_NOTHING(...) do{}while(0)
// Change any of these to DEBUG_STDERR(__VA_ARGS__) to enable, 0 to disable:
#define DEBUG_CLUES(...)
#define DEBUG_IS_WORD_POSSIBLE(...)
#define DEBUG_ELIGIBLE_WORDS(...)

// C version of lexeme.algorithms.clues_of_guess
char *clues_of_guess(int len, const char *guess, const char *target, char *clues) {
    char leftovers[len];

    for (int ii=0; ii < len; ii++) {
        char gl = guess[ii], tl = target[ii];
        leftovers[ii] = (gl == tl ? '\0' : tl);
    }

    int cl = 0;
    for (int ii=0; ii < len; ii++) {
        char gl = guess[ii], tl = target[ii], *lo;
        int c;
        if (gl == tl) {
            DEBUG_CLUES("%s %s %c -> RP\n", guess, target, gl);
            c = RightPosition;
        } else if ((lo = memchr(leftovers, gl, len)) != NULL) {
            DEBUG_CLUES("%s %s %c -> WP\n", guess, target, gl);
            c = WrongPosition;
            *lo = '\0'; // remove it
        } else {
            DEBUG_CLUES("%s %s %c -> A\n", guess, target, gl);
            c = Absent;
        }
        clues[ii] = c;
    }

    clues[len] = '\0';
    return clues;
}

/*  This is a structure to store clues in a way that is intended
 *  to make 'is_possible_word' as efficient as possible to execute,
 *  while being equivalent to the standard string of 3-valued
 *  clues.
 *
 *  The 'clues_of_guess' function only runs (Nguesses * Ntargets)
 *  times, whereas the main solver has to call 'is_possible_word'
 *  (Nguesses * Ntargets^2) times.
 *
 *  Interpretation of structure elements:
 *
 *  If a bit is set, must be a particular letter at that position:
 *    must_be[pos] & (1<<(letter-'A')) -> pos must be letter
 *  If a bit is set, must NOT be a particular letter at that position:
 *    must_not_be[pos] & (1<<(letter-'A')) -> pos must NOT be letter
 *  Must have at least/most this many of a given letter:
 *    have[].at_least -> must have at least this many of .c
 *    have[].at_most  -> must have at most  this many of .c
 *
 *  -> RightPosition clue is equivalent to 1 must_be,     1 have_at_least
 *  -> WrongPosition clue is equivalent to 1 must_not_be, 1 have_at_least, 1 have_at_most
 *  -> Absent        clue is equivalent to 1 must_not_be, 1 have_at_most
 *       (special case: set ALL positions to must_not_be if ALL clues for this letter are Absent)
 */

struct _have { char c; uint8_t at_most; uint8_t at_least; };
#define STRUCT_CLUEVEC(len, nlet, ...) \
    struct __VA_ARGS__ {               \
        uint32_t must_be[len];         \
        uint32_t must_not_be[len];     \
        struct _have have[nlet];       \
    }

int compar_have(const void *_a, const void *_b) {
    const struct _have *a = _a, *b = _b;
    return a->at_least != b->at_least
        ? -(a->at_least - b->at_least)   // Sort higher at_least letters first
        :  (a->at_most  - b->at_most );  // Tiebreaker: Sort lower at_most letters first
}

const void *cluevec_of_guess(int len, const char *guess, const char *target, void *cluevec) {
    STRUCT_CLUEVEC(len, N_LETTERS) *cv = cluevec;

    char leftovers[len];
    bzero(leftovers, len);

    DEBUG_CLUES("guess=%s, target=%s composing cluevec:\n", guess, target);
    bzero(cv, sizeof(*cv));
    for (int ii=0; ii < N_LETTERS; ii++) {
        cv->have[ii].c = ii+'A';
        cv->have[ii].at_most = len;
    }

    // Pass 1: distinguish RP from WP/A
    for (int ii=0; ii < len; ii++) {
        char gl = guess[ii], tl = target[ii];
        uint32_t gl_off = gl-'A', tl_off = tl-'A', gl_bit = 1<<gl_off, tl_bit=1<<tl_off;

        if (gl == tl) {
            // RightPosition
            cv->must_be[ii] = gl_bit;
            cv->have[gl_off].at_least++;
        } else {
            // WrongPosition/Absent
            cv->must_not_be[ii] = gl_bit;
            cv->have[gl_off].at_most--;
            leftovers[ii] = tl; // Save for pass 2
        }
    }
    // Pass 2: distinguish WP from A
    for (int ii=0; ii < len; ii++) {
        char gl = guess[ii], tl = target[ii], *lo;
        uint32_t gl_off = gl-'A', tl_off = tl-'A', gl_bit = 1<<gl_off, tl_bit=1<<tl_off;

        if (gl == tl) {
            // RightPosition -> No further effect
        } else if ((lo = memchr(leftovers, gl, len)) != NULL) {
            // WrongPosition -> have_at_least[gl]++
            cv->have[gl_off].at_least++;
            // Remove this letter from further consideration
            *lo = '\0';
        } else {
            // Absent        -> have_at_most[gl] = have_at_least[gl]
            if ((cv->have[gl_off].at_most = cv->have[gl_off].at_least) == 0) {
                // Zero occurrences! -> Add gl to must_not_be[*]
                for (int jj=0; jj < len; jj++)
                    cv->must_not_be[jj] |= gl_bit;
            }
        }
        DEBUG_CLUES("must_be[%2d]     = 0x%08x ('%c'), ", ii, cv->must_be[ii],
                    __builtin_ffs(cv->must_be[ii]) ? __builtin_ffs(cv->must_be[ii]) - 1 + 'A' : 0);
        DEBUG_CLUES("must_not_be[%2d] = 0x%08x ('%c')\n", ii, cv->must_not_be[ii],
                    __builtin_ffs(cv->must_not_be[ii]) ? __builtin_ffs(cv->must_not_be[ii]) - 1 + 'A' : 0);
        DEBUG_CLUES("have['%c'-'A'] = {.c='%c', .at_least=%d, .at_most=%d}\n",
                    gl_off+'A', cv->have[gl_off].c, cv->have[gl_off].at_least, cv->have[gl_off].at_most);
    }

    // Sort the have-at-least/have-at-most clues to put the most significant ones first,
    // and discard the ones that give no additional value:
    //   at_most == 0                    -> already set must_not_be for all positions
    //   at_least == 0 && at_most == len -> always true
    qsort(cv->have, N_LETTERS, sizeof(cv->have[0]), compar_have);
    for (int ii=0; ii < N_LETTERS; ii++) {
        struct _have *h = &cv->have[ii];
        if ((h->at_most == 0) || (h->at_least == 0 && h->at_most == len)) {
            h->c = '\0';
            break;
        }
        DEBUG_CLUES("have[%d] = {.c='%c', .at_least=%d, .at_most=%d}\n", ii, h->c, h->at_least, h->at_most);
    }

    return cv;
}

void test_guess_clues() {
    char clues[7];

    assert(!strcmp(clues_of_guess(5, "SWEAT",  "FLEAS",  clues), "WARRA"));
    assert(!strcmp(clues_of_guess(5, "REELS",  "REBUS",  clues), "RRAAR"));
    assert(!strcmp(clues_of_guess(5, "ROARS",  "BEARS",  clues), "AARRR"));
    assert(!strcmp(clues_of_guess(5, "ARIAS",  "PAPAS",  clues), "WAARR"));
    assert(!strcmp(clues_of_guess(5, "ALAMO",  "ARIAS",  clues), "RAWAA"));
    assert(!strcmp(clues_of_guess(6, "REVILE", "SEVENS", clues), "ARRAAW"));
    assert(!strcmp(clues_of_guess(6, "EVENER", "SEVENS", clues), "WWWWAA"));
    assert(!strcmp(clues_of_guess(5, "AAHED",  "ABEAM",  clues), "RWAWA")); // <-- Was a Py->C bug!
}

// C version of lexeme.algorithms.is_word_possible_after_guess
int is_word_possible_after_guess(int len, const char *guess, const char *word, const char *clues) {
    int wp_guess[N_LETTERS];
    int a_guess[N_LETTERS];
    int left_word[N_LETTERS];

    bzero(wp_guess, N_LETTERS*sizeof(int));
    bzero(a_guess, N_LETTERS*sizeof(int));
    bzero(left_word, N_LETTERS*sizeof(int));

    for (int ii=0; ii < len; ii++) {
        char gl = guess[ii], wl = word[ii], s = clues[ii];
        if ((gl == wl) && s != RightPosition) {
            DEBUG_IS_WORD_POSSIBLE("1) %d %c==%c but %c!=R\n", ii, gl, wl, s);
            return 0;   // Guess and word share a letter which is NOT marked as RP in the guess
        } else if ((gl != wl) && s == RightPosition) {
            DEBUG_IS_WORD_POSSIBLE("2) %d %c!=%c but %c==R\n", ii, gl, wl, s);
            return 0;   // Guess and word differ in a letter which IS marked as RP in the guess
        } else {
            // Count number of leftover (non-RP) letters in the word
            if (s != RightPosition)
                left_word[wl-'A']++;

            // Count number of A/WP letters in the guess
            if (s == WrongPosition)
                wp_guess[gl-'A']++;
            else if (s == Absent)
                a_guess[gl-'A']++;
        }
    }

    // 1. Make sure there are enough of the WP letters from the guess in the word
    // 2. Make sure there aren't any of the A letters from the guess in the word, after discounting
    //    the WP letters from the guess.
    for (int ii=0; ii < N_LETTERS; ii++) {
        // char c = ii+'A';
        if (left_word[ii] < wp_guess[ii]) {
            DEBUG_IS_WORD_POSSIBLE("3) %d '%c' counts %d<%d\n", ii, ii+'A', left_word[ii], wp_guess[ii]);
            return 0; // Guess has more of these letters as WP than the word has leftover
        }
        if (a_guess[ii] && (left_word[ii] > wp_guess[ii])) {
            DEBUG_IS_WORD_POSSIBLE("4) %d '%c' counts (%d!=0 && %d>%d)\n", ii, ii+'A', a_guess[ii], left_word[ii], wp_guess[ii]);
            return 0; // Guess has this letter as A, and word still has some left
        }
    }

    // It's (still) a possible match
    return 1;
}

inline int is_word_possible_after_cluevec(int len, const char *word, const void *cluevec) {
    const STRUCT_CLUEVEC(len, N_LETTERS) *cv = cluevec;

    uint32_t count[N_LETTERS];
    bzero(count, sizeof(count));

    for (int ii=0; ii < len; ii++) {
        char wl = word[ii];
        uint32_t wl_off = wl-'A', wl_bit = 1<<wl_off;
        if (cv->must_be[ii]     & (~wl_bit))  return 0;  // Must be some other letter
        if (cv->must_not_be[ii] &   wl_bit)   return 0;  // Must not be this letter
        count[wl_off]++;
    }
    for (int ii=0; ii < N_LETTERS; ii++) {
        const struct _have *h = &cv->have[ii];
        if (h->c == '\0')
            break;
        if (count[h->c-'A'] < h->at_least) return 0;  // Not enough of this letter
        if (count[h->c-'A'] > h->at_most ) return 0;  // Too much of this letter
    }
    return 1;
}

void test_word_possible_after_guess() {
    char clues[7];

    assert(!strcmp(clues_of_guess       (6, "VXXXXX", "ADDUCE", clues), "AAAAAA"));
    assert( is_word_possible_after_guess(6, "VXXXXX", "ADDUCE", clues));
    assert( is_word_possible_after_guess(6, "VXXXXX", "DEDUCE", clues));
    assert(!is_word_possible_after_guess(6, "VXXXXX", "ADVICE", clues));

    assert(!strcmp(clues_of_guess       (6, "XXXXXV", "VIOLAS", clues), "AAAAAW"));
    assert( is_word_possible_after_guess(6, "XXXXXV", "VESSEL", clues));
    assert( is_word_possible_after_guess(6, "XXXXXV", "VIOLAS", clues));
    assert( is_word_possible_after_guess(6, "XXXXXV", "VIOLIN", clues));
    assert(!is_word_possible_after_guess(6, "XXXXXV", "ADDUCE", clues));

    assert(!strcmp(clues_of_guess       (6, "ADVICE", "EVENER", clues), "AAWAAW"));
    assert( is_word_possible_after_guess(6, "ADVICE", "EVENER", clues));
    assert( is_word_possible_after_guess(6, "ADVICE", "VESSEL", clues));
    assert(!is_word_possible_after_guess(6, "ADVICE", "DEVILS", clues));

    assert(!strcmp(clues_of_guess       (5, "AAHED",  "ABEAM",  clues), "RWAWA")); // <-- Was a Py->C bug
    assert( is_word_possible_after_guess(5, "AAHED",  "ABASE",  clues));

    assert(!strcmp(clues_of_guess       (5, "AAEHD",  "AAHED",  clues), "RRWWR"));
    assert( is_word_possible_after_guess(5, "AAEHD",  "AAHED",  clues));
}

void test_word_possible_after_cluevec() {
    STRUCT_CLUEVEC(6, N_LETTERS) *cluevec = calloc(1, sizeof(*cluevec));

    cluevec_of_guess                      (6, "VXXXXX", "ADDUCE", cluevec);
    assert( is_word_possible_after_cluevec(6, "ADDUCE", cluevec));
    assert( is_word_possible_after_cluevec(6, "DEDUCE", cluevec));
    assert(!is_word_possible_after_cluevec(6, "ADVICE", cluevec));

    cluevec_of_guess                      (6, "ADVICE", "EVENER", cluevec);
    assert( is_word_possible_after_cluevec(6, "EVENER", cluevec));
    assert( is_word_possible_after_cluevec(6, "VESSEL", cluevec));
    assert(!is_word_possible_after_cluevec(6, "DEVILS", cluevec));

    cluevec_of_guess                      (5, "AAHED", "ABEAM", cluevec);
    assert( is_word_possible_after_cluevec(5, "ABEAM", cluevec));

    cluevec_of_guess                      (5, "AAEHD", "AAHED", cluevec);
    assert( is_word_possible_after_cluevec(5, "AAHED", cluevec));

    free(cluevec);
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
    char **buf = calloc(bufsize, sizeof(*output));
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
            buf = realloc(buf, bufsize * sizeof(*output));
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

#define USE_CLUEVEC 1

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
    test_word_possible_after_cluevec();

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
    printf("guess,avg_targets_left_after_guess,max_targets_left_after_guess\n");

    char clues[targetlen + 1];
    time_t tstart = time(NULL);

    // Try each guess word...
    for (int ii=0; ii<ngw; ii++) {
        const char *guess = guesses[ii];
        int acc = 0, worst_left = 0;

        // Try each target word...
        for (int jj=0; jj<ntw; jj++) {
            const char *target = targets[jj];
            int count = 0;

#if USE_CLUEVEC
            // What clues do we get from that guess against each target?
            STRUCT_CLUEVEC(targetlen, N_LETTERS) cluevec;
            cluevec_of_guess(targetlen, guess, target, &cluevec);
#else
            char clues[targetlen];
            clues_of_guess(targetlen, guess, target, clues);
#endif

            // How many of the target words are still possible?
            for (int kk=0; kk<ntw; kk++) {
                const char *ptw = targets[kk];
                if (
#if USE_CLUEVEC
                    is_word_possible_after_cluevec(targetlen, ptw, &cluevec)
#else
                    is_word_possible_after_guess(targetlen, guess, ptw, clues)
#endif
                    )
                    count++;
            }
            acc += count;
            if (count > worst_left)
                worst_left = count;
            //printf("\"%s\",\"%s\",%d\n", guess, target, nleft);
        }

        // Output results.
        double avg_left = ((double)acc) / ((double)ntw);
        printf("\"%s\",%g,%d\n", guess, avg_left, worst_left);
        fflush(stdout);

        fprintf(stderr, "(%d/%d) First guess of \"%s\" leaves %g/%d possible targets on average, %d at worst. Solving at %g sec/guess\n",
                ii+1, ngw, guess, avg_left, ntw, worst_left, (time(NULL) - tstart)/((double)(ii+1)));
    }

    fprintf(stderr, "Crunched %d guesses in %ld seconds (%g guesses/second).\n",
            ngw, time(NULL)-tstart, ((double)ngw)/((double)((time(NULL)-tstart) ?: 1)));
    return 0;
}
