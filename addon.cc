// Based off <https://github.com/nodejs/nan> examples (MIT license).
// Runs the HIRL MK test as a proof of concept.

#include <list>
#include <measurement_kit/common.hpp>
#include <measurement_kit/ext.hpp>      // for json, will go away in v0.8.x
#include <measurement_kit/nettests.hpp>
#include <mutex>
#include <nan.h>

namespace mk {
namespace node {

// Generic worker that can be used to wrap all MK tests.
//
// We inherit from the cool AsyncProgressWorkerBase of Nan. We must be non
// copyable and non movable because we own a pointer allocated with new. (This
// may be redundant with AsyncProgressWorkerBase but better to be explicit.)
class NettestWorker : public Nan::AsyncProgressWorkerBase<char>,
                      mk::NonCopyable, mk::NonMovable {
 public:

  // The NettestWorker constructor creates a generic nettest worker.
  //
  // The first argument is a callback, allocated with `new`, of which this
  // object takes ownership. This callback will be called whenever there
  // is progress, so to update the caller timely on that respect.
  //
  // The second argument is a callback, allocated with `new`, of which this
  // object takes ownership. This callback will be called when the test
  // is finished, with an unique argument indicating the error.
  //
  // The third argument is the test to run. This is designed so that you
  // can configure the test elsewhere and then pass it here when ready. It
  // should fit quite nicely with existing code written by @hellais.
  NettestWorker(Nan::Callback *prog_cb, Nan::Callback *callback,
                mk::nettests::BaseTest test)
    : AsyncProgressWorkerBase<char>{callback}, prog_cb{prog_cb}, test{test} {}

  // The destructor makes sure we cleanup the progress callback.
  ~NettestWorker() {
    // Note: I am assuming it would be okay to do this, since the destructor
    // of the most basic async NaN class does the same for its callback.
    if (prog_cb) {
      delete prog_cb;
    }
  }

  // Executes the nettest inside the worker-thread.
  //
  // Note: it is not safe to access V8, or V8 data structures here, so
  // everything we need for input and output should go on `this`.
  //
  // Note: here we filter exceptions for robustness. Node is compiled
  // with exceptions disabled. My understanding is that it means the
  // compiler has not generated any unwinding code. Better to stop the
  // exception here as propagating it would be really unsafe.
  //
  // Note: depending on the version of MK, this would spawn another
  // blocking thread that takes care of running the test. This is
  // maybe a little inefficient, but it fits well with the C++ model
  // that is enforced by Nan (i.e. that's the easiest way).
  //
  // Note: by default, when this function terminates, the final
  // callback is called with undefined (on success) and with the
  // registered error message (on failure).
  //
  // Note: progress it routed to the main thread - from which we can
  // call node - using Nan. I've decided to implement my own safe queue
  // to be sure that I am not losing any data here.
  void Execute(const ExecutionProgress &epro) override {
    try {
      test
        .on_progress([&epro, this](double percent, const char *message) {
          // Note: the capture here is safe because `.run()` is blocking
          nlohmann::json json{
            {"percentage", percent},
            {"message", message}
          };
          {
            std::unique_lock<std::recursive_mutex> _{mutex};
            prog_pending.push_back(json.dump());
          }
          epro.Signal();
        })
        .run();
    } catch (const std::exception &exc) {
      SetErrorMessage(exc.what());
    } catch (...) {
      SetErrorMessage("unhandled exception");
    }
  }

  // The `HandleProgressCallback` method is called by the main thread (i.e.
  // by the libuv loop) to propagate into such thread progress info.
  //
  // We basically extract from the queue of pending progress and dispatch.
  void HandleProgressCallback(const char *, size_t) override {
    std::list<std::string> cur;
    {
      std::unique_lock<std::recursive_mutex> _{mutex};
      std::swap(cur, prog_pending);
    }
    for (auto &s: cur) {
      Nan::HandleScope scope;
      v8::Local<v8::Value> argv[] = {
        // I think it's fine at this level to dispatch a serialized JSON and I
        // guess we can unparse it inside of the JavaScript code.
        Nan::New<v8::String>(s.data()).ToLocalChecked()
      };
      // My understand here is that we `Call` because we're already in the
      // context of the main thread and there would be no point in doing the
      // call using `MakeCallback`. Also, this style of calling is exactly
      // the same used by Nan, so I guess it will be okay.
      prog_cb->Call(1, argv);
    }
  }

 private:
  std::recursive_mutex mutex;
  Nan::Callback *prog_cb = nullptr;   // Note: we own this callback
  std::list<std::string> prog_pending;
  mk::nettests::BaseTest test;
};

} // namespace node
} // namespace mk

// The `run_hirl` method is what is called from Node to run the Http Invalid
// Request Line (HIRL) test (i.e. the simpler MK test).
//
// Its only argument is the callback to be called when done.
//
// In real node bindings, this would probably be a method of an exported
// object used to configure and to start the test (I think @hellais has
// written code that basically does that).
//
// We use Nan::AsyncQueueWorker, whose job is most likely to use libuv
// threads to run a background task and keep the I/O loop running (<- yes, I
// have read the code, and this corresponds to truth). I say
// so because I noticed that, without using this abstraction, Node was
// exiting before end of the test (when it was not segfaulting :-).
NAN_METHOD(run_hirl) {
  Nan::AsyncQueueWorker(new mk::node::NettestWorker{
      new Nan::Callback{info[0].As<v8::Function>()},  // progress-cb
      new Nan::Callback{info[1].As<v8::Function>()},  // done-cb
      // This part of the PoC shows that you can initialize the test elsewhere
      // and then, since it's copyable, pass it to the nettest worker
      mk::nettests::HttpInvalidRequestLineTest()
          .set_verbosity(MK_LOG_INFO)
  });
}

// The `Initalize` hook is called when the module is loaded.
//
// It basically only defines the `http_invalid_request_line` function to
// map to the `run_hirl` real function defined above.
NAN_MODULE_INIT(Initialize) {
  Nan::Set(
      target,
      Nan::New<v8::String>("run_http_invalid_request_line").ToLocalChecked(),
      Nan::GetFunction(
          Nan::New<v8::FunctionTemplate>(run_hirl)).ToLocalChecked());
}

// The `NODE_MODULE` macro tell node that this is a module.
NODE_MODULE(measurement_kit, Initialize)
