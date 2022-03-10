/*
 * Copyright (C) 2022 Canonical Ltd
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 3 a
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#define _GNU_SOURCE
#ifndef _XOPEN_SOURCE
#define _XOPEN_SOURCE 700
#endif
#define _LARGEFILE64_SOURCE
#define _FILE_OFFSET_BITS 64

#include <ctype.h>
#include <dirent.h>
#include <fcntl.h>
#include <ftw.h>
#include <linux/limits.h>
#include <pwd.h>
#include <sched.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

/// Returns systemd real PID.
pid_t find_systemd(void);

/// Returns 0 when the process PID is ready.
// TODO
int wait_on_pid(int PID) { return 0; }

/// Enters in the mount and pid namespaces of the process PID, while preserving PWD.
/// Returns 0 on success.
int enter_target_ns(int PID);

/// Stops this process in favor or running it as child.
void continue_as_child(void);

int main(int argc, char *argv[]) {
#ifndef NDEBUG
    printf("Starting nslogin with arguments: ");
    for (int i = 0; i < argc; i++) {
        printf("%s ", argv[i]);
    }
    putc('\n', stdout);
    // print euid
    uid_t uid = getuid();
    gid_t gid = getgid();
    printf("euid: %d\n", geteuid());
    printf("uid: %d\n", getuid());
#endif
    // Wait for systemd to be ready
    pid_t systemdPid = find_systemd();
    if (systemdPid == 0) {
        perror("Could not find systemd PID");
        exit(1);
    }
    int systemdStatus;
    while ((systemdStatus = wait_on_pid(systemdPid))) {
    }

    // Gain root privilege
    if (setegid(0) != 0) {
        perror("setegid as root");
        exit(2);
    }
    if (seteuid(0) != 0) {
        perror("seteuid as root");
        exit(3);
    }

    // Enter systemd namespace
    if (enter_target_ns(systemdPid) != 0) {
        perror("Failed to set namespace to systemd's");
        exit(4);
    }

    // Drop priviledges back as the current user
    if (seteuid(uid) != 0) {
        perror("seteuid back as user");
        exit(5);
    }
    if (setegid(gid) != 0) {
        perror("setegid back as user");
        exit(6);
    }

    // Without forking after setting PID namespace, we get setpgid errors when piping commands.
    //  child setpgid (PID_X to PID_Y): Operation not permitted
    continue_as_child();
    // Having no argument means that we want to shell in.
    int argvslen = 1;
    if (argc > 1) {
        argvslen = argc - 1; // argv[0] is this binary, skip it.
    }
    char *argvs[argvslen + 1];
    argvs[argvslen] = NULL;

    // Entering shell as default behaviour.
    char *pathname = getenv("SHELL");
    argvs[0] = pathname;

    // We pointed to a binary to execute instead of entering the default shell.
    if (argc > 1) {
        pathname = argv[1]; // executable is passed as first argument
        for (int i = 1; i < argc; ++i) {
            argvs[i - 1] = argv[i];
        }
    }

    // Make sure any pending output is flushed before replacing this binary image.
    fflush(stdout);
    fflush(stderr);

    // Finally exec.
    if (execvp(pathname, argvs) == -1) {
        const size_t size = 128;
        char msg[size];
        snprintf(msg, size, "Executing \"%s\" failed", pathname);
        perror(msg);
        exit(7);
    }
} // main.
