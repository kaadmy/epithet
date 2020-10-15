
// ============================================================
//
// Sweer, a minimal terminal control library.
//
// (sweer.h)
//
// ============================================================

/*
  Copyright © 2020 kaadmy

  Permission is hereby granted, free of charge, to any person
  obtaining a copy of this software and associated documentation files
  (the “Software”), to deal in the Software without restriction,
  including without limitation the rights to use, copy, modify, merge,
  publish, distribute, sublicense, and/or sell copies of the Software,
  and to permit persons to whom the Software is furnished to do so,
  subject to the following conditions:

  The above copyright notice and this permission notice shall be
  included in all copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED “AS IS”, WITHOUT WARRANTY OF ANY KIND,
  EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
  MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
  NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
  BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
  ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
  CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
  SOFTWARE.
*/

#if !defined(SWEER_H_)
#define SWEER_H_

#include <signal.h> // `signal`.
#include <stdarg.h> // `va_*`.
#include <stdbool.h> // `NULL`.
//#include <stddef.h>
#include <stdint.h> // `int*_t`, `uint*_t`.
#include <stdio.h> // `perror`.
#include <string.h> // `strlen`, `memcpy`.
#include <sys/ioctl.h> // `ioctl`.
#include <time.h> // `struct timespec`, `nanosleep`.
#include <termios.h> // `struct termios`, `tcgetattr`, `tcsetattr`.
#include <unistd.h> // `read`.

// ========================================
//
// Types.

// Terminal flags.

typedef enum SweerFlagBits {
  SWEER_NO_ECHO_BIT = 1,
  SWEER_CBREAK_BIT = 2,
  SWEER_NO_SIGNAL_BIT = 4,
  SWEER_NO_BLOCK_BIT = 8,
  SWEER_RAW_BIT = 16,

  SWEER_FLAGS_DEFAULT = 0,
} SweerFlagBits;

typedef uint8_t SweerFlags;

// Attributes and styling.

// Color enums are all shifted up by one, so a value of `0` means no
// color.
typedef enum SweerColor {
  SWEER_COLOR_NONE = 0,

  SWEER_COLOR_BLACK = 1,
  SWEER_COLOR_RED = 2,
  SWEER_COLOR_GREEN = 3,
  SWEER_COLOR_YELLOW = 4,
  SWEER_COLOR_BLUE = 5,
  SWEER_COLOR_MAGENTA = 6,
  SWEER_COLOR_CYAN = 7,
  SWEER_COLOR_WHITE = 8,

  SWEER_COLOR_MAX_ENUM,

  SWEER_COLOR_BRIGHT_BLACK = 11,
  SWEER_COLOR_BRIGHT_RED = 12,
  SWEER_COLOR_BRIGHT_GREEN = 13,
  SWEER_COLOR_BRIGHT_YELLOW = 14,
  SWEER_COLOR_BRIGHT_BLUE = 15,
  SWEER_COLOR_BRIGHT_MAGENTA = 16,
  SWEER_COLOR_BRIGHT_CYAN = 17,
  SWEER_COLOR_BRIGHT_WHITE = 18,
} SweerColor;

typedef enum SweerStyle {
  SWEER_STYLE_NONE = 0,
  SWEER_STYLE_BOLD = 1,
  SWEER_STYLE_ITALIC = 2,
  SWEER_STYLE_UNDERLINE = 4,
  SWEER_STYLE_INVERT = 8,
  SWEER_STYLE_INVISIBLE = 16,
  SWEER_STYLE_STRIKETHROUGH = 32,
} SweerStyle;

typedef struct SweerAttr {
  SweerStyle style;
  SweerColor fg;
  SweerColor bg;
} SweerAttr;

#if defined(SWEER_IMPLEMENTATION)
const SweerAttr SWEER_ATTR_NONE = {
  .style = SWEER_STYLE_NONE,
  .fg = SWEER_COLOR_NONE,
  .bg = SWEER_COLOR_NONE,
};
#else
extern const SweerAttr SWEER_ATTR_NONE;
#endif

#define SWEER_ATTR(style_, fg_, bg_) ((const SweerAttr) { .style = style_, .fg = fg_, .bg = bg_, })

// ========================================
//
// Key enum.

typedef enum SweerKey {
  SWEER_KEY_NONE = 0,

  SWEER_KEY_CUSTOM = 2097152, // 1<<21 to avoid potential UTF-8 overlaps.

  SWEER_KEY_RESIZE = SWEER_KEY_CUSTOM,
} SweerKey;

