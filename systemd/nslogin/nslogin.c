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

#include <fcntl.h>
#include <inttypes.h>
#include <linux/limits.h>
#include <pwd.h>
#include <sched.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
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

#define RESTART_MSG "Please terminate this instance by running \"wsl -t <distro>\" from Windows shell and try again.\n"

int main(int argc, char *argv[]) {
    uid_t uid = getuid();
    gid_t gid = getgid();
#ifndef NDEBUG
    printf("Starting nslogin with arguments: ");
    for (int i = 0; i < argc; i++) {
        printf("%s ", argv[i]);
    }
    putc('\n', stdout);
    // print euid
    printf("euid: %d\n", geteuid());
    printf("uid: %d\n", getuid());
#endif
    // Wait for systemd to be ready
    pid_t systemdPid = find_systemd();
    if (systemdPid == 0) {
        fprintf(stderr, "Systemd is not running. " RESTART_MSG);
        exit(1);
    }
    if (!wait_on_systemd()) {
        fprintf(stderr, RESTART_MSG);
        exit(1);
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

    // Drop privileges back as the current user
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

/// Returns true if systemd completed its startup.
static bool is_systemd_startup_complete(sd_bus *bus) {
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

/// Returns 0 if the basename of the symlink target matches the expected name up to length.
static int symlink_basename_cmp(const char *symlink, const char *name, size_t length) {
    char target[PATH_MAX];
    ssize_t len = readlink(symlink, target, PATH_MAX);
    if (len == -1) {
        // This condition becomes really common if attempting to find the PID by trial-and-error.
        // Thus, printing this error condition would become just noise.
        return -5;
    }
    target[len] = '\0';
    char *targetBasename = strrchr(target, '/');
    if (targetBasename == NULL) {
        perror("Target base name error");
        return -5;
    }
    targetBasename += 1;
    return strncmp(targetBasename, name, length);
}

static bool match_owner(const char *path, uid_t uid, gid_t gid) {
    struct stat fileStat;
    if (stat(path, &fileStat) != 0) {
        char msg[96];
        snprintf(msg, 96, "Failed to stat %s", path);
        perror(msg);
        return false;
    }

    return uid == fileStat.st_uid && gid == fileStat.st_gid;
}

pid_t find_systemd(void) {
    struct procInfo {
        const char *basename;
        size_t basenameSize;
        uid_t owner;
    };

    struct procInfo systemd = {"systemd", 7, 0};
    char exeLinkPath[16] = {'\0'};
    for (int16_t pidCandidate = 2; pidCandidate < INT16_MAX; pidCandidate++) {
        snprintf(exeLinkPath, 16, "/proc/%" SCNd16 "/exe", pidCandidate);
        int res = symlink_basename_cmp(exeLinkPath, systemd.basename, systemd.basenameSize);
        if (res != 0) {
            continue;
        }
        if (!match_owner(exeLinkPath, systemd.owner, systemd.owner)) {
            continue;
        }
        return (pid_t)pidCandidate;
    }
    return 0;
}

bool wait_on_systemd(void) {
    sd_bus *bus;
    bool systemdStarted = true;
    int waitCounts = 0;
    int busOk = -1;
    const unsigned int timeoutUs = 500000;
    const int loopMax = 19;
    for (busOk = sd_bus_default_system(&bus); busOk < 0 && waitCounts < loopMax; waitCounts++) {
        usleep(timeoutUs); // 500 ms.
        busOk = sd_bus_default_system(&bus);
    }
    // If a timeout had occurred before and bus was still unavailable, the following loop will not even run.
    for (; busOk >= 0 && waitCounts <= loopMax && !(systemdStarted = is_systemd_startup_complete(bus)); waitCounts++) {
        usleep(timeoutUs); // 500 ms.
    }
    fflush(stdout);
    fflush(stderr);
    sd_bus_unref(bus);
    return systemdStarted;
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
    bool preserveDir = true;
    // If getcwd fails, we give up on keeping PWD.
    if (getcwd(currentDir, PATH_MAX) == NULL) {
        preserveDir = false;
    }

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
            char msg[80];
            snprintf(msg, 80, "Failed to open namespace file descriptor from path %s", nsPath);
            perror(msg);
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

    if (preserveDir) {
        if(chdir(currentDir)!=0){
            perror("Attempt to keep working directory");
        }
    }
    return 0;
}
