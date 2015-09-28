{
    "target_defaults": {
      "defines": [
        "SQLITE_USE_URI",
        "SQLITE_ENABLE_STAT3=1",
        "SQLITE_ENABLE_FTS4=1",
        "SQLITE_ENABLE_FTS3_PARENTHESIS=1",
        "SQLITE_ENABLE_COLUMN_METADATA=1",
        "SQLITE_ALLOW_COVERING_INDEX_SCAN=1",
        "SQLITE_ENABLE_UNLOCK_NOTIFY",
        "SQLITE_ENABLE_LOAD_EXTENSION",
        "SQLITE_SOUNDEX",
        "HAVE_INTTYPES_H=1",
        "HAVE_STDINT_H=1",
        "HAVE_USLEEP=1",
        "HAVE_LOCALTIME_R=1",
        "HAVE_GMTIME_R=1",
        "HAVE_STRERROR_R=1",
        "HAVE_READLINE=1",
        "NDEBUG",
        "_REENTRANT",
        "_THREAD_SAFE",
        "_POSIX_PTHREAD_SEMANTICS",
        "UNSAFE_STAT_OK",
      ],
      "include_dirs": [
        ".",
        "bklib",
        "sqlite",
        "/opt/local/include",
        "<!(node -e \"require('nan')\")"
      ]
    },
    "targets": [
    {
      "target_name": "binding",
      "sources": [
        "binding.cpp",
        "bklib/bkjs.cpp",
        "bklib/bksqlite.cpp",
        "bklib/bklib.cpp",
        "bklib/bklog.cpp",
        "sqlite/sqlite3.c",
      ],
      "conditions": [
        [ 'OS=="mac"', {
          "defines": [
            "OS_MACOSX",
          ],
          "xcode_settings": {
            "OTHER_CFLAGS": [
              "-g -fPIC",
            ],
          },
        }],
        [ 'OS=="linux"', {
          "defines": [
            "OS_LINUX",
          ],
          "cflags_cc+": [
            "-g -fPIC -rdynamic",
          ],
        }],
      ]
    }]
}
