{
    "target_defaults": {
      "defines": [
        "SQLITE_USE_URI",
        "SQLITE_ENABLE_STAT4=1",
        "SQLITE_ENABLE_FTS4=1",
        "SQLITE_ENABLE_FTS5=1",
        "SQLITE_ENABLE_FTS3_PARENTHESIS=1",
        "SQLITE_ENABLE_COLUMN_METADATA=1",
        "SQLITE_ALLOW_COVERING_INDEX_SCAN=1",
        "SQLITE_ENABLE_MATH_FUNCTIONS",
        "SQLITE_ENABLE_UNLOCK_NOTIFY",
        "SQLITE_ENABLE_LOAD_EXTENSION",
        "SQLITE_ENABLE_SORTER_REFERENCES",
        "SQLITE_ENABLE_EXPLAIN_COMMENTS",
        "SQLITE_ENABLE_DBPAGE_VTAB",
        "SQLITE_ENABLE_STMTVTAB",
        "SQLITE_ENABLE_DBSTAT_VTAB",
        "SQLITE_MAX_EXPR_DEPTH=0",
        "SQLITE_OMIT_DEPRECATED",
        "SQLITE_LIKE_DOESNT_MATCH_BLOBS",
        "SQLITE_DEFAULT_MEMSTATUS=0",
        "SQLITE_OMIT_DEPRECATED",
        "SQLITE_OMIT_AUTOINIT",
        "SQLITE_ENABLE_SESSION",
        "SQLITE_ENABLE_PREUPDATE_HOOK",
        "SQLITE_DQS=0",
        "SQLITE_SOUNDEX",
        "SQLITE_ENABLE_JSON1",
        "HAVE_INTTYPES_H=1",
        "HAVE_STDINT_H=1",
        "HAVE_USLEEP=1",
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
        "sqlite/sqlite3.c",
      ],
      "conditions": [
        ['OS=="win"', {
          "msvs_settings": {
            "VCCLCompilerTool": {
              "ExceptionHandling": 1
            }
          },
          'defines': [
            'uint=unsigned int',
          ]
        }],
        [ 'OS=="mac"', {
          "defines": [
            "OS_MACOSX",
            "HAVE_LOCALTIME_R=1",
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
            "HAVE_LOCALTIME_R=1",
          ],
          "cflags_cc+": [
            "-g -fPIC -rdynamic",
          ],
        }],
      ]
    }]
}