// ========================================
//
// Functions.

// Global state.

void sweerInit(void);
void sweerDeinit(void);

void sweerSetFlags(SweerFlags flags);

// Input handling.

uint32_t sweerGetKey(void);

// Window handling.

void sweerGetSize(uint32_t *w, uint32_t *h);

void sweerFlush(void);
void sweerClear(void);

// Cursor handling.

void sweerSetCursorVisibility(bool visible);
void sweerSetCursorPos(uint32_t x, uint32_t y);

// Viewport.

void sweerSetViewport(uint32_t x, uint32_t y, uint32_t w, uint32_t h, bool clip);

bool sweerPosInViewportX(uint32_t x);
bool sweerPosInViewportY(uint32_t y);
bool sweerPosInViewportXY(uint32_t x, uint32_t y);

// Drawing.

void sweerDrawChar(uint32_t x, uint32_t y, SweerAttr attr, char c);

void sweerDrawString(uint32_t x, uint32_t y, SweerAttr attr, const char *str, size_t str_len);
void sweerDrawStringf(uint32_t x, uint32_t y, SweerAttr attr, const char *fmt, ...);

void sweerDrawHline(uint32_t x, uint32_t y, uint32_t len, SweerAttr attr, char c);
void sweerDrawVline(uint32_t x, uint32_t y, uint32_t len, SweerAttr attr, char c);

void sweerDrawRect(uint32_t x, uint32_t y, uint32_t w, uint32_t h, SweerAttr attr, char c);

// ========================================
//
// Internal implementation stuff.

#if defined(SWEER_IMPLEMENTATION)

#define SWEER_ESC_ "\x1b["
#define SWEER_ESC_CHAR0_ 0x1b
//#define SWEER_ESC_CHAR0_ '?'
#define SWEER_ESC_CHAR1_ '['

#define SWEER_WRITE_BUFFER_SIZE_ 4096
// Enough of a margin so that escape sequences don't get cut off or
// corrputed.
#define SWEER_WRITE_BUFFER_FLUSH_SIZE_ (SWEER_WRITE_BUFFER_SIZE_ - 64)

static struct {
  struct termios original_termios;
  struct termios current_termios;

  int stdin_fileno;
  int stdout_fileno;

  // Terminal flags.
  SweerFlags flags;

  // If the terminal size has changed since the last resize event.
  volatile sig_atomic_t resized;
  uint32_t size_w;
  uint32_t size_h;

  // Output buffering.
  char write_buffer[SWEER_WRITE_BUFFER_SIZE_];
  size_t write_buffer_size;

  // Cursor coordinates are relative to viewport.
  uint32_t cursor_x;
  uint32_t cursor_y;

  // Viewport.
  uint32_t viewport_x;
  uint32_t viewport_y;
  uint32_t viewport_w;
  uint32_t viewport_h;
  bool viewport_clip;
} sweerState_ = {
  .stdin_fileno = 0,
  .stdout_fileno = 1,

  .resized = false,
};

// Internal output handling.

static inline void sweerFlush_(bool force) {
  if(force || sweerState_.write_buffer_size >= SWEER_WRITE_BUFFER_FLUSH_SIZE_) {
    write(sweerState_.stdout_fileno, sweerState_.write_buffer, sweerState_.write_buffer_size);
    sweerState_.write_buffer_size = 0;
  }
}

static inline char sweerLastChar_(void) {
  if(sweerState_.write_buffer_size > 0) {
    return sweerState_.write_buffer[sweerState_.write_buffer_size - 1];
  }
  return '0';
}

static inline void sweerUnwriteChar_(void) {
  if(sweerState_.write_buffer_size > 0) {
    sweerState_.write_buffer_size--;
  }
}

static inline void sweerWriteChar_(char c) {
  sweerState_.write_buffer[sweerState_.write_buffer_size++] = c;
}

static inline void sweerWriteNum_(uint32_t num) {
  if(num > 999) {
    sweerWriteChar_('0' + (num / 1000));
    sweerWriteChar_('0' + ((num % 1000) / 100));
    sweerWriteChar_('0' + ((num % 100) / 10));
  } else if(num > 99) {
    sweerWriteChar_('0' + (num / 100));
    sweerWriteChar_('0' + ((num % 100) / 10));
  } else if(num > 9) {
    sweerWriteChar_('0' + (num / 10));
  }
  sweerWriteChar_('0' + (num % 10));
  sweerWriteChar_(';');
}

