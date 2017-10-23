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

## Interaction with event loop explained and future plans

Studying Node bindings I basically discovered that we cannot call v8 APIs
from a background thread controlled by us. But nodejs/Nan features the
`AsyncWorker` family of templates that we can use for this purpose.

With the current implementation, especially after MK v0.8.0, we will create
two threads per nettest. The first thread will be created by Nan off the
libuv poll for executing the `AsyncWorker`. The second thread will be created
off the MK pool by `run()`. It is worth noting that, while the second thread
will be running, the former will be blocked on a condition variable. It is
probably not that bad, if we consider that for sure after MK v0.9.0, we will
force a single test to run at a time.

Therefore, we can keep the two-threads-per-test model for some time (and by
the way, this is something we have also been doing on Android since when the
first public version of the ooniprobe app was released).

Swithing to a model where we use a single thread entails writing more code
and perhaps the starting point will be subclassing and/or rewriting the
`AsyncWorker` family of templates. The key implementation points to keep
in mind in such case would be the following:

1. [According to Nan code, we should use libuv default event loop](
     https://github.com/nodejs/nan/blob/7aa06dbc4e5f402cf8b99c57c235bcd195f5abd3/nan.h#L1617
   ).

2. We don't know whether there are other events keeping alive the event
   loop, so we need to guarantee it will not exit. [With Nan API what
   happens is that AsyncQueueWorker will schedule the AsyncWorker to
   run on a thread in libuv's poll, and that will keep alive the evloop
   until the thread is complete](
     https://github.com/nodejs/nan/blob/7aa06dbc4e5f402cf8b99c57c235bcd195f5abd3/nan.h#L1868
   ). If we want to guarantee that the loop is alive even in the event
   in which there are no other pending events, we can use the following
   strategy to guarantee that:

```C
#include <stdio.h>
#include <stdlib.h>
#include <uv.h>

#define REQUIRE(statement) if (!(statement)) exit(1)

static void progress_cb(uv_async_t *handle) { (void)handle; }

int main() {
  uv_loop_t *loop = uv_default_loop();
  uv_async_t async;
  REQUIRE(uv_async_init(loop, &async, progress_cb) == 0);
  async.data = NULL;
  REQUIRE(uv_run(loop, UV_RUN_DEFAULT) == 0);
}
```

3. [Nan is indeed using uv_async to signal the event loop that it must
    call specific callacks for dealing with progress](
       https://github.com/nodejs/nan/blob/7aa06dbc4e5f402cf8b99c57c235bcd195f5abd3/nan.h#L1616
   ). Yet, the model provided by Nan is a bit unsatisfactory in that
   we cannot wake up the event loop for different kind of events,
   thus we may want to roll out our abstraction.

## Other details to keep in mind

I also needed to make sure that the compiler was using RTTI, exceptions and
C++14. This is not the default for node extensions. See the `binding.gyp` to
see the weird way in which this is done.

Note that no exceptions and no RTTI are the default because both v8 and node are
compiled with no exceptions and no RTTI. This has the implication that we MUST
NOT let exceptions traverse the boundary of MK code, because other C++ code does
not contain RAII cleanup code to deal with stack unwinding, so many bad things
could happen (I think mainly that objects won't be destroyed properly).
