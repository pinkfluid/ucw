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

#include <limits.h>
#include <stdbool.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <assert.h>
#include <errno.h>
#include <string.h>
#include <libgen.h>

void usage()
{
    printf("ucw FILE [ARG1] [ARG2] ... \n");
    printf("\n");
    printf("(U)niversal (C)Cache (W)rapper\n");
    printf("Set LD_PRELOAD to libucw.so and execute FILE.\n\n");
}

bool get_self(char *path, size_t pathsz)
{
    if (readlink("/proc/self/exe", path, pathsz) < 0)
    {
        return false;
    }

    return true;
}

int main(int argc, char *argv[])
{
    char exec_path[PATH_MAX];
    char *libdir;
    char libucw_path[PATH_MAX];

    if (argc <= 1)
    {
        usage();
        return 1;
    }

    /* Get the current executable filename */
    get_self(exec_path, sizeof(exec_path));

    libdir = dirname(exec_path);

    snprintf(libucw_path, sizeof(libucw_path), "%s/%s", libdir, "libucw.so");

    setenv("LD_PRELOAD", libucw_path, 1);

    execvp(argv[1], &argv[1]);

    perror("Error executing command");

    return 1;
}
