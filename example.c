
#define EP_IMPLEMENTATION
#include "epithet.h"

void draw(void) {
  epClear();

  // The last argument is the length of the string. If it is zero, the length is automatically determined.
  //
  // If it is not zero, the string does not need to have a NUL termination.
  epDrawString(0, 0, EP_ATTR_NONE, "Press any key to continue", 0);
  epFlush();
}

int main(int argc, char **argv) {
  // Initialize the library.
  epInit();

  // Don't echo pressed keys, and send key events without buffering and waiting for the enter key.
  epSetFlags(EP_NO_ECHO_BIT | EP_NO_BUFFER_BIT);

  draw();

  // Wait for the user to press any key.
  uint32_t key = EP_KEY_NONE;
  while(1) {
    key = epGetKey();
    if(key == EP_KEY_RESIZE) {
      draw();
      epDrawString(0, 1, EP_ATTR(EP_STYLE_INVERT, EP_COLOR_BLUE, EP_COLOR_NORMAL), "Resized!", 0);
      epFlush();
      continue;
    } else {
      break;
    }

    draw();
  }

  // Display the key number in red.
  epDrawStringf(0, 2, EP_ATTR(EP_STYLE_NONE, EP_COLOR_RED, EP_COLOR_NORMAL), "You pressed %u", key);
  epFlush();

  // Deinitialize the library.
  //
  // This will reset the terminal as much as possible to prevent attributes, input state, etc. from leaking.
 	epDeinit();

  return 0;
}
