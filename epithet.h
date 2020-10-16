
// ============================================================
//
// Epithet, a minimal terminal control library.
//
// (epithet.h)
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

#if !defined(EP_H_)
#define EP_H_

#include <signal.h> // `signal`.
#include <stdarg.h> // `va_*`.
#include <stdbool.h> // `bool`, `true`, `false`.
#include <stddef.h> // `NULL`.
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

typedef enum EpFlagBits {
  // Disable key echo.
  EP_NO_ECHO_BIT = 1,

  // Don't buffer by newlines.
  EP_NO_BUFFER_BIT = 2,

  // Don't catch signals (Ctrl-C, etc.)
  EP_NO_SIGNAL_BIT = 4,

  // Don't block when calling `epGetKey`.
  EP_NO_BLOCK_BIT = 8,

  EP_RAW_BIT = 16,

  EP_FLAGS_DEFAULT = 0,
} EpFlagBits;

typedef uint8_t EpFlags;

// Attributes and styling.

// Color enums are all shifted up by one, so a value of `0` means no
// color.
typedef enum EpColor {
  EP_COLOR_NORMAL = 0,

  EP_COLOR_BLACK = 1,
  EP_COLOR_RED = 2,
  EP_COLOR_GREEN = 3,
  EP_COLOR_YELLOW = 4,
  EP_COLOR_BLUE = 5,
  EP_COLOR_MAGENTA = 6,
  EP_COLOR_CYAN = 7,
  EP_COLOR_WHITE = 8,

  EP_COLOR_MAX_ENUM,

  EP_COLOR_BRIGHT_BLACK = 11,
  EP_COLOR_BRIGHT_RED = 12,
  EP_COLOR_BRIGHT_GREEN = 13,
  EP_COLOR_BRIGHT_YELLOW = 14,
  EP_COLOR_BRIGHT_BLUE = 15,
  EP_COLOR_BRIGHT_MAGENTA = 16,
  EP_COLOR_BRIGHT_CYAN = 17,
  EP_COLOR_BRIGHT_WHITE = 18,
} EpColor;

typedef enum EpStyle {
  EP_STYLE_NONE = 0,

  EP_STYLE_BOLD = 1,
  EP_STYLE_ITALIC = 2,
  EP_STYLE_UNDERLINE = 4,
  EP_STYLE_INVERT = 8,
  EP_STYLE_INVISIBLE = 16,
  EP_STYLE_STRIKETHROUGH = 32,
} EpStyle;

typedef struct EpAttr {
  EpStyle style;
  EpColor fg;
  EpColor bg;
} EpAttr;

#if defined(EP_IMPLEMENTATION)
const EpAttr EP_ATTR_NONE = {
  .style = EP_STYLE_NONE,
  .fg = EP_COLOR_NORMAL,
  .bg = EP_COLOR_NORMAL,
};
#else
extern const EpAttr EP_ATTR_NONE;
#endif

#define EP_ATTR(style_, fg_, bg_) ((const EpAttr) { .style = style_, .fg = fg_, .bg = bg_, })

// ========================================
//
// Key enum.

typedef enum EpKey {
  EP_KEY_NONE = 0,

  EP_KEY_CUSTOM = 2097152, // 1<<21 to avoid potential UTF-8 overlaps.

  EP_KEY_RESIZE = EP_KEY_CUSTOM,
} EpKey;

// ========================================
//
// Functions.

// Global state.

void epInit(void);
void epDeinit(void);

void epSetFlags(EpFlags flags);

// Input handling.

void epSetKey(uint32_t key);
uint32_t epGetKey(void);

// Window handling.

void epGetSize(uint32_t *w, uint32_t *h);

void epFlush(void);
void epClear(void);

// Cursor handling.

void epSetCursorVisibility(bool visible);
void epSetCursorPos(uint32_t x, uint32_t y);

// Viewport.

void epSetViewport(uint32_t x, uint32_t y, uint32_t w, uint32_t h, bool clip);

bool epPosInViewportX(uint32_t x);
bool epPosInViewportY(uint32_t y);
bool epPosInViewportXY(uint32_t x, uint32_t y);

// Drawing.

void epDrawChar(uint32_t x, uint32_t y, EpAttr attr, char c);

void epDrawString(uint32_t x, uint32_t y, EpAttr attr, const char *str, size_t str_len);
void epDrawStringf(uint32_t x, uint32_t y, EpAttr attr, const char *fmt, ...);

void epDrawHline(uint32_t x, uint32_t y, uint32_t len, EpAttr attr, char c);
void epDrawVline(uint32_t x, uint32_t y, uint32_t len, EpAttr attr, char c);

void epDrawRect(uint32_t x, uint32_t y, uint32_t w, uint32_t h, EpAttr attr, char c);

