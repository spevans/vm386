/* keycodes.h -- Scan code definitions.
   John Harper. */

#ifndef _VMM_KEYCODES_H
#define _VMM_KEYCODES_H

/* All of these key codes are the same as the key's actual scan code. */

#define K_ESC		0x01
#define K_1		0x02
#define K_2		0x03
#define K_3		0x04
#define K_4		0x05
#define K_5		0x06
#define K_6		0x07
#define K_7		0x08
#define K_8		0x09
#define K_9		0x0a
#define K_0		0x0b
#define K_MINUS		0x0c
#define K_EQUALS	0x0d
#define K_BACKSPACE	0x0e
#define K_TAB		0x0f
#define K_Q		0x10
#define K_W		0x11
#define K_E		0x12
#define K_R		0x13
#define K_T		0x14
#define K_Y		0x15
#define K_U		0x16
#define K_I		0x17
#define K_O		0x18
#define K_P		0x19
#define K_OPEN_BRACK	0x1a
#define K_CLOSE_BRACK	0x1b
#define K_ENTER		0x1c
#define K_LEFT_CTRL	0x1d
#define K_A		0x1e
#define K_S		0x1f
#define K_D		0x20
#define K_F		0x21
#define K_G		0x22
#define K_H		0x23
#define K_J		0x24
#define K_K		0x25
#define K_L		0x26
#define K_SEMI_COLON	0x27
#define K_QUOTE		0x28
#define K_CLOSE_QUOTE	0x29
#define K_LEFT_SHIFT	0x2a
#define K_BACKSLASH	0x2b
#define K_Z		0x2c
#define K_X		0x2d
#define K_C		0x2e
#define K_V		0x2f
#define K_B		0x30
#define K_N		0x31
#define K_M		0x32
#define K_COMMA		0x33
#define K_DOT		0x34
#define K_SLASH		0x35
#define K_RIGHT_SHIFT	0x36
#define K_KP_ASTERISK	0x37
#define K_LEFT_ALT	0x38
#define K_SPACE		0x39
#define K_CAPS_LOCK	0x3a
#define K_F1		0x3b
#define K_F2		0x3c
#define K_F3		0x3d
#define K_F4		0x3e
#define K_F5		0x3f
#define K_F6		0x40
#define K_F7		0x41
#define K_F8		0x42
#define K_F9		0x43
#define K_F10		0x44
#define K_NUM_LOCK	0x45
#define K_SCROLL_LOCK	0x46
#define K_KP_7		0x47
#define K_KP_8		0x48
#define K_KP_9		0x49
#define K_KP_MINUS	0x4a
#define K_KP_4		0x4b
#define K_KP_5		0x4c
#define K_KP_6		0x4d
#define K_KP_PLUS	0x4e
#define K_KP_1		0x4f
#define K_KP_2		0x50
#define K_KP_3		0x51
#define K_KP_0		0x52
#define K_KP_DOT	0x53
#define K_SYS_REQ	0x54

#define K_F11		0x57
#define K_F12		0x58

/* The following are keys which will be escaped by 0xe0, they get given
   unused key codes. */

#define K_E0_KP_ENTER	0x60
#define K_E0_RIGHT_CTRL	0x61
#define K_E0_KP_SLASH	0x62
#define K_E0_PRT_SCR	0x63
#define K_E0_RIGHT_ALT	0x64
#define K_E0_BREAK	0x65
#define K_E0_HOME	0x66
#define K_E0_UP		0x67
#define K_E0_PAGE_UP	0x68
#define K_E0_LEFT	0x69
#define K_E0_RIGHT	0x6a
#define K_E0_END	0x6b
#define K_E0_DOWN	0x6c
#define K_E0_PAGE_DOWN	0x6d
#define K_E0_INSERT	0x6e
#define K_E0_DELETE	0x6f
#define K_E1_PAUSE	0x77

#endif /* _VMM_KEYCODES_H */
