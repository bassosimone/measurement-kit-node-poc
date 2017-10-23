// Based off <https://github.com/nodejs/nan> examples (MIT license)

#include <measurement_kit/common/logger.hpp>
#include <measurement_kit/nettests/nettests.hpp>
#include <nan.h>

namespace mk {
namespace node {

class NettestWorker : public Nan::AsyncWorker {
 public:
  NettestWorker(Nan::Callback *callback, mk::nettests::BaseTest test)
    : AsyncWorker{callback}, test{test} {}

  ~NettestWorker() {}

  // Executed inside the worker-thread.
  // It is not safe to access V8, or V8 data structures
  // here, so everything we need for input and output
  // should go on `this`.
  void Execute() {
    test.run();
  }

  // Executed when the async work is complete
  // this function will be run inside the main event loop
  // so it is safe to use V8 again
  void HandleOKCallback() {
    Nan::HandleScope scope;
    v8::Local<v8::Value> argv[] = { Nan::Null() };
    callback->Call(1, argv);
  }

private:
  mk::nettests::BaseTest test;
};

} // namespace node
} // namespace mk

NAN_METHOD(run_hirl) {
  Nan::Callback *callback = new Nan::Callback{info[0].As<v8::Function>()};
  Nan::AsyncQueueWorker(new mk::node::NettestWorker{
      callback,
      mk::nettests::HttpInvalidRequestLineTest()
          .set_verbosity(MK_LOG_INFO)
  });
}

NAN_MODULE_INIT(Initialize) {
  Nan::Set(
      target,
      Nan::New<v8::String>("run_http_invalid_request_line").ToLocalChecked(),
      Nan::GetFunction(
          Nan::New<v8::FunctionTemplate>(run_hirl)).ToLocalChecked());
}

NODE_MODULE(addon, Initialize)
