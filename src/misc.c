#include <stdio.h>
#include <sys/stat.h>
#include <sys/types.h>
#if defined(__APPLE__) && defined(__MACH__) || defined(__ellcc__ )
#include <limits.h>
#else
#include <linux/limits.h>
#endif
#include <errno.h>
#include <string.h>
#include <stdlib.h>

#include "log.h"
#include "misc.h"
#include "tmpstr.h"

#define MKDIR(path, mode) do {                                          \
                if (-1 == mkdir(path, mode) && EEXIST != errno)         \
                        LOG(LOG_ERR, "mkdir(%s): %s",                   \
                            path, strerror(errno));                     \
        } while (0)

void
mkdir_tree(const char *dir) {
        char *tmp = NULL;
        char *p = NULL;

        LOG(LOG_DEBUG, "dir='%s'", dir);

        if (! dir)
                return;

        tmp = tmpstr_printf("%s", dir);

        for (p = tmp + 1; *p; p++) {
                if ('/' == *p) {
                        *p = 0;
                        MKDIR(tmp, S_IRWXU);
                        *p = '/';
                }
        }

        MKDIR(tmp, S_IRWXU);
}
