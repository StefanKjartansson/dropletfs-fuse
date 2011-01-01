#include <unistd.h>
#include <errno.h>

#include "fsync.h"
#include "hash.h"
#include "log.h"
#include "glob.h"

int
dfs_fsync(const char *path,
          int issync,
          struct fuse_file_info *info)
{
        struct pentry *pe = NULL;

        LOG("%s", path);

        pe = g_hash_table_find(hash, pentry_cmp_callback, (char *)path);

        if (pe && -1 != pe->fd) {
                if (! issync && -1 == fsync(pe->fd)) {
                        LOG("%s - %s", path, strerror(errno));
                        return EIO;
                }
        }

        return 0;
}
