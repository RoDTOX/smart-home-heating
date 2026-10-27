#ifndef VERSION_H
#define VERSION_H

#if __has_include("git_version.h")
#include "git_version.h"
#endif

#ifndef APP_VERSION
#define APP_VERSION "v1.10.27.1"
#endif
#ifndef APP_BUILD_NUMBER
#define APP_BUILD_NUMBER 1
#endif
#ifndef APP_TOTAL_COMMITS
#define APP_TOTAL_COMMITS 34
#endif
#ifndef APP_GIT_COMMIT
#define APP_GIT_COMMIT "unknown"
#endif
#ifndef APP_BUILD_DATE
#define APP_BUILD_DATE __DATE__
#endif
#ifndef APP_BUILD_TIME
#define APP_BUILD_TIME __TIME__
#endif

#endif // VERSION_H