static inline void sweerWriteAttr_(SweerAttr attr) {
  sweerWriteChar_(SWEER_ESC_CHAR0_);
  sweerWriteChar_(SWEER_ESC_CHAR1_);

  if(attr.style == SWEER_STYLE_NONE) {
    sweerWriteNum_(0);
  } else {
    if(attr.style & SWEER_STYLE_BOLD) {
      sweerWriteNum_(1);
    }
    if(attr.style & SWEER_STYLE_ITALIC) {
      sweerWriteNum_(3);
    }
    if(attr.style & SWEER_STYLE_UNDERLINE) {
      sweerWriteNum_(4);
    }
    if(attr.style & SWEER_STYLE_INVERT) {
      sweerWriteNum_(7);
    }
    if(attr.style & SWEER_STYLE_INVISIBLE) {
      sweerWriteNum_(8);
    }
    if(attr.style & SWEER_STYLE_STRIKETHROUGH) {
      sweerWriteNum_(9);
    }
  }

  if(attr.fg > 10) {
    sweerWriteNum_(90 + (attr.fg - 11));
  } else if(attr.fg > 0) {
    sweerWriteNum_(30 + (attr.fg - 1));
  }

  if(attr.bg > 10) {
    sweerWriteNum_(attr.bg - 11);
  } else if(attr.bg > 0) {
    sweerWriteNum_(40 + (attr.bg - 1));
  }

  if(sweerLastChar_() == ';') {
    sweerUnwriteChar_();
  }
  sweerWriteChar_('m');
}

// Global state.

static void sweerSignal_(int sig) {
  if(sig == SIGWINCH) {
    sweerState_.resized = true;
  }
}

void sweerInit(void) {
  // SIGWINCH handler.
  {
    struct sigaction act = {
      .sa_handler = sweerSignal_,
    };

    if(sigaction(SIGWINCH, &act, NULL)) {
      perror("Failed to set SIGWINCH handler");
    }
  }

  // Get initial terminal state.
  if(tcgetattr(sweerState_.stdin_fileno, &sweerState_.original_termios)) {
    perror("Failed to get terminal state");
  }

  memcpy(&sweerState_.current_termios, &sweerState_.original_termios, sizeof(struct termios));

  // Set initial flags.
  sweerSetFlags(SWEER_FLAGS_DEFAULT);
}

void sweerDeinit(void) {
  sweerWriteChar_(SWEER_ESC_CHAR0_);
  sweerWriteChar_(SWEER_ESC_CHAR1_);
  sweerWriteChar_('m');
  sweerWriteChar_('\n');
  sweerFlush_(true);

  if(tcsetattr(sweerState_.stdin_fileno, TCSAFLUSH, &sweerState_.original_termios)) {
    perror("Failed to reset terminal state");
  }
}

void sweerSetFlags(SweerFlags flags) {
  sweerState_.flags = flags;

  sweerState_.current_termios.c_cflag |= CS8;

  if(flags & SWEER_RAW_BIT) {
    sweerState_.current_termios.c_iflag &= ~(IGNBRK | IEXTEN | ISTRIP);
    sweerState_.current_termios.c_oflag &= ~OPOST;
    sweerState_.current_termios.c_cflag &= ~CSIZE;
  } else {
    sweerState_.current_termios.c_iflag |= IGNBRK | IEXTEN | ISTRIP;
    sweerState_.current_termios.c_oflag |= OPOST;
    sweerState_.current_termios.c_cflag |= CSIZE;
  }

  if((flags & SWEER_NO_ECHO_BIT) || (flags & SWEER_RAW_BIT)) {
    sweerState_.current_termios.c_lflag &= ~ECHO;
  } else {
    sweerState_.current_termios.c_lflag |= ECHO;
  }

  if((flags & SWEER_CBREAK_BIT) || (flags & SWEER_RAW_BIT)) {
    sweerState_.current_termios.c_iflag |= BRKINT | IXON;
    sweerState_.current_termios.c_lflag &= ~ICANON;
  } else {
    sweerState_.current_termios.c_iflag &= ~(BRKINT | IXON);
    sweerState_.current_termios.c_lflag |= ICANON;
  }

  if((flags & SWEER_NO_SIGNAL_BIT) || (flags & SWEER_RAW_BIT)) {
    sweerState_.current_termios.c_lflag &= ~ISIG;
  } else {
    sweerState_.current_termios.c_lflag |= ISIG;
  }

  sweerState_.current_termios.c_cc[VMIN] = 0;
  sweerState_.current_termios.c_cc[VTIME] = 0;

  if(tcsetattr(sweerState_.stdin_fileno, TCSAFLUSH, &sweerState_.current_termios)) {
    perror("Failed to set new terminal state");
  }
}