// ========================================
//
// Internal implementation stuff.

#if defined(EP_IMPLEMENTATION)

#define EP_ESC_ "\x1b["
#define EP_ESC_CHAR0_ 0x1b
//#define EP_ESC_CHAR0_ '?'
#define EP_ESC_CHAR1_ '['

#define EP_WRITE_BUFFER_SIZE_ 4096
// Enough of a margin so that escape sequences don't get cut off or
// corrputed.
#define EP_WRITE_BUFFER_FLUSH_SIZE_ (EP_WRITE_BUFFER_SIZE_ - 64)

static struct {
  struct termios original_termios;
  struct termios current_termios;

  int stdin_fileno;
  int stdout_fileno;

  // Terminal flags.
  EpFlags flags;

  // If the terminal size has changed since the last resize event.
  volatile sig_atomic_t resized;
  uint32_t size_w;
  uint32_t size_h;

  // Output buffering.
  char write_buffer[EP_WRITE_BUFFER_SIZE_];
  size_t write_buffer_size;

  // Input state.
  uint32_t next_key;

  // Cursor coordinates are relative to viewport.
  uint32_t cursor_x;
  uint32_t cursor_y;

  // Viewport.
  uint32_t viewport_x;
  uint32_t viewport_y;
  uint32_t viewport_w;
  uint32_t viewport_h;
  bool viewport_clip;
} epState_ = {
  .stdin_fileno = 0,
  .stdout_fileno = 1,

  .resized = false,
};

// Internal output handling.

static inline void epFlush_(bool force) {
  if(force || epState_.write_buffer_size >= EP_WRITE_BUFFER_FLUSH_SIZE_) {
    write(epState_.stdout_fileno, epState_.write_buffer, epState_.write_buffer_size);
    epState_.write_buffer_size = 0;
  }
}

static inline char epLastChar_(void) {
  if(epState_.write_buffer_size > 0) {
    return epState_.write_buffer[epState_.write_buffer_size - 1];
  }
  return '0';
}

static inline void epUnwriteChar_(void) {
  if(epState_.write_buffer_size > 0) {
    epState_.write_buffer_size--;
  }
}

static inline void epWriteChar_(char c) {
  epState_.write_buffer[epState_.write_buffer_size++] = c;
}

static inline void epWriteNum_(uint32_t num) {
  if(num > 999) {
    epWriteChar_('0' + (num / 1000));
    epWriteChar_('0' + ((num % 1000) / 100));
    epWriteChar_('0' + ((num % 100) / 10));
  } else if(num > 99) {
    epWriteChar_('0' + (num / 100));
    epWriteChar_('0' + ((num % 100) / 10));
  } else if(num > 9) {
    epWriteChar_('0' + (num / 10));
  }
  epWriteChar_('0' + (num % 10));
  epWriteChar_(';');
}

static inline void epWriteAttr_(EpAttr attr) {
  epWriteChar_(EP_ESC_CHAR0_);
  epWriteChar_(EP_ESC_CHAR1_);

  if(attr.style == EP_STYLE_NONE) {
    epWriteNum_(0);
  } else {
    if(attr.style & EP_STYLE_BOLD) {
      epWriteNum_(1);
    }
    if(attr.style & EP_STYLE_ITALIC) {
      epWriteNum_(3);
    }
    if(attr.style & EP_STYLE_UNDERLINE) {
      epWriteNum_(4);
    }
    if(attr.style & EP_STYLE_INVERT) {
      epWriteNum_(7);
    }
    if(attr.style & EP_STYLE_INVISIBLE) {
      epWriteNum_(8);
    }
    if(attr.style & EP_STYLE_STRIKETHROUGH) {
      epWriteNum_(9);
    }
  }

  if(attr.fg > 10) {
    epWriteNum_(90 + (attr.fg - 11));
  } else if(attr.fg > 0) {
    epWriteNum_(30 + (attr.fg - 1));
  }

  if(attr.bg > 10) {
    epWriteNum_(attr.bg - 11);
  } else if(attr.bg > 0) {
    epWriteNum_(40 + (attr.bg - 1));
  }

  if(epLastChar_() == ';') {
    epUnwriteChar_();
  }
  epWriteChar_('m');
}

// Global state.

static void epSignal_(int sig) {
  if(sig == SIGWINCH) {
    epState_.resized = true;
  }
}

void epInit(void) {
  // SIGWINCH handler.
  {
    struct sigaction act = {
      .sa_handler = epSignal_,
    };

    if(sigaction(SIGWINCH, &act, NULL)) {
      perror("Failed to set SIGWINCH handler");
    }
  }

  // Get initial terminal state.
  if(tcgetattr(epState_.stdin_fileno, &epState_.original_termios)) {
    perror("Failed to get terminal state");
  }

  memcpy(&epState_.current_termios, &epState_.original_termios, sizeof(struct termios));

  // Set initial flags.
  epSetFlags(EP_FLAGS_DEFAULT);
}

