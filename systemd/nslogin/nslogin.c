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
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <systemd/sd-bus.h>
#include <unistd.h>

/// Returns systemd real PID.
pid_t find_systemd(void);

/// Returns true when the process PID is ready.
bool wait_on_systemd(void);

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
    while (!wait_on_systemd()) {
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
    // Make sure any pending output is flushed before forking.
    fflush(stdout);
    fflush(stderr);

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

    // Finally exec.
    if (execvp(pathname, argvs) == -1) {
        const size_t size = 128;
        char msg[size];
        snprintf(msg, size, "Executing \"%s\" failed", pathname);
        perror(msg);
        exit(7);
    }
} // main.

/// Returns true if systemd is still starting.
static bool is_systemd_starting(sd_bus *bus) {
    char *msg = NULL;
    // sd_bus_error_free is safe to call even if there is no error data.
    sd_bus_error err __attribute__((__cleanup__(sd_bus_error_free))) = SD_BUS_ERROR_NULL;
    int busOk = sd_bus_get_property_string(bus, "org.freedesktop.systemd1", "/org/freedesktop/systemd1",
                                           "org.freedesktop.systemd1.Manager", "SystemState", &err, &msg);
    if (busOk < 0) {
        fprintf(stderr, "Bus error %s", err.message);
        return false;
    }

    if (msg == NULL) {
        return false;
    }
    // We won't get the message "Failed to connect to bus: No such file or directory" at this point because there is a
    // bus available at this point.
    bool starting = (strncmp(msg, "initializing", 12) == 0 || strncmp(msg, "starting", 8) == 0);
    free(msg);
    return starting;
}

/// Returns a pid from a path of the form /proc/[PID]/anything (3 level deep)
static pid_t pid_from_path(const char *path, int basenameOffset) {
    char pidString[10];
    char *pidStarts = strchr(path + 1, '/');
    if (pidStarts == NULL) {
        perror("Cannot determine where pid starts in path");
        return 0;
    }
    ssize_t pidLenght = path + basenameOffset - pidStarts - 2;
    strncpy(pidString, pidStarts + 1, pidLenght);
    pidString[pidLenght] = '\0';
    int pid = atoi(pidString);
    if (pid == 0) {
        perror("Failed to determine PID from path");
        fprintf(stderr, "Failed pid string was %s", pidString);
        return 0;
    }
    return (pid_t)pid;
}

/// Returns 0 if the basename of the symlink target matches the expected name up to length.
static int symlink_basename_cmp(const char *symlink, const char *name, int length) {
    char target[PATH_MAX];
    ssize_t len = readlink(symlink, target, PATH_MAX);
    if (len == -1){
        perror(symlink);
        return -5;
    }
    target[len] = '\0';
    char *targetBasename = strrchr(target, '/');
    if (targetBasename == NULL) {
        perror("Target base name error");
        return -5;
    }
    targetBasename += 1;
    printf("%s --> %s\n", symlink, target);
    return strncmp(targetBasename, name, length);
}

/// Returns the systemd PID if found, or zero otherwise.
static int check_entry_for_systemd(const char *path, const struct stat *info, const int typeflag, struct FTW *pathinfo) {
    struct procInfo {
        const char *basename;
        int basenameSize;
        uid_t owner;
    };

    struct procInfo systemd = {"systemd", 7, 0};
    // struct procInfo systemd = {"nvim", 4, 1000};

    if (typeflag == FTW_SL && pathinfo->level == 2 && strncmp(&path[pathinfo->base], "exe", 3) == 0 &&
        info->st_uid == systemd.owner && info->st_gid == systemd.owner) {
        if (symlink_basename_cmp(path, systemd.basename, systemd.basenameSize) != 0) {
            return 0;
        }

        return pid_from_path(path, pathinfo->base);
    }
    return 0;
}

pid_t find_systemd(void) {
    int result = nftw("/proc", check_entry_for_systemd, 10, FTW_PHYS);
    return (pid_t)result;
}

bool wait_on_systemd(void) {
    sd_bus *bus;
    bool systemdStarting = true;
    int waitCounts = 0;
    int busOk = -1;
    const int timeoutUs = 500000;
    const int loopMax = 19;
    for (busOk = sd_bus_default_system(&bus); busOk < 0 && waitCounts < loopMax; waitCounts++) {
        usleep(timeoutUs); // 500 ms.
        busOk = sd_bus_default_system(&bus);
    }
    // If a timeout had occurred before and bus was still unavailable, the following loop will not even run.
    for (; busOk >= 0 && waitCounts <= loopMax && (systemdStarting = is_systemd_starting(bus)); waitCounts++) {
        usleep(timeoutUs); // 500 ms.
    }
    fflush(stdout);
    fflush(stderr);
    sd_bus_unref(bus);
    return !systemdStarting;
}

void continue_as_child(void) {
    pid_t child = fork();
    int status;
    pid_t ret;

    if (child < 0)
        perror("fork failed");

    /* Only the child returns */
    if (child == 0)
        return;

    for (;;) {
        ret = waitpid(child, &status, WUNTRACED);
        if ((ret == child) && (WIFSTOPPED(status))) {
            /* The child suspended so suspend us as well */
            kill(getpid(), SIGSTOP);
            kill(child, SIGCONT);
        } else {
            break;
        }
    }
    /* Return the child's exit code if possible */
    if (WIFEXITED(status)) {
        exit(WEXITSTATUS(status));
    } else if (WIFSIGNALED(status)) {
        kill(getpid(), WTERMSIG(status));
    }
    exit(EXIT_FAILURE);
}

#define MAX_NS_PATH 25
#define TARGET_NS_COUNT 2
int enter_target_ns(pid_t PID) {
    char currentDir[PATH_MAX];
    getcwd(currentDir, PATH_MAX);

    struct {
        int nstype;
        const char *nsname;
        int nsfd;
    } ns[TARGET_NS_COUNT] = {{CLONE_NEWPID, "pid", 0}, {CLONE_NEWNS, "mnt", 0}};

    // open all file descriptors first to avoid problems to find them after setting namespaces.
    char nsPath[MAX_NS_PATH];
    for (int i = 0; i < TARGET_NS_COUNT; i++) {
        memset(nsPath, 0, MAX_NS_PATH);
        if (snprintf(nsPath, MAX_NS_PATH, "/proc/%d/ns/%s", PID, ns[i].nsname) <= 0) {
            perror("Faild to format namespace path");
            return -1;
        }
        int fd = open(nsPath, O_RDONLY);
        if (fd <= 0) {
            fprintf(stderr, "\tpath was %s\n", nsPath);
            perror("Failed to open namespace file descriptor");
            return -2;
        }
        ns[i].nsfd = fd;
    }

    for (int i = 0; i < TARGET_NS_COUNT; i++) {
        if (setns(ns[i].nsfd, ns[i].nstype) != 0) {
            perror("Failed to set namespace");
            return -3;
        }
        close(ns[i].nsfd);
    }

    chdir(currentDir);
    return 0;
}
