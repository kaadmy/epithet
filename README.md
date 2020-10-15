
> A minimal terminal control library.

# Compatibility

Currently no compatibility testing has been done, and Epithet has only been tested within a Linux TTY, and a terminal emulator running in X (Alacritty.)

# Compiling

Simply include the file `epithet.h` as needed, and define `EP_IMPLEMENTATION` at one place in your source tree.

```c
#define EP_IMPLEMENTATION
#include "epithet.h"
```

# Usage

See file `example.c` for a working example of basic API usage.


# License

Unmodified MIT license.

***

Copyright © 2020 kaadmy

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the “Software”), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED “AS IS”, WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
