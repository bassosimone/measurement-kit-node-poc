// Based off <https://github.com/nodejs/nan> examples (MIT license).
// Runs the HIRL MK test as a proof of concept.

#include <functional>
#include <list>
#include <measurement_kit/common.hpp>
#include <measurement_kit/nettests.hpp>
#include <mutex>
#include <nan.h>
#include <uv.h>

extern "C" {
static void mkuv_async_wakeup(uv_async_t *handle);
static void mkuv_async_destroy(uv_handle_t *handle);
}

namespace mk {
namespace node {

struct NettestContext {
    std::list<std::function<void()>> actions;
    Var<Nan::Callback> cb_done;
    Var<Nan::Callback> cb_progress;
    std::recursive_mutex mutex;
};

template <typename Nettest>
void nettest_start_with_context(Var<NettestContext> ctx) {
    uv_async_t *async = new uv_async_t{};
    async->data = ctx.get();
    if (uv_async_init(uv_default_loop(), async, mkuv_async_wakeup) != 0) {
        throw std::runtime_error("uv_async_init");
    }
    Nettest nettest;
    nettest.set_verbosity(MK_LOG_DEBUG); // XXX
    nettest.on_destroy(
            [async]() { uv_close((uv_handle_t *)async, mkuv_async_destroy); });
    if (ctx->cb_progress) {
        nettest.on_progress([async, ctx](double percentage, const char *msg) {
            std::unique_lock<std::recursive_mutex> _{ctx->mutex};
            std::string mcopy = msg;
            ctx->actions.push_back([ctx, percentage, mcopy]() {
                std::unique_lock<std::recursive_mutex> _{ctx->mutex};
                Nan::HandleScope scope;
                const int argc = 2;
                v8::Local<v8::Value> argv[argc] = {
                        Nan::New(percentage), Nan::New(mcopy).ToLocalChecked()};
                ctx->cb_progress->Call(argc, argv);
            });
            if (uv_async_send(async) != 0) {
                throw std::runtime_error("uv_async_send");
            }
        });
    }
    nettest.start([async, ctx]() {
        std::unique_lock<std::recursive_mutex> _{ctx->mutex};
        ctx->actions.push_back([ctx]() {
            std::unique_lock<std::recursive_mutex> _{ctx->mutex};
            Nan::HandleScope scope;
            ctx->cb_done->Call(0, nullptr);
        });
        if (uv_async_send(async) != 0) {
            throw std::runtime_error("uv_async_send");
        }
    });
}

} // namespace node
} // namespace mk

static void mkuv_async_wakeup(uv_async_t *handle) {
    using namespace mk::node;
    NettestContext *ctx = static_cast<NettestContext *>(handle->data);
    std::list<std::function<void()>> actions;
    {
        std::unique_lock<std::recursive_mutex> _{ctx->mutex};
        std::swap(ctx->actions, actions);
    }
    for (auto &fn : actions) {
        fn();
    }
}

static void mkuv_async_destroy(uv_handle_t *handle) {
    delete reinterpret_cast<uv_async_t *>(handle);
}

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
    mk::Var<mk::node::NettestContext> ctx = new mk::node::NettestContext{};
    ctx->cb_progress.reset(new Nan::Callback{info[0].As<v8::Function>()});
    ctx->cb_done.reset(new Nan::Callback{info[1].As<v8::Function>()});
    mk::node::nettest_start_with_context<
            mk::nettests::HttpInvalidRequestLineTest>(ctx);
}

// The `Initalize` hook is called when the module is loaded.
//
// It basically only defines the `http_invalid_request_line` function to
// map to the `run_hirl` real function defined above.
NAN_MODULE_INIT(Initialize) {
    Nan::Set(target,
            Nan::New<v8::String>("run_http_invalid_request_line")
                    .ToLocalChecked(),
            Nan::GetFunction(Nan::New<v8::FunctionTemplate>(run_hirl))
                    .ToLocalChecked());
}

// The `NODE_MODULE` macro tell node that this is a module.
NODE_MODULE(measurement_kit, Initialize)
