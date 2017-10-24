require("./build/Release/measurement-kit")
  .run_http_invalid_request_line(
    (progress) => {
      console.log("Progress", progress);
    },
    (error) => {
      console.log("Test complete");
      console.log(error);
    });
