/*
 * Copyright (c) 2015, Mitja Horvat
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * * Redistributions of source code must retain the above copyright notice, this
 *   list of conditions and the following disclaimer.
 *
 * * Redistributions in binary form must reproduce the above copyright notice,
 *   this list of conditions and the following disclaimer in the documentation
 *   and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifdef __linux__
/*
 * We need the RTLD_NEXT macro, therefore we must define _GNU_SOURCE to get it
 */
#define _GNU_SOURCE
#endif

#include <sys/stat.h>
#include <sys/syscall.h>

#include <fcntl.h>
#include <limits.h>
#include <stdbool.h>
#include <unistd.h>
#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <dlfcn.h>
#include <assert.h> 
#include <string.h>
#include <errno.h>

#define CCACHE_BIN  "/usr/bin/ccache"

/*
 * Default libc path
 */
#define LIBC_PATH   "/lib/libc.so"

#define MAX_ARG 4096

extern char **environ;

typedef int execve_func_t(const char *path, char *const argv[], char *const envv[]);

static execve_func_t *libc_execve = NULL;

#if 0
void ucw_log(char *fmt, ...)
{
FILE       *log;
va_list     va;

va_start(va, fmt);

log = fopen("/tmp/ucw.log", "a");

fprintf(log, "%d -- ", getpid());
vfprintf(log, fmt, va);

fclose(log);
}
#else
#define ucw_log(...)
#endif

/**
* Initialize UCW wrapper 
*/
void ucw_init(void)
{

if (libc_execve != NULL) return;

#ifndef __linux__
    void *h;

    h = dlopen(LIBC_PATH, RTLD_LAZY | RTLD_NOLOAD);
    if (h == NULL)
    {
        ucw_log("Error!\n");
        perror("");
        assert(!"Unable to dlopen() libc.so: " LIBC_PATH);
    }
#endif

    libc_execve = dlsym(RTLD_NEXT, "execve");
    if (libc_execve == NULL) assert(!"unable to resolve execve()");

#ifndef __linux__
    dlclose(h);
#endif
}

/**
* Compare end of strings 
*/
int strendcmp(const char *str, const char *end)
{
    size_t slen;
    size_t elen;

    slen = strlen(str);
    elen = strlen(end);

    if (slen < elen) return -1;

    return strcmp(str + slen - elen, end);
}

/**
* Try to find @p filename in the paths specified by the environment variable PATH and return
* a full path if found.
*/
bool resolve_path(char *out, size_t outsz, const char *filename)
{
    const char *env_path;
    const char *ppath;
    size_t      plen;
    size_t      flen;
    char        opath[PATH_MAX];
    const char *psrc = NULL;

    /* Do not do any PATH resolving if filename contains a '/' */
    if (strchr(filename, '/') != NULL)
    {
        psrc = filename;
        goto exit;
    }

    /* No PATH variable set, use some defaults */
    env_path = getenv("PATH");
    if (env_path == NULL)
    {
        env_path = ".:/bin:/usr/bin";
    }

    /* Split PATH by the ':' separator and append filename.
     * Check if a file by that name exists and is executable */
    ppath = env_path;
    flen  = strlen(filename);
    plen  = 0;

    /* Traverse the PATH fields */
    while (*ppath != '\0')
    {
        /* Move string ahead */
        ppath += plen;

        /* This is the last entry in the string? */
        if (*ppath == '\0')
        {
            break;
        }

        /* Skip all empty ':' */
        ppath += strspn(ppath, ":");

        /* Caluclate path length up to the ':' delimiter or end of the string */
        plen = strcspn(ppath, ":");

        /* If the total resolved path exceeds opath, skip it */
        if ((plen + flen + 1) >= sizeof(opath))
        {
            continue;
        }

        /* Copy the partial string from ppath to opath, and append a '\0' */
        strncpy(opath, ppath, plen);
        opath[plen] = '\0';


        /* Append a '/' character at the end of the string */
        if (opath[plen - 1] != '/') strcat(opath, "/");

        /* Append the filename */
        strcat(opath, filename);

        /* Check if this path is accessible and is executable */
        if (access(opath, X_OK) == 0)
        {
            psrc = opath;
            goto exit;
        }
    }

exit:
    if (psrc == NULL)
    {
        ucw_log("Error resolving: %s\n", filename);
        return false;
    }

    if (outsz < (strlen(psrc) + 1))
    {
        /* Not enough space on target */
        return false;
    }

    strncpy(out, psrc, outsz);
    return true;
}

