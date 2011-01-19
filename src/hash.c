#include <assert.h>
#include <pthread.h>
#include <semaphore.h>
#include <errno.h>
#include <glib.h>

#include "log.h"
#include "hash.h"
#include "metadata.h"


extern GHashTable *hash;

/* path entry on remote storage file system */
struct pentry {
        int fd;
        struct stat st;
        char digest[MD5_DIGEST_LENGTH];
        dpl_dict_t *metadata;
        pthread_mutex_t mutex;
        sem_t refcount;
        int flag;
};


pentry_t *
pentry_new(void)
{
        pentry_t *pe = NULL;
        pthread_mutexattr_t attr;
        int rc;

        pe = malloc(sizeof *pe);
        if (! pe) {
                LOG("out of memory");
                return NULL;
        }

        pe->metadata = dpl_dict_new(13);

        rc = sem_init(&pe->refcount, 0, 0);
        if (-1 == rc) {
                LOG("sem_init sem@%p: %s",
                    (void *)&pe->refcount, strerror(errno));
                goto release;
        }

        rc = pthread_mutexattr_init(&attr);
        if (rc) {
                LOG("pthread_mutexattr_init mutex@%p: %s",
                    (void *)&pe->mutex, strerror(rc));
                goto release;
        }

        rc = pthread_mutex_init(&pe->mutex, &attr);
        if (rc) {
                LOG("pthread_mutex_init mutex@%p %s",
                    (void *)&pe->mutex, strerror(rc));
                goto release;
        }

        pe->flag = FLAG_DIRTY;
        return pe;

  release:
        pentry_free(pe);
        return NULL;
}

void
pentry_free(pentry_t *pe)
{
        if (-1 != pe->fd)
                close(pe->fd);

        if (pe->metadata)
                dpl_dict_free(pe->metadata);

        (void)pthread_mutex_destroy(&pe->mutex);
        (void)sem_destroy(&pe->refcount);

        free(pe);
}

void
pentry_inc_refcount(pentry_t *pe)
{
        assert(pe);


        if (-1 == sem_post(&pe->refcount))
                LOG("sem_post@%p: %s", (void *)&pe->refcount, strerror(errno));
}

void
pentry_dec_refcount(pentry_t *pe)
{
        assert(pe);


        if (-1 == sem_wait(&pe->refcount))
                LOG("sem_wait@%p: %s", (void *)&pe->refcount, strerror(errno));
}

int
pentry_get_refcount(pentry_t *pe)
{
        assert(pe);

        int ret;
        (void)sem_getvalue(&pe->refcount, &ret);

        return ret;
}

int
pentry_trylock(pentry_t *pe)
{
        int ret;

        assert(pe);

        ret = pthread_mutex_trylock(&pe->mutex);

        LOG("mutex@%p, fd=%d: %saquired",
            (void *)&pe->mutex, pe->fd, ret ? "not " : "");

        return ret;
}

int
pentry_lock(pentry_t *pe)
{
        int ret;

        assert(pe);

        ret = pthread_mutex_lock(&pe->mutex);
        if (ret)
                LOG("mutex@%p: %s", (void *)&pe->mutex, strerror(errno));
        else
                LOG("mutex@%p, fd=%d: acquired", (void *)&pe->mutex, pe->fd);


        return ret;
}

int
pentry_unlock(pentry_t *pe)
{
        int ret;

        assert(pe);

        ret = pthread_mutex_unlock(&pe->mutex);
        if (ret)
                LOG("mutex@%p: %s", (void *)&pe->mutex, strerror(errno));
        else
                LOG("mutex@%p, fd=%d: released", (void *)&pe->mutex, pe->fd);

        return ret;
}

void
pentry_set_fd(pentry_t *pe,
              int fd)
{
        assert(pe);

        pe->fd = fd;
}

int
pentry_get_fd(pentry_t *pe)
{
        assert(pe);

        return pe->fd;
}

int
pentry_get_flag(pentry_t *pe)
{
        assert(pe);

        return pe->flag;
}

void
pentry_set_flag(pentry_t *pe, int flag)
{
        assert(pe);

        pe->flag = flag;
}


int
pentry_set_metadata(pentry_t *pe,
                    dpl_dict_t *meta)
{
        assert(pe);

        if (DPL_FAILURE == dpl_dict_copy(pe->metadata, meta))
                return -1;

        return 0;
}

dpl_dict_t *
pentry_get_metadata(pentry_t *pe)
{
        assert(pe);

        return pe->metadata;
}

int
pentry_set_digest(pentry_t *pe,
                  const char *digest)
{
        assert(pe);

        if (! digest)
                return -1;

        memcpy(pe->digest, digest, sizeof pe->digest);
        return 0;
}

char *
pentry_get_digest(pentry_t *pe)
{
        assert(pe);

        return pe->digest;
}


static void
print(void *key, void *data, void *user_data)
{
        char *path = key;
        pentry_t *pe = data;
        LOG("key=%s, fd=%d, digest=%.*s",
            path, pe->fd, MD5_DIGEST_LENGTH, pe->digest);
}

void
hash_print_all(void)
{
        g_hash_table_foreach(hash, print, NULL);
}
