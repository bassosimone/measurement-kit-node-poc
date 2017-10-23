require("./build/Release/addon")
  .run_http_invalid_request_line(() => {
    console.log("Test complete");
  });
