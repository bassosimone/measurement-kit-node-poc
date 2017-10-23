require("./build/Release/addon")
  .run_http_invalid_request_line((error) => {
    console.log("Test complete");
    console.log(error);
  });
