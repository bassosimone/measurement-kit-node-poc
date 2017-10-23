# measurement-kit-node proof of concept

Based off [nodejs/Nan](https://github.com/nodejs/nan) examples.

## Build & Test

To build:

- install measurement-kit

- `npm install`

To rebuild:

- `node-gyp rebuild`

To test:

- `node ./addon.js`

## Discussion

Studying Node bindings I basically discovered that it's bad to call v8 code
from a background thread, unless we're using NaN code specifically written to
support this use case. The main problem with the current bindings seems to
be that we create two threads for each test. Perhaps, since we're moving MK
to use one thread per test, we can have a running mode in which we do not
create any background thread for running the test. This would reduce the
number of threads per test to one, like we are doing on iOS. (On Android we
use two threads like we're using here.)

Apart from the code issues that I needed to deal with, I also needed to make
sure that the compiler was using RTTI and exceptions and C++14. See the
`binding.gyp` to see the weird way in which this is done.
