
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

#include <sys/kassert.h>
#include <sys/kdebug.h>
#include <sys/irq.h>

#include <machine/amd64.h>
#include <machine/amd64op.h>

#include "../console.h"
#include "ps2.h"

// Status/Command Port (Input/Output)
#define PS2_STATUS_PORT		0x64
#define PS2_COMMAND_PORT	0x64
#define PS2_DATA_PORT		0x60

// Controller Status
#define PS2_STATUS_OBS		0x01	/* Input Buffer Ready */
#define PS2_STATUS_IBS		0x02	/* Output Buffer Full */

// Controller Commands
#define PS2_COMMAND_AUXDISABLE	0xA7	/* Disable Auxiliary */
#define PS2_COMMAND_AUXENABLE	0xA8	/* Enable Auxiliary */
#define PS2_COMMAND_SELFTEST	0xAA	/* Test Controller */
#define PS2_COMMAND_KBDDISABLE	0xAD	/* Disable Keyboard */
#define PS2_COMMAND_KBDENABLE	0xAE	/* Enable Keyboard */

// Keyboard Commands
#define PS2_KBD_RESET		0xFF	/* Reset */
#define PS2_KBD_SETLED		0xED	/* Set LED */

// Keyboard Response
#define PS2_KBD_RESETDONE	0xAA	/* Reset Done */

static IRQHandler kbdHandler;
static IRQHandler psmHandler;

// Key state flags
#define KS_SHIFT		0x01
#define KS_ALT			0x02
#define KS_CONTROL		0x04
#define KS_CAPSLOCK		0x08
#define KS_NUMLOCK		0x10
#define KS_SCROLLLOCK		0x20
#define KS_E0ESCAPE		0x30
static uint32_t keyState;

static uint8_t keyMap[256] = {
    0x00, 0x1B,  '1',  '2',  '3',  '4',  '5',  '6',
     '7',  '8',  '9',  '0',  '-',  '=',  0x08, 0x09,
     'q',  'w',  'e',  'r',  't',  'y',  'u',  'i',
     'o',  'p',  '[',  ']',  0x0A, 0x00, 'a',  's',
     'd',  'f',  'g',  'h',  'j',  'k',  'l',  ';',
     '\'', '`',  0x00, '\\', 'z',  'x',  'c',  'v',
     'b',  'n',  'm',  ',',  '.',  '/',  0x00, '*',
    0x00,  ' ', KEY_F1, KEY_F2, KEY_F3, KEY_F4, KEY_F5,

    KEY_F6, KEY_F7, KEY_F8, KEY_F9, KEY_F10, 0x00, 0x00, '7',
     '8',  '9',  '-',  '4',  '5',  '6',  '+',  '1',
     '2',  '3',  '0',  '.', 0x00, 0x00, 0x00, KEY_F11,
    KEY_F12, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,

    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,

    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};

static uint8_t shiftKeyMap[256] = {
    0x00, 0x1B,  '!',  '@',  '#',  '$',  '%',  '^',
     '&',  '*',  '(',  ')',  '_',  '+',  0x08, 0x09,
     'Q',  'W',  'E',  'R',  'T',  'Y',  'U',  'I',
     'O',  'P',  '{',  '}',  0x0A, 0x00, 'A',  'S',
     'D',  'F',  'G',  'H',  'J',  'K',  'L',  ':',
     '"',  '~',  0x00, '|',  'Z',  'X',  'C',  'V',
     'B',  'N',  'M',  '<',  '>',  '?',  0x00, '*',
    0x00,  ' ', KEY_F1, KEY_F2, KEY_F3, KEY_F4, KEY_F5,

    KEY_F6, KEY_F7, KEY_F8, KEY_F9, KEY_F10, 0x00, 0x00, '7',
     '8',  '9',  '-',  '4',  '5',  '6',  '+',  '1',
     '2',  '3',  '0',  '.', 0x00, 0x00, 0x00, KEY_F11,
    KEY_F12, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,

    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,

    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};

