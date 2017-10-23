{
  "targets": [
    {
      "target_name": "addon",
      "sources": [
        "addon.cc"
      ],
      "include_dirs": ["<!(node -e \"require('nan')\")"],
      "libraries": [ "-lmeasurement_kit" ],
      "cflags_cc!": [ "-fno-rtti", "-fno-exceptions" ],
      "cflags_cc": [ "-std=c++14" ],
      "conditions": [
        ['OS=="mac"', {
          "xcode_settings": {
	    "OTHER_CPLUSPLUSFLAGS" : [ "-std=c++14" ],
            "GCC_ENABLE_CPP_EXCEPTIONS": "YES",
            "GCC_ENABLE_CPP_RTTI": "YES"
          }
        }]
      ]
    },
  ]
}