void epDeinit(void) {
  epWriteChar_(EP_ESC_CHAR0_);
  epWriteChar_(EP_ESC_CHAR1_);
  epWriteChar_('m');
  epWriteChar_('\n');
  epFlush_(true);

  if(tcsetattr(epState_.stdin_fileno, TCSAFLUSH, &epState_.original_termios)) {
    perror("Failed to reset terminal state");
  }
}

void epSetFlags(EpFlags flags) {
  epState_.flags = flags;

  epState_.current_termios.c_cflag |= CS8;

  if(flags & EP_RAW_BIT) {
    epState_.current_termios.c_iflag &= ~(IGNBRK | IEXTEN | ISTRIP);
    epState_.current_termios.c_oflag &= ~OPOST;
    epState_.current_termios.c_cflag &= ~CSIZE;
  } else {
    epState_.current_termios.c_iflag |= IGNBRK | IEXTEN | ISTRIP;
    epState_.current_termios.c_oflag |= OPOST;
    epState_.current_termios.c_cflag |= CSIZE;
  }

  if((flags & EP_NO_ECHO_BIT) || (flags & EP_RAW_BIT)) {
    epState_.current_termios.c_lflag &= ~ECHO;
  } else {
    epState_.current_termios.c_lflag |= ECHO;
  }

  if((flags & EP_NO_BUFFER_BIT) || (flags & EP_RAW_BIT)) {
    epState_.current_termios.c_iflag |= BRKINT | IXON;
    epState_.current_termios.c_lflag &= ~ICANON;
  } else {
    epState_.current_termios.c_iflag &= ~(BRKINT | IXON);
    epState_.current_termios.c_lflag |= ICANON;
  }

  if((flags & EP_NO_SIGNAL_BIT) || (flags & EP_RAW_BIT)) {
    epState_.current_termios.c_lflag &= ~ISIG;
  } else {
    epState_.current_termios.c_lflag |= ISIG;
  }

  epState_.current_termios.c_cc[VMIN] = 0;
  epState_.current_termios.c_cc[VTIME] = 0;

  if(tcsetattr(epState_.stdin_fileno, TCSAFLUSH, &epState_.current_termios)) {
    perror("Failed to set new terminal state");
  }
}

// Input handling.

void epSetKey(uint32_t key) {
  epState_.next_key = key;
}