/**
* Check if path is a real-GCC compiler (not a ccache symlink or anything similar)
*/
bool gcc_check(const char *path, char *const argv[], char *const envv[])
{
    char          **argp;
    struct stat     st;
    char            she_bang[2];

    bool            gcc_found = false;
    char           *gcc_path = NULL;
    int             gcc_fd   = -1;

    /* Check if the gcc_path ends in "gcc" */
    if (strendcmp(path, "gcc") != 0 &&
        strendcmp(path, "g++") != 0 && 
        strendcmp(path, "c++") != 0)
    {
        goto exit;
    }

    if (lstat(path, &st) == 0)
    {
        /* She-bang detection, if we're executing a script, do not run it through ccache */
        gcc_fd = open(path, O_RDONLY);
        if (gcc_fd < 0)
        {
            goto exit;
        }

        if (read(gcc_fd, she_bang, 2) >= 2)
        {
            if (she_bang[0] == '#' && she_bang[1] == '!')
            {
                goto exit;
            }
        }

        /* It is a symbolic link, read the target path */
        if (S_ISLNK(st.st_mode))
        {
            gcc_path = realpath(path, NULL); 
        }
    }

    if (gcc_path == NULL) gcc_path = strdup(path);

    /*
     * CCACHE doesn't know how to handle the --save-temps option which is
     * used by some configure scripts to inspect temporary files produced during compilation
     */
    for (argp = (char **)argv; *argp != NULL; argp++)
    {
        if (strendcmp(*argp, "save-temps") == 0) goto exit;
    }

    gcc_found = true;

exit:
    if (gcc_fd >= 0) close(gcc_fd);
    if (gcc_path != NULL) free(gcc_path);

    return gcc_found;
}

/**
 * Copy a NULL-terminated variable argument list to an array of char *
 */
bool va_to_argv(va_list va, char **argv, size_t argv_len)
{
    char  *carg;

    char **argp = argv;

    for (argp = argv; (argp - argv) < argv_len; argp++)
    {
        carg = va_arg(va, char *);
        *argp = carg;

        /* End of arguments, return */
        if (carg == NULL) return true;
    }

    return false;
}


/**
 * Run command in @p path through CCACHE
 */
int ccache_exec(const char *path, char *const argv[], char *const envv[])
{
    int   retval;
    char *new_envv[MAX_ARG];
    char *new_argv[MAX_ARG];

    /*
     * Mangle the environment, copy envv to new_envv:
     *
     *  - Remove LD_PRELOAD
     *  - remove CCACHE_DIR 
     */
    char **sp = (char **)envv;
    char **dp = (char **)new_envv;

    while (*sp != NULL)
    {
        if ((dp - new_envv) >= MAX_ARG)
        {
            /* Error */
            errno = E2BIG;
            return -1;
        }

        /* Remove the LD_PRELOAD env variable */
        if (strncmp(*sp, "LD_PRELOAD", strlen("LD_PRELOAD")) == 0)
        {
        }
        /* Remove CCACHE_DIR */
        else if (strncmp(*sp, "CCACHE_DIR", strlen("CCACHE_DIR")) == 0)
        {
        }
        /* Copy arguments */
        else
        {
            *dp++ = *sp;
        }

        sp++;
    }
    *dp = NULL;

    /**
     * Mangle the argument list:
     *  - As first argument insert CCACHE_BIN
     *  - As second argument insert the full path
     *  - Ignore the first argument (the filename) from the first list
     */
    sp = (char **)argv;
    dp = (char **)new_argv;

    /* ARGv[0] must be ccache, ARGV[1] must the full path */
    *dp++ = CCACHE_BIN;
    *dp++ = (char *)path;

    /* Some error checking */
    if (*sp == NULL)
    {
        return EFAULT;
    }

    sp++;

    while (*sp != NULL)
    {
        if ((dp - new_argv) >= MAX_ARG)
        {
            errno = E2BIG;
            return -1;
        }

        *dp++ = *sp++;
    }

    *dp = NULL;

#if 0
    ucw_log("-----------------------------------\n");
    ucw_log("CCACHE: path = %s\n", path);

    dp = new_argv;
    while (*dp != NULL)
    {
        ucw_log("ARG: %s\n", *dp++);
    }

    dp = new_envv;
    while (*dp != NULL)
    {
        ucw_log("ENV: %s\n", *dp++);
    }
#endif

    ucw_log("CCACHE_EXEC: %s\n", new_argv[0]);

    retval = libc_execve(CCACHE_BIN, new_argv, new_envv);
    if (retval != 0)
    {
        ucw_log("execve failed: %s %s\n", CCACHE_BIN, strerror(errno));
        ucw_log("PATH: %s -- %s %s %s\n", path, new_argv[0], new_argv[1], new_argv[2]);

        dp = new_argv;

        while (*dp != NULL)
        {
            ucw_log("ARG: %s\n", *dp++);
        }

        dp = new_envv;

        while (*dp != NULL)
        {
            ucw_log("ENV: %s\n", *dp++);
        }
    }

    return retval;

}

