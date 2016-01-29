
AC_DEFUN([AC_DEF_API_EXPORT_ATTRS],
[
  AC_REQUIRE([AC_PROG_CC])
  dnl First, check whether -Werror can be added to the command line, or
  dnl whether it leads to an error because of some other option that the
  dnl user has put into $CC $CFLAGS $CPPFLAGS.
  AC_MSG_CHECKING([whether the -Werror option is usable])
  AC_CACHE_VAL([gl_cv_cc_vis_werror], [
    gl_save_CFLAGS="$CFLAGS"
    CFLAGS="$CFLAGS -Werror"
    AC_COMPILE_IFELSE(
      [AC_LANG_PROGRAM([[]], [[]])],
      [gl_cv_cc_vis_werror=yes],
      [gl_cv_cc_vis_werror=no])
    CFLAGS="$gl_save_CFLAGS"])
  AC_MSG_RESULT([$gl_cv_cc_vis_werror])
  dnl Now check whether visibility declarations are supported.
  AC_MSG_CHECKING([for API/local visibility declarations])
  local_symbol=
  api_exported=
  gl_save_CFLAGS="$CFLAGS"
  dnl We use the option -Werror and a function dummyfunc, because on some
  dnl platforms (Cygwin 1.7) the use of -fvisibility triggers a warning
  dnl "visibility attribute not supported in this configuration; ignored"
  dnl at the first function definition in every compilation unit, and we
  dnl don't want to use the option in this case.
  if test $gl_cv_cc_vis_werror = yes; then
    CFLAGS="$CFLAGS -Werror"
  fi
  dnl Try ELF specific attribute
  AC_COMPILE_IFELSE(
    [AC_LANG_PROGRAM(
       [[int hiddenfunc (void);
         int exportedfunc (void);
         __attribute__((__visibility__("hidden"))) int hiddenfunc (void) {return 1;}
         __attribute__((__visibility__("default"))) int exportedfunc (void) {return 1;}
       ]],
       [[]])],
    [local_symbol='__attribute__ ((visibility ("hidden")))'
     api_exported='__attribute__ ((visibility ("default")))'])
  dnl Try WIN32 specific attribute
  AS_IF([test -z "$api_exported"],
    [AC_COMPILE_IFELSE(
      [AC_LANG_PROGRAM(
         [[int exportedfunc (void);
           __declspec(dllexport) int exportedfunc (void) {return 1;}
         ]],
         [[]])],
      [api_exported='__declspec(dllexport)'])])
  AC_DEFINE_UNQUOTED(LOCAL_FN, [$local_symbol], [attribute of the non-exported symbols])
  AC_DEFINE_UNQUOTED(API_EXPORTED, [$api_exported], [attribute of the symbols exported in the API])
  CFLAGS="$gl_save_CFLAGS"
])