uint32_t epGetKey(void) {
  if(epState_.next_key) {
    uint32_t k = epState_.next_key;
    epState_.next_key = 0;
    return k;
  }

  char keybuf[16];
  int keybuf_size;

  uint32_t key = 0;

  while(true) {
    // Check if a resize has occured since the last call to this
    // function.
    if(epState_.resized) {
      epGetSize(&epState_.size_w, &epState_.size_h);
      epState_.resized = false;
      return EP_KEY_RESIZE;
    }

    // Check for key events.
    keybuf_size = read(epState_.stdin_fileno, &keybuf, sizeof(keybuf));

    // If non-blocking mode is enabled, return immediately.
    if(epState_.flags & EP_NO_BLOCK_BIT) {
      return EP_KEY_NONE;
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

void epGetSize(uint32_t *w, uint32_t *h) {
  struct winsize ws;
  if(ioctl(epState_.stdin_fileno, TIOCGWINSZ, &ws)) {
    perror("Failed to get window size");
  }

  epState_.size_w = ws.ws_col;
  epState_.size_h = ws.ws_row;

  *w = ws.ws_col;
  *h = ws.ws_row;
}

void epFlush(void) {
  epFlush_(true);
}

void epClear(void) {
  epWriteChar_(EP_ESC_CHAR0_);
  epWriteChar_(EP_ESC_CHAR1_);
  epWriteChar_('m');
  epWriteChar_(EP_ESC_CHAR0_);
  epWriteChar_(EP_ESC_CHAR1_);
  epWriteChar_('H');
  epWriteChar_(EP_ESC_CHAR0_);
  epWriteChar_(EP_ESC_CHAR1_);
  epWriteChar_('2');
  epWriteChar_('J');
  epFlush_(true);
}

// Cursor handling.

void epSetCursorVisibility(bool visible) {
  epWriteChar_(EP_ESC_CHAR0_);
  epWriteChar_(EP_ESC_CHAR1_);
  epWriteChar_('?');
  epWriteChar_('2');
  epWriteChar_('5');
  epWriteChar_((visible ? 'h' : 'l'));
}

void epSetCursorPos(uint32_t x, uint32_t y) {
  epState_.cursor_x = x;
  epState_.cursor_y = y;

  // Clamp cursor to viewport.
  if(epState_.viewport_clip) {
    if(epState_.cursor_x >= epState_.viewport_w) {
      epState_.cursor_x = epState_.viewport_w - 1;
    }
    if(epState_.cursor_y >= epState_.viewport_h) {
      epState_.cursor_y = epState_.viewport_h - 1;
    }
  }

  // Write escape sequence to actually move the cursor.
  epWriteChar_(EP_ESC_CHAR0_);
  epWriteChar_(EP_ESC_CHAR1_);

  epWriteNum_(epState_.viewport_y + epState_.cursor_y + 1);
  epWriteNum_(epState_.viewport_x + epState_.cursor_x + 1);

  if(epLastChar_() == ';') {
    epUnwriteChar_();
  }
  epWriteChar_('H');

  epFlush_(false);
}

// Viewport.

// When the terminal is resized, the viewport will *not* be
// automatically adjusted, and it is up to the caller to update the
// relevant viewport regions. The resulting behaviour will be
// undefined.
void epSetViewport(uint32_t x, uint32_t y, uint32_t w, uint32_t h, bool clip) {
  epState_.viewport_clip = clip;

  uint32_t sx, sy;
  epGetSize(&sx, &sy);

  // Offscreen in either axis.
  if(x >= sx || y >= sy) {
    epState_.viewport_x = 0;
    epState_.viewport_y = 0;
    epState_.viewport_w = 0;
    epState_.viewport_h = 0;
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

  epState_.viewport_x = x;
  epState_.viewport_y = y;
  epState_.viewport_w = w;
  epState_.viewport_h = h;
}

bool epPosInViewportX(uint32_t x) {
  return (epState_.viewport_clip ? x < epState_.viewport_w : true);
}

bool epPosInViewportY(uint32_t y) {
  return (epState_.viewport_clip ? y < epState_.viewport_h : true);
}

bool epPosInViewportXY(uint32_t x, uint32_t y) {
  return epPosInViewportX(x) && epPosInViewportY(y);
}

// Drawing.

void epDrawChar(uint32_t x, uint32_t y, EpAttr attr, char c) {
  if(!epPosInViewportXY(x, y)) {
    return;
  }

  epWriteAttr_(attr);
  epSetCursorPos(x, y);
  epWriteChar_(c);
  epFlush_(false);
}

void epDrawString(uint32_t x, uint32_t y, EpAttr attr, const char *str, size_t str_len) {
  str_len = (str_len?str_len:strlen(str));

  if(str_len < 1) {
    return;
  }

  if(!epPosInViewportXY(x, y)) {
    return;
  }

  epWriteAttr_(attr);
  epSetCursorPos(x, y);
  for(size_t i = 0; i < str_len; i++) {
    if(!epPosInViewportX(x + i)) {
      return;
    }
    epWriteChar_(str[i]);
  }
  epFlush_(false);
}

void epDrawStringf(uint32_t x, uint32_t y, EpAttr attr, const char *fmt, ...) {
  if(!epPosInViewportXY(x, y)) {
    return;
  }

  static char str[512];
  va_list arg;
  va_start(arg, fmt);
  size_t str_len = vsnprintf(str, sizeof(str), fmt, arg);
  va_end(arg);

  if(str_len > 0) {
    epDrawString(x, y, attr, str, str_len);
  }
}

void epDrawHline(uint32_t x, uint32_t y, uint32_t len, EpAttr attr, char c) {
  if(!epPosInViewportXY(x, y)) {
    return;
  }

  epWriteAttr_(attr);
  epSetCursorPos(x, y);
  for(uint32_t i = 0; i < len; i++) {
    if(!epPosInViewportX(x + i)) {
      return;
    }
    epWriteChar_(c);
  }
  epFlush_(false);
}

void epDrawVline(uint32_t x, uint32_t y, uint32_t len, EpAttr attr, char c) {
  epWriteAttr_(attr);
  for(uint32_t i = 0; i < len; i++) {
    epSetCursorPos(x, y + i);
    epWriteChar_(c);
  }
  epFlush_(false);
}

void epDrawRect(uint32_t x, uint32_t y, uint32_t w, uint32_t h, EpAttr attr, char c) {
  if(!epPosInViewportXY(x, y)) {
    return;
  }

  epWriteAttr_(attr);
  for(uint32_t i = 0; i < h; i++) {
    if(!epPosInViewportY(y + i)) {
      break;
    }
    epSetCursorPos(x, y + i);
    for(uint32_t j = 0; j < w; j++) {
      if(!epPosInViewportX(x + j)) {
        break;
      }
      epWriteChar_(c);
    }
  }
  epFlush_(false);
}

#endif // #if defined(EP_IMPLEMENTATION)

#endif // #if !defined(EP_H_)
