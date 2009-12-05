AC_ARG_WITH([build-timestamp],
            AC_HELP_STRING([--without-build-timestamp],[Don't include build timestamp in version information]),
            [ with_build_timestamp=${withval} ],
            [ with_build_timestamp=yes ])

if test "$with_build_timestamp" != "no" ; then
  AC_DEFINE(HAVE_BUILD_TIMESTAMP, [1], [Define if the build timestamp should be included in the version information])
fi
