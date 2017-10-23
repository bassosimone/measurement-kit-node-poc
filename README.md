# measurement-kit-node proof of concept

Based off [nodejs/Nan](https://github.com/nodejs/nan) examples.

To build:

- install measurement-kit

- `npm install`

To rebuild:

- `node-gyp rebuild`

To test:

- `node ./addon.js`

Apart from the code issues that I needed to deal with, I also needed to make
sure that the compiler was using RTTI and exceptions and C++14. See the
`binding.gyp` to see the weird way in which this is done.
