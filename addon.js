require("./build/Release/measurement-kit")
  .run_http_invalid_request_line(
    (progress, message) => {
      console.log("Progress", progress, message);
    },
    (error) => {
      console.log("Test complete");
      console.log(error);
    });
