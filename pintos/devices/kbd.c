#include "devices/kbd.h"
#include <ctype.h>
#include <debug.h>
#include <stdio.h>
#include <string.h>
#include "devices/input.h"
#include "threads/interrupt.h"
#include "threads/io.h"

/* keyboard data register port. */
#define DATA_REG 0x60

/* shift key의 현재 상태.
   눌려 있으면 true, 아니면 false. */
static bool left_shift, right_shift;    /* 왼쪽과 오른쪽 Shift key. */
static bool left_alt, right_alt;        /* 왼쪽과 오른쪽 Alt key. */
static bool left_ctrl, right_ctrl;      /* 왼쪽과 오른쪽 Ctrl key. */

/* caps lock 상태.
   켜져 있으면 true, 꺼져 있으면 false. */
static bool caps_lock;

/* 눌린 key 수. */
static int64_t key_cnt;

static intr_handler_func keyboard_interrupt;

/* keyboard를 초기화한다. */
void
kbd_init (void) {
	intr_register_ext (0x21, keyboard_interrupt, "8042 Keyboard");
}

/* keyboard 통계를 출력한다. */
void
kbd_print_stats (void) {
	printf ("Keyboard: %lld keys pressed\n", key_cnt);
}

/* 연속된 scancode 묶음을 문자로 매핑한다. */
struct keymap {
	uint8_t first_scancode;     /* 첫 scancode. */
	const char *chars;          /* chars[0]은 first_scancode의 scancode를,
								   chars[1]은 first_scancode + 1의 scancode를
								   가지며 문자열 끝까지 이어진다. */
};

/* shift key가 눌렸는지와 상관없이 같은 문자를 만드는 key들.
   알파벳 대소문자는 다른 곳에서 처리하는 예외다. */
static const struct keymap invariant_keymap[] = {
	{0x01, "\033"},
	{0x0e, "\b"},
	{0x0f, "\tQWERTYUIOP"},
	{0x1c, "\r"},
	{0x1e, "ASDFGHJKL"},
	{0x2c, "ZXCVBNM"},
	{0x37, "*"},
	{0x39, " "},
	{0, NULL},
};

/* shift 없이 눌렀을 때의 문자. shift 여부가 의미 있는 key들에 해당한다. */
static const struct keymap unshifted_keymap[] = {
	{0x02, "1234567890-="},
	{0x1a, "[]"},
	{0x27, ";'`"},
	{0x2b, "\\"},
	{0x33, ",./"},
	{0, NULL},
};

/* shift와 함께 눌렀을 때의 문자. shift 여부가 의미 있는 key들에 해당한다. */
static const struct keymap shifted_keymap[] = {
	{0x02, "!@#$%^&*()_+"},
	{0x1a, "{}"},
	{0x27, ":\"~"},
	{0x2b, "|"},
	{0x33, "<>?"},
	{0, NULL},
};

static bool map_key (const struct keymap[], unsigned scancode, uint8_t *);

static void
keyboard_interrupt (struct intr_frame *args UNUSED) {
	/* shift key 상태. */
	bool shift = left_shift || right_shift;
	bool alt = left_alt || right_alt;
	bool ctrl = left_ctrl || right_ctrl;

	/* keyboard scancode. */
	unsigned code;

	/* key가 눌렸으면 false, 놓였으면 true. */
	bool release;

	/* `code'에 대응하는 문자. */
	uint8_t c;

	/* prefix code라면 두 번째 byte까지 포함해 scancode를 읽는다. */
	code = inb (DATA_REG);
	if (code == 0xe0)
		code = (code << 8) | inb (DATA_REG);

	/* bit 0x80은 key press와 key release를 구분한다.
	   prefix가 있어도 마찬가지다. */
	release = (code & 0x80) != 0;
	code &= ~0x80u;

	/* key를 해석한다. */
	if (code == 0x3a) {
		/* caps lock. */
		if (!release)
			caps_lock = !caps_lock;
	} else if (map_key (invariant_keymap, code, &c)
			|| (!shift && map_key (unshifted_keymap, code, &c))
			|| (shift && map_key (shifted_keymap, code, &c))) {
		/* 일반 문자. */
		if (!release) {
			/* ctrl과 shift를 처리한다.
			   ctrl이 shift보다 우선한다는 점에 주의한다. */
			if (ctrl && c >= 0x40 && c < 0x60) {
				/* A는 0x41, Ctrl+A는 0x01 등이다. */
				c -= 0x40;
			} else if (shift == caps_lock)
				c = tolower (c);

			/* high bit를 설정해 Alt를 처리한다.
			   여기의 0x80은 key press와 key release를 구분하는 데 쓰인
			   0x80과 관련이 없다. */
			if (alt)
				c += 0x80;

			/* keyboard buffer에 추가한다. */
			if (!input_full ()) {
				key_cnt++;
				input_putc (c);
			}
		}
	} else {
		/* keycode를 shift 상태 변수에 매핑한다. */
		struct shift_key {
			unsigned scancode;
			bool *state_var;
		};

		/* shift key table. */
		static const struct shift_key shift_keys[] = {
			{  0x2a, &left_shift},
			{  0x36, &right_shift},
			{  0x38, &left_alt},
			{0xe038, &right_alt},
			{  0x1d, &left_ctrl},
			{0xe01d, &right_ctrl},
			{0,      NULL},
		};

		const struct shift_key *key;

		/* table을 훑는다. */
		for (key = shift_keys; key->scancode != 0; key++)
			if (key->scancode == code) {
				*key->state_var = !release;
				break;
			}
	}
}

/* keymap 배열 K에서 SCANCODE를 찾는다.
   찾으면 *C를 대응하는 문자로 설정하고 true를 반환한다.
   찾지 못하면 false를 반환하고 C는 무시된다. */
static bool
map_key (const struct keymap k[], unsigned scancode, uint8_t *c) {
	for (; k->first_scancode != 0; k++)
		if (scancode >= k->first_scancode
				&& scancode < k->first_scancode + strlen (k->chars)) {
			*c = k->chars[scancode - k->first_scancode];
			return true;
		}

	return false;
}
