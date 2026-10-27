#ifndef VERSION_H
#define VERSION_H

#if __has_include("git_version.h")
#include "git_version.h"
#endif

#ifndef APP_BUILD_NUMBER
#define APP_BUILD_NUMBER 0
#endif
#ifndef APP_GIT_COMMIT
#define APP_GIT_COMMIT "unknown"
#endif

#define APP_STR_(x) #x
#define APP_STR(x) APP_STR_(x)

// Bump major.minor by hand; the patch number is the git commit count (tools/git_version.py)
#define APP_VERSION "1.2." APP_STR(APP_BUILD_NUMBER)
#define APP_BUILD_DATE __DATE__
#define APP_BUILD_TIME __TIME__

#endif // VERSION_H
