/*
 * Run by setup.sh against the trimmed ICU data in build/icu. Exercises every
 * ICU data path libltfs uses, so it fails if the trimmed package lacks one.
 */
#include <assert.h>
#include <stdio.h>
#include <unicode/ubrk.h>
#include <unicode/ucnv.h>
#include <unicode/uloc.h>
#include <unicode/unorm2.h>
#include <unicode/ustring.h>

int main(void)
{
	UErrorCode e = U_ZERO_ERROR;

	/* NFC/NFD (pathname.c): e + combining acute <-> U+00E9 */
	UChar nfd[] = {'e', 0x301, 0}, out[8];
	assert(unorm2_normalize(unorm2_getNFCInstance(&e), nfd, -1, out, 8, &e) == 1 && out[0] == 0xE9);
	assert(unorm2_normalize(unorm2_getNFDInstance(&e), out, 1, nfd, 8, &e) == 2 && U_SUCCESS(e));

	/* case folding (pathname.c) */
	UChar up[] = {'A', 0xC9, 0};
	u_strFoldCase(out, 8, up, -1, U_FOLD_CASE_DEFAULT, &e);
	assert(out[1] == 0xE9 && U_SUCCESS(e));

	/* character break (index_criteria.c): flag emoji + decomposed e-acute = 2 graphemes */
	UChar s[] = {0xD83C, 0xDDEC, 0xD83C, 0xDDE7, 'e', 0x301, 0};
	const char *locs[] = {uloc_getDefault(), "en_US", "ja", "zh_Hant", "de", "th"};
	for (unsigned i = 0; i < sizeof locs / sizeof *locs; i++) {
		e = U_ZERO_ERROR;
		UBreakIterator *b = ubrk_open(UBRK_CHARACTER, locs[i], s, -1, &e);
		if (U_FAILURE(e)) {
			printf("FAIL ubrk_open %s: %s\n", locs[i], u_errorName(e));
			return 1;
		}
		assert(ubrk_next(b) == 4 && ubrk_next(b) == 6 && ubrk_next(b) == UBRK_DONE);
		ubrk_close(b);
	}

	/* UTF-8 converter (pathname.c, ltfslogging.c): u-umlaut, and a stray byte rejected */
	e = U_ZERO_ERROR;
	UConverter *c = ucnv_open("UTF-8", &e);
	ucnv_setToUCallBack(c, UCNV_TO_U_CALLBACK_STOP, NULL, NULL, NULL, &e);
	assert(ucnv_toUChars(c, out, 8, "\xC3\xBC", -1, &e) == 1 && out[0] == 0xFC && U_SUCCESS(e));
	ucnv_toUChars(c, out, 8, "\xFC", -1, &e);
	assert(e == U_ILLEGAL_CHAR_FOUND);
	ucnv_close(c);

	printf("ICU data check ok\n");
	return 0;
}
