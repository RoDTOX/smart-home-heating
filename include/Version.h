#ifndef VERSION_H
#define VERSION_H

#define APP_VERSION "1.2.0"
#define APP_BUILD_DATE __DATE__
#define APP_BUILD_TIME __TIME__

// APP_GIT_COMMIT is written by tools/sh_git_version.py on every build (never hard-code it here).
#if __has_include("git_version.h")
#include "git_version.h"
#endif
#ifndef APP_GIT_COMMIT
#define APP_GIT_COMMIT "unknown"
#endif

#endif // VERSION_H