int execve(const char *path, char *const argv[], char *const envv[])
{
    ucw_log("EXECVE: %s\n", path);

    ucw_init();

    if (getenv("UCC_RECURSE") != NULL)
    {
        ucw_log("UCC recursion detected.\n");
        return libc_execve(path, argv, envv);
    }

    /* If we're not executing a GCC binary, just bypass it */
    if (gcc_check(path, argv, envv))
    {
        return ccache_exec(path, argv, envv);
    }

    return libc_execve(path, argv, envv);
}

int execv(const char *path, char *const argv[])
{
    ucw_log("EXECV: %s\n", path);

    return execve(path, argv, environ);
}

int execvpe(const char *path, char *const argv[], char *const envv[])
{
    int retval;

    ucw_log("EXECVPE: %s\n", path);

    char rpath[PATH_MAX];

    if (!resolve_path(rpath, sizeof(rpath), path))
    {
        return ENOENT;
    }

    retval = execve(rpath, argv, envv);
    if (retval != 0 && errno == ENOEXEC)
    {
        /*
         * Execution failed because the exec format is unknown.  The manual page
         * says we have to run it through /bin/sh at this point 
         */
        char   *new_argv[MAX_ARG];

        char  **dp = (char **)new_argv;
        char  **sp = (char **)argv;

        if (*sp == NULL)
        {
            return EFAULT;
        }

        *dp++ = *sp++;
        *dp++ = (char *)rpath;

        while (*sp != NULL)
        {
            if ((dp - new_argv) >= MAX_ARG) break;
            *dp++ = *sp++;
        }

        *dp++ = NULL;

        retval = execve("/bin/sh", new_argv, envv);
    }

    return retval;
}

int execvp(const char *path, char *const argv[])
{
    ucw_log("EXECVP: %s\n", path);
    return execvpe(path, argv, environ);
}

int execle(const char *path, const char *arg, ...)
{
    va_list     va;
    char       *new_argv[MAX_ARG + 1];
    char      **new_envv;

    ucw_log("EXECLE: %s %s\n", path, arg);

    va_start(va, arg);

    new_argv[0] = (char *)arg;
    if (!va_to_argv(va, new_argv + 1, MAX_ARG)) return -1;
    
    /* va_to_argv() exhausted all argument va_list entries; the next one should be the
       environment */
    new_envv = va_arg(va, char **);

    return execve(path, new_argv, new_envv);
}

int execl(const char *path, const char *arg, ...)
{
    va_list     va;
    char       *new_argv[MAX_ARG + 1];

    ucw_log("EXECL: %s %s\n", path, arg);

    va_start(va, arg);
    new_argv[0] = (char *)arg;
    if (!va_to_argv(va, new_argv + 1, MAX_ARG)) return -1;
    va_end(va);

    return execve(path, new_argv, environ);
}

int execlp(const char *path, const char *arg, ...)
{
    va_list     va;
    char       *new_argv[MAX_ARG + 1];

    ucw_log("EXECLP: %s %s\n", path, arg);

    va_start(va, arg);
    new_argv[0] = (char *)arg;
    if (!va_to_argv(va, new_argv + 1, MAX_ARG)) return -1;
    va_end(va);

    return execvpe(path, new_argv, environ);
}
