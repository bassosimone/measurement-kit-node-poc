{
  "targets": [
    {
      "target_name": "addon",
      "sources": [
        "addon.cc"
      ],
      "include_dirs": ["<!(node -e \"require('nan')\")"],
      "libraries": [ "-lmeasurement_kit" ],
      'cflags_cc!': [ '-fno-rtti', '-fno-exceptions' ],
      'conditions': [
        ['OS=="mac"', {
          'xcode_settings': {
            'GCC_ENABLE_CPP_EXCEPTIONS': 'YES',
            'GCC_ENABLE_CPP_RTTI': 'YES'
          }
        }]
      ]
    },
  ]
}
