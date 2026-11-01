#ifndef VERSION_H
#define VERSION_H

#define APP_BUILD_DATE __DATE__
#define APP_BUILD_TIME __TIME__

// APP_GIT_COMMIT and APP_VERSION (1.MM.DD.NN, FR-21) are written by tools/sh_git_version.py on every build
// (never hard-code them here).
#if __has_include("git_version.h")
#include "git_version.h"
#endif
#ifndef APP_GIT_COMMIT
#define APP_GIT_COMMIT "unknown"
#endif
#ifndef APP_VERSION
#define APP_VERSION "1.00.00.00"
#endif

#endif // VERSION_H