// Input handling.

uint32_t sweerGetKey(void) {
  char keybuf[16];
  int keybuf_size;

  uint32_t key = 0;

  while(true) {
    // Check if a resize has occured since the last call to this
    // function.
    if(sweerState_.resized) {
      sweerGetSize(&sweerState_.size_w, &sweerState_.size_h);
      sweerState_.resized = false;
      return SWEER_KEY_RESIZE;
    }

    // Check for key events.
    keybuf_size = read(sweerState_.stdin_fileno, &keybuf, sizeof(keybuf));

    // If non-blocking mode is enabled, return immediately.
    if(sweerState_.flags & SWEER_NO_BLOCK_BIT) {
      return SWEER_KEY_NONE;
    }

    // If we get a key event, break out of the loop.
    if(keybuf_size > 0) {
      break;
    }

    // Sleep before the next loop to prevent excessive CPU
    // utilization.
    {
      struct timespec ts = {
        .tv_sec = 0,
        .tv_nsec = ((uint64_t) 1e9) / 1000, // 1 millisecond.
      };

      nanosleep(&ts, NULL);
    }
  }

  // Single byte key codes.
  if(keybuf_size == 1) {
    return keybuf[0];
  }

  // Multibyte input sequences.
  for(int i = 0; i < keybuf_size; i++) {
    //printf("kb:%d\n", keybuf[i]);
  }

  return 0;
}

// Window handling.

void sweerGetSize(uint32_t *w, uint32_t *h) {
  struct winsize ws;
  if(ioctl(sweerState_.stdin_fileno, TIOCGWINSZ, &ws)) {
    perror("Failed to get window size");
  }

  sweerState_.size_w = ws.ws_col;
  sweerState_.size_h = ws.ws_row;

  *w = ws.ws_col;
  *h = ws.ws_row;
}

void sweerFlush(void) {
  sweerFlush_(true);
}

void sweerClear(void) {
  sweerWriteChar_(SWEER_ESC_CHAR0_);
  sweerWriteChar_(SWEER_ESC_CHAR1_);
  sweerWriteChar_('m');
  sweerWriteChar_(SWEER_ESC_CHAR0_);
  sweerWriteChar_(SWEER_ESC_CHAR1_);
  sweerWriteChar_('H');
  sweerWriteChar_(SWEER_ESC_CHAR0_);
  sweerWriteChar_(SWEER_ESC_CHAR1_);
  sweerWriteChar_('2');
  sweerWriteChar_('J');
  sweerFlush_(true);
}

// Cursor handling.

void sweerSetCursorVisibility(bool visible) {
  sweerWriteChar_(SWEER_ESC_CHAR0_);
  sweerWriteChar_(SWEER_ESC_CHAR1_);
  sweerWriteChar_('?');
  sweerWriteChar_('2');
  sweerWriteChar_('5');
  sweerWriteChar_((visible ? 'h' : 'l'));
}

void sweerSetCursorPos(uint32_t x, uint32_t y) {
  sweerState_.cursor_x = x;
  sweerState_.cursor_y = y;

  // Clamp cursor to viewport.
  if(sweerState_.viewport_clip) {
    if(sweerState_.cursor_x >= sweerState_.viewport_w) {
      sweerState_.cursor_x = sweerState_.viewport_w - 1;
    }
    if(sweerState_.cursor_y >= sweerState_.viewport_h) {
      sweerState_.cursor_y = sweerState_.viewport_h - 1;
    }
  }

  // Write escape sequence to actually move the cursor.
  sweerWriteChar_(SWEER_ESC_CHAR0_);
  sweerWriteChar_(SWEER_ESC_CHAR1_);

  sweerWriteNum_(sweerState_.viewport_y + sweerState_.cursor_y + 1);
  sweerWriteNum_(sweerState_.viewport_x + sweerState_.cursor_x + 1);

  if(sweerLastChar_() == ';') {
    sweerUnwriteChar_();
  }
  sweerWriteChar_('H');

  sweerFlush_(false);
}

// Viewport.

