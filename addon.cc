// Based off <https://github.com/nodejs/nan> examples (MIT license).
// Runs the HIRL MK test as a proof of concept.

#include <measurement_kit/common.hpp>
#include <measurement_kit/nettests.hpp>
#include <nan.h>

namespace mk {
namespace node {

// Generic worker that can be used to wrap all MK tests.
class NettestWorker : public Nan::AsyncWorker {
 public:

  // The NettestWorker constructor creates a generic nettest worker.
  //
  // The first argument is a callback, allocated with `new`, of which this
  // object takes ownership. This callback will be called when the test
  // is finished, with `null` as its unique argument.
  //
  // The second argument is the test to run. This is designed so that you
  // can configure the test elsewhere and then pass it here when ready.
  NettestWorker(Nan::Callback *callback, mk::nettests::BaseTest test)
    : AsyncWorker{callback}, test{test} {}

  ~NettestWorker() {}

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
  void Execute() override {
    try {
      test.run();
    } catch (const std::exception &exc) {
      SetErrorMessage(exc.what());
    } catch (...) {
      SetErrorMessage("unhandled exception");
    }
  }

 private:
  mk::nettests::BaseTest test;
};

} // namespace node
} // namespace mk

// The `run_hirl` method is what is called from Node to run the Http Invalid
// Request Line (HIRL) test (i.e. the simpler MK test).
//
// Its only argument is the callback to be called when done.
//
// In real node bindings, this would probably a method of an exported
// object used to configure and to start the test.
//
// We use Nan::AsyncQueueWorker, whose job is most likely to use libuv
// threads to run a background task and keep the I/O loop running. I say
// so because I noticed that, without using this abstraction, Node was
// exiting before end of the test (when it was not segfaulting).
NAN_METHOD(run_hirl) {
  Nan::Callback *callback = new Nan::Callback{info[0].As<v8::Function>()};
  Nan::AsyncQueueWorker(new mk::node::NettestWorker{
      callback,
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
NODE_MODULE(addon, Initialize)
