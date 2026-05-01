#include "devices/vga.h"
#include <round.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include "threads/io.h"
#include "threads/interrupt.h"
#include "threads/vaddr.h"

/* VGA text screen 지원. 자세한 내용은 [FREEVGA]를 참고한다. */

/* text display의 열과 행 수. */
#define COL_CNT 80
#define ROW_CNT 25

/* 현재 cursor 위치. (0,0)은 display의 왼쪽 위 모서리다. */
static size_t cx, cy;

/* 검은 배경 위 회색 글자용 attribute 값. */
#define GRAY_ON_BLACK 0x07

/* framebuffer. [FREEVGA]의 "VGA Text Mode Operation"을 참고한다.
   (x,y)의 문자는 fb[y][x][0]이다.
   (x,y)의 attribute는 fb[y][x][1]이다. */
static uint8_t (*fb)[COL_CNT][2];

static void clear_row (size_t y);
static void cls (void);
static void newline (void);
static void move_cursor (void);
static void find_cursor (size_t *x, size_t *y);

/* VGA text display를 초기화한다. */
static void
init (void) {
	/* 이미 초기화되었는가? */
	static bool inited;
	if (!inited) {
		fb = ptov (0xb8000);
		find_cursor (&cx, &cy);
		inited = true;
	}
}

/* C를 VGA text display에 쓰며, control character는 일반적인 방식으로 해석한다. */
void
vga_putc (int c) {
	/* console에 쓸 수 있는 interrupt handler를 막기 위해 interrupt를 비활성화한다. */
	enum intr_level old_level = intr_disable ();

	init ();

	switch (c) {
		case '\n':
			newline ();
			break;

		case '\f':
			cls ();
			break;

		case '\b':
			if (cx > 0)
				cx--;
			break;

		case '\r':
			cx = 0;
			break;

		case '\t':
			cx = ROUND_UP (cx + 1, 8);
			if (cx >= COL_CNT)
				newline ();
			break;

		default:
			fb[cy][cx][0] = c;
			fb[cy][cx][1] = GRAY_ON_BLACK;
			if (++cx >= COL_CNT)
				newline ();
			break;
	}

	/* cursor 위치를 갱신한다. */
	move_cursor ();

	intr_set_level (old_level);
}

/* 화면을 지우고 cursor를 왼쪽 위로 이동한다. */
static void
cls (void) {
	size_t y;

	for (y = 0; y < ROW_CNT; y++)
		clear_row (y);

	cx = cy = 0;
	move_cursor ();
}

/* Y 행을 공백으로 지운다. */
static void
clear_row (size_t y) {
	size_t x;

	for (x = 0; x < COL_CNT; x++)
	{
		fb[y][x][0] = ' ';
		fb[y][x][1] = GRAY_ON_BLACK;
	}
}

/* cursor를 화면의 다음 줄 첫 번째 열로 이동한다. cursor가 이미 화면의 마지막
   줄에 있으면 화면을 한 줄 위로 scroll한다. */
static void
newline (void) {
	cx = 0;
	cy++;
	if (cy >= ROW_CNT)
	{
		cy = ROW_CNT - 1;
		memmove (&fb[0], &fb[1], sizeof fb[0] * (ROW_CNT - 1));
		clear_row (ROW_CNT - 1);
	}
}

/* hardware cursor를 (cx,cy)로 이동한다. */
static void
move_cursor (void) {
	/* [FREEVGA]의 "Manipulating the Text-mode Cursor"를 참고한다. */
	uint16_t cp = cx + COL_CNT * cy;
	outw (0x3d4, 0x0e | (cp & 0xff00));
	outw (0x3d4, 0x0f | (cp << 8));
}

/* 현재 hardware cursor 위치를 (*X,*Y)에 읽어 온다. */
static void
find_cursor (size_t *x, size_t *y) {
	/* [FREEVGA]의 "Manipulating the Text-mode Cursor"를 참고한다. */
	uint16_t cp;

	outb (0x3d4, 0x0e);
	cp = inb (0x3d5) << 8;

	outb (0x3d4, 0x0f);
	cp |= inb (0x3d5);

	*x = cp % COL_CNT;
	*y = cp / COL_CNT;
}