static uint8_t controlKeyMap[256] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,

    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,

    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,

    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};

static void
PS2_KeyboardIntr(void *arg)
{
    uint8_t status;
    uint8_t data;
    uint8_t key;

    while (1) {
	status = inb(PS2_STATUS_PORT);
        if ((status & PS2_STATUS_OBS) == 0)
	    return;

	data = inb(PS2_DATA_PORT);

	// E0 escape sequence
	if (data == 0xE0) {
	    keyState |= KS_E0ESCAPE;
	    continue;
	}

	// Check for release of shift, alt, control
	if (data & 0x80) {
	    if (data == 0xAA || data == 0xB6)
		keyState &= ~KS_SHIFT;
	    if (data == 0x9D)
		keyState &= ~KS_CONTROL;
	    if (data == 0x38)
		keyState &= ~KS_ALT;

	    // Other key release
	    continue;
	}

	// Check for shift, alt, control
	if (data == 0x2A || data == 0x36)
	    keyState |= KS_SHIFT;
	if (data == 0x1D)
	    keyState |= KS_CONTROL;
	if (data == 0x38)
	    keyState |= KS_ALT;

	// Check for numlock, capslock, scrolllock
	if (data == 0x3A)
	    keyState ^= KS_CAPSLOCK;
	if (data == 0x45)
	    keyState ^= KS_NUMLOCK;
	if (data == 0x46)
	    keyState ^= KS_SCROLLLOCK;

	// Decode character
	if (keyState & KS_E0ESCAPE) {
	    keyState &= ~KS_E0ESCAPE;

	    // Escape sequences
	    continue;
	}

	// Debugger sequences
	if ((keyState & (KS_ALT | KS_CONTROL)) == (KS_ALT | KS_CONTROL)) {
	    // Debugger: Alt-Control-Backspace
	    if (data == 0x0E)
		breakpoint();

	    // Reboot: Alt-Control-Delete
	}

	int isShift = 0;
	if (keyState & KS_SHIFT)
	    isShift = 1;
	if (keyState & KS_CAPSLOCK)
	    isShift ^= 1;

	if (keyState & KS_CONTROL) {
	    key = controlKeyMap[data];
	} else if (isShift) {
	    key = shiftKeyMap[data];
	} else {
	    key = keyMap[data];
	}

	if (key == 0x00)
	    continue;

	//kprintf("Key Press: %02X\n", key);
	Console_EnqueueKey(key);

	continue;
    }
}

static void
PS2_MouseIntr(void *arg)
{
}

void
PS2Wait()
{
    uint8_t status;

    while (1) {
	status = inb(PS2_STATUS_PORT);
	if ((status & PS2_STATUS_OBS) != 0)
	    return;
    }
}

void
PS2_Init()
{
    uint8_t status, data;

    keyState = 0;

    outb(PS2_COMMAND_PORT, PS2_COMMAND_KBDDISABLE);

    while (1) {
	status = inb(PS2_STATUS_PORT);
        if ((status & PS2_STATUS_OBS) == 0)
	    break;

	data = inb(PS2_DATA_PORT);
    }

    // Self test
    outb(PS2_COMMAND_PORT, PS2_COMMAND_SELFTEST);
    PS2Wait();
    data = inb(PS2_DATA_PORT);
    if (data != 0x55)
	kprintf("PS2: Controller test failed\n");

    outb(PS2_COMMAND_PORT, PS2_COMMAND_KBDENABLE);

    kbdHandler.irq = 1;
    kbdHandler.cb = &PS2_KeyboardIntr;
    kbdHandler.arg = NULL;
    psmHandler.irq = 12;
    psmHandler.cb = &PS2_MouseIntr;
    psmHandler.arg = NULL;

    IRQ_Register(1, &kbdHandler);
    IRQ_Register(12, &psmHandler);
}