// When the terminal is resized, the viewport will *not* be
// automatically adjusted, and it is up to the caller to update the
// relevant viewport regions. The resulting behaviour will be
// undefined.
void sweerSetViewport(uint32_t x, uint32_t y, uint32_t w, uint32_t h, bool clip) {
  sweerState_.viewport_clip = clip;

  uint32_t sx, sy;
  sweerGetSize(&sx, &sy);

  // Offscreen in either axis.
  if(x >= sx || y >= sy) {
    sweerState_.viewport_x = 0;
    sweerState_.viewport_y = 0;
    sweerState_.viewport_w = 0;
    sweerState_.viewport_h = 0;
    return;
  }

  // Clip width and height to fit screen.
  int32_t dx = (x + w) - sx;
  if(dx > 0) {
    w -= dx;
  }

  int32_t dy = (y + h) - sy;
  if(dy > 0) {
    h -= dy;
  }

  sweerState_.viewport_x = x;
  sweerState_.viewport_y = y;
  sweerState_.viewport_w = w;
  sweerState_.viewport_h = h;
}

bool sweerPosInViewportX(uint32_t x) {
  return (sweerState_.viewport_clip ? x < sweerState_.viewport_w : true);
}

bool sweerPosInViewportY(uint32_t y) {
  return (sweerState_.viewport_clip ? y < sweerState_.viewport_h : true);
}

bool sweerPosInViewportXY(uint32_t x, uint32_t y) {
  return sweerPosInViewportX(x) && sweerPosInViewportY(y);
}

// Drawing.

void sweerDrawChar(uint32_t x, uint32_t y, SweerAttr attr, char c) {
  if(!sweerPosInViewportXY(x, y)) {
    return;
  }

  sweerWriteAttr_(attr);
  sweerSetCursorPos(x, y);
  sweerWriteChar_(c);
  sweerFlush_(false);
}

void sweerDrawString(uint32_t x, uint32_t y, SweerAttr attr, const char *str, size_t str_len) {
  str_len = (str_len?str_len:strlen(str));

  if(str_len < 1) {
    return;
  }

  if(!sweerPosInViewportXY(x, y)) {
    return;
  }

  sweerWriteAttr_(attr);
  sweerSetCursorPos(x, y);
  for(size_t i = 0; i < str_len; i++) {
    if(!sweerPosInViewportX(x + i)) {
      return;
    }
    sweerWriteChar_(str[i]);
  }
  sweerFlush_(false);
}

void sweerDrawStringf(uint32_t x, uint32_t y, SweerAttr attr, const char *fmt, ...) {
  if(!sweerPosInViewportXY(x, y)) {
    return;
  }

  static char str[512];
  va_list arg;
  va_start(arg, fmt);
  size_t str_len = vsnprintf(str, sizeof(str), fmt, arg);
  va_end(arg);

  if(str_len > 0) {
    sweerDrawString(x, y, attr, str, str_len);
  }
}

void sweerDrawHline(uint32_t x, uint32_t y, uint32_t len, SweerAttr attr, char c) {
  if(!sweerPosInViewportXY(x, y)) {
    return;
  }

  sweerWriteAttr_(attr);
  sweerSetCursorPos(x, y);
  for(uint32_t i = 0; i < len; i++) {
    if(!sweerPosInViewportX(x + i)) {
      return;
    }
    sweerWriteChar_(c);
  }
  sweerFlush_(false);
}

void sweerDrawVline(uint32_t x, uint32_t y, uint32_t len, SweerAttr attr, char c) {
  sweerWriteAttr_(attr);
  for(uint32_t i = 0; i < len; i++) {
    sweerSetCursorPos(x, y + i);
    sweerWriteChar_(c);
  }
  sweerFlush_(false);
}

void sweerDrawRect(uint32_t x, uint32_t y, uint32_t w, uint32_t h, SweerAttr attr, char c) {
  if(!sweerPosInViewportXY(x, y)) {
    return;
  }

  sweerWriteAttr_(attr);
  for(uint32_t i = 0; i < h; i++) {
    if(!sweerPosInViewportY(y + i)) {
      break;
    }
    sweerSetCursorPos(x, y + i);
    for(uint32_t j = 0; j < w; j++) {
      if(!sweerPosInViewportX(x + j)) {
        break;
      }
      sweerWriteChar_(c);
    }
  }
  sweerFlush_(false);
}

#endif // #if defined(SWEER_IMPLEMENTATION)

#endif // #if !defined(SWEER_H_)
