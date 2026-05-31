/*
 * keys.h - physical scancodes, mouse buttons, and modifier flags.
 *
 * rc_scancode values match GLFW key constants directly so the GLFW backend
 * can cast without a lookup table.  An alternative backend must map its own
 * codes to these values before dispatching events.
 *
 * Use rc_scancode for key-down/key-up events: it identifies the physical
 * position of the key on the keyboard regardless of the active layout.
 * For text input, use the codepoint passed to the on_key_char callback.
 */

#ifndef RC_APP_KEYS_H_
#define RC_APP_KEYS_H_

/* ---- modifier flags ---- */

typedef enum {
    RC_MOD_SHIFT   = 0x0001,
    RC_MOD_CTRL    = 0x0002,
    RC_MOD_ALT     = 0x0004,
    RC_MOD_SUPER   = 0x0008,
    RC_MOD_CAPS    = 0x0010,
    RC_MOD_NUMLOCK = 0x0020
} rc_mod;

/* ---- mouse buttons ---- */

typedef enum {
    RC_MOUSE_BUTTON_LEFT   = 0,
    RC_MOUSE_BUTTON_RIGHT  = 1,
    RC_MOUSE_BUTTON_MIDDLE = 2
} rc_mouse_button;

/* ---- physical scancodes ---- */

typedef enum {
    /* printable keys */
    RC_SCANCODE_SPACE         =  32,
    RC_SCANCODE_APOSTROPHE    =  39,
    RC_SCANCODE_COMMA         =  44,
    RC_SCANCODE_MINUS         =  45,
    RC_SCANCODE_PERIOD        =  46,
    RC_SCANCODE_SLASH         =  47,
    RC_SCANCODE_0             =  48,
    RC_SCANCODE_1             =  49,
    RC_SCANCODE_2             =  50,
    RC_SCANCODE_3             =  51,
    RC_SCANCODE_4             =  52,
    RC_SCANCODE_5             =  53,
    RC_SCANCODE_6             =  54,
    RC_SCANCODE_7             =  55,
    RC_SCANCODE_8             =  56,
    RC_SCANCODE_9             =  57,
    RC_SCANCODE_SEMICOLON     =  59,
    RC_SCANCODE_EQUAL         =  61,
    RC_SCANCODE_A             =  65,
    RC_SCANCODE_B             =  66,
    RC_SCANCODE_C             =  67,
    RC_SCANCODE_D             =  68,
    RC_SCANCODE_E             =  69,
    RC_SCANCODE_F             =  70,
    RC_SCANCODE_G             =  71,
    RC_SCANCODE_H             =  72,
    RC_SCANCODE_I             =  73,
    RC_SCANCODE_J             =  74,
    RC_SCANCODE_K             =  75,
    RC_SCANCODE_L             =  76,
    RC_SCANCODE_M             =  77,
    RC_SCANCODE_N             =  78,
    RC_SCANCODE_O             =  79,
    RC_SCANCODE_P             =  80,
    RC_SCANCODE_Q             =  81,
    RC_SCANCODE_R             =  82,
    RC_SCANCODE_S             =  83,
    RC_SCANCODE_T             =  84,
    RC_SCANCODE_U             =  85,
    RC_SCANCODE_V             =  86,
    RC_SCANCODE_W             =  87,
    RC_SCANCODE_X             =  88,
    RC_SCANCODE_Y             =  89,
    RC_SCANCODE_Z             =  90,
    RC_SCANCODE_LEFT_BRACKET  =  91,
    RC_SCANCODE_BACKSLASH     =  92,
    RC_SCANCODE_RIGHT_BRACKET =  93,
    RC_SCANCODE_GRAVE         =  96,

    /* function / navigation keys */
    RC_SCANCODE_ESCAPE        = 256,
    RC_SCANCODE_ENTER         = 257,
    RC_SCANCODE_TAB           = 258,
    RC_SCANCODE_BACKSPACE     = 259,
    RC_SCANCODE_INSERT        = 260,
    RC_SCANCODE_DELETE        = 261,
    RC_SCANCODE_RIGHT         = 262,
    RC_SCANCODE_LEFT          = 263,
    RC_SCANCODE_DOWN          = 264,
    RC_SCANCODE_UP            = 265,
    RC_SCANCODE_PAGE_UP       = 266,
    RC_SCANCODE_PAGE_DOWN     = 267,
    RC_SCANCODE_HOME          = 268,
    RC_SCANCODE_END           = 269,
    RC_SCANCODE_CAPS_LOCK     = 280,
    RC_SCANCODE_SCROLL_LOCK   = 281,
    RC_SCANCODE_NUM_LOCK      = 282,
    RC_SCANCODE_PRINT_SCREEN  = 283,
    RC_SCANCODE_PAUSE         = 284,
    RC_SCANCODE_F1            = 290,
    RC_SCANCODE_F2            = 291,
    RC_SCANCODE_F3            = 292,
    RC_SCANCODE_F4            = 293,
    RC_SCANCODE_F5            = 294,
    RC_SCANCODE_F6            = 295,
    RC_SCANCODE_F7            = 296,
    RC_SCANCODE_F8            = 297,
    RC_SCANCODE_F9            = 298,
    RC_SCANCODE_F10           = 299,
    RC_SCANCODE_F11           = 300,
    RC_SCANCODE_F12           = 301,

    /* keypad */
    RC_SCANCODE_KP_0          = 320,
    RC_SCANCODE_KP_1          = 321,
    RC_SCANCODE_KP_2          = 322,
    RC_SCANCODE_KP_3          = 323,
    RC_SCANCODE_KP_4          = 324,
    RC_SCANCODE_KP_5          = 325,
    RC_SCANCODE_KP_6          = 326,
    RC_SCANCODE_KP_7          = 327,
    RC_SCANCODE_KP_8          = 328,
    RC_SCANCODE_KP_9          = 329,
    RC_SCANCODE_KP_DECIMAL    = 330,
    RC_SCANCODE_KP_DIVIDE     = 331,
    RC_SCANCODE_KP_MULTIPLY   = 332,
    RC_SCANCODE_KP_SUBTRACT   = 333,
    RC_SCANCODE_KP_ADD        = 334,
    RC_SCANCODE_KP_ENTER      = 335,
    RC_SCANCODE_KP_EQUAL      = 336,

    /* modifier keys */
    RC_SCANCODE_LEFT_SHIFT    = 340,
    RC_SCANCODE_LEFT_CTRL     = 341,
    RC_SCANCODE_LEFT_ALT      = 342,
    RC_SCANCODE_LEFT_SUPER    = 343,
    RC_SCANCODE_RIGHT_SHIFT   = 344,
    RC_SCANCODE_RIGHT_CTRL    = 345,
    RC_SCANCODE_RIGHT_ALT     = 346,
    RC_SCANCODE_RIGHT_SUPER   = 347,
    RC_SCANCODE_MENU          = 348,

    RC_SCANCODE_UNKNOWN       = -1
} rc_scancode;

#endif /* RC_APP_KEYS_H_ */
