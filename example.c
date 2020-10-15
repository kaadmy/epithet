
#define SWEER_IMPLEMENTATION
#include "sweer.h"

void draw(void) {
  sweerClear();

  // The last argument is the length of the string. If it is zero, the length is automatically determined.
  //
  // If it is not zero, the string does not need to have a NUL termination.
  sweerDrawString(0, 0, SWEER_ATTR_NONE, "Press any key to continue", 0);
  sweerFlush();
}

int main(int argc, char **argv) {
  // Initialize the library.
  sweerInit();

  // Don't echo pressed keys, and send key events without buffering and waiting for the enter key.
  sweerSetFlags(SWEER_NO_ECHO_BIT | SWEER_CBREAK_BIT);

  draw();

  // Wait for the user to press any key.
  uint32_t key = SWEER_KEY_NONE;
  while(1) {
    key = sweerGetKey();
    if(key == SWEER_KEY_RESIZE) {
      draw();
      sweerDrawString(0, 1, SWEER_ATTR(SWEER_STYLE_INVERT, SWEER_COLOR_BLUE, 0), "Resized!", 0);
      sweerFlush();
      continue;
    } else {
      break;
    }

    draw();
  }

  // Display the key number in red.
  sweerDrawStringf(0, 2, SWEER_ATTR(0, SWEER_COLOR_RED, 0), "You pressed %u", key);
  sweerFlush();

  // Deinitialize the library.
  //
  // This will reset the terminal as much as possible to prevent attributes, input state, etc. from leaking.
 	sweerDeinit();

  return 0;
}
