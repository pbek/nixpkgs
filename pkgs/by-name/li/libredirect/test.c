#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <spawn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>

#define TESTDIR "/bar/baz"
#define TESTPATH "/foo/bar/test"
#define SUBTEST "./test sub"

extern char **environ;

void test_spawn(void) {
    pid_t pid;
    int ret;
    posix_spawn_file_actions_t file_actions;
    char *argv[] = {"true", NULL};

    assert(posix_spawn_file_actions_init(&file_actions) == 0);

    ret = posix_spawn(&pid, TESTPATH, &file_actions, NULL, argv, environ);

    assert(ret == 0);
    assert(waitpid(pid, NULL, 0) != -1);
}

void test_execv(void) {
    char *argv[] = {"true", NULL};
    assert(execv(TESTPATH, argv) == 0);
}

void test_system(void) {
    assert(system(TESTPATH) == 0);
}

void test_subprocess(void) {
    assert(system(SUBTEST) == 0);
}

void test_stat_with_null_path(void) {
    // This checks whether the compiler optimizes away the null pointer check
    // on the path passed to stat(). If that's the case, the following code
    // should segfault.
    struct stat buf;
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wnonnull"
    stat(NULL, &buf);
#pragma GCC diagnostic pop
}

void assert_mktemp_path(
    const char * orig_prefix,
    const char * orig_suffix,
    const char * updated
) {
    // prefix unchanged
    assert(strncmp(updated, orig_prefix, strlen(orig_prefix)) == 0);
    // wildcards replaced
    assert(strcmp(updated + strlen(orig_prefix), "XXXXXX") != 0);
    // suffix unchanged
    assert(strcmp(updated + strlen(orig_prefix) + 6, orig_suffix) == 0);
}

int main(int argc, char *argv[])
{
    FILE *testfp;
    int testfd;
    struct stat testsb;
#ifdef __GLIBC__
    struct stat64 testsb64;
#endif
#if defined(__linux__) && defined(STATX_TYPE)
    struct statx testsbx;
#endif
    char buf[PATH_MAX];

    testfp = fopen(TESTPATH, "r");
    assert(testfp != NULL);
    fclose(testfp);

    testfd = open(TESTPATH, O_RDONLY);
    assert(testfd != -1);
    close(testfd);

    assert(access(TESTPATH, X_OK) == 0);

    // On EOVERFLOW checks below: when TESTPATH lands on a filesystem
    // that requires 64-bit inode values (like btrfs used for a while)
    // it will fail on 32-bit systems.

    assert(stat(TESTPATH, &testsb) != -1 || errno == EOVERFLOW);
#ifdef __GLIBC__
    assert(stat64(TESTPATH, &testsb64) != -1);
#endif
    assert(fstatat(123, TESTPATH, &testsb, 0) != -1 || errno == EOVERFLOW);
#ifdef __GLIBC__
    assert(fstatat64(123, TESTPATH, &testsb64, 0) != -1);
#endif
#if defined(__linux__) && defined(STATX_TYPE)
    assert(statx(123, TESTPATH, 0, STATX_ALL, &testsbx) != -1);
#endif

    assert(getcwd(buf, PATH_MAX) != NULL);
    assert(chdir(TESTDIR) == 0);
    assert(chdir(buf) == 0);

    assert(mkdir(TESTDIR "/dir-mkdir", 0777) == 0);
    assert(unlink(TESTDIR "/dir-mkdir") == -1); // it's a directory!
#ifndef __APPLE__
    assert(errno == EISDIR);
#endif
    assert(rmdir(TESTDIR "/dir-mkdir") == 0);
    assert(unlink(TESTDIR "/dir-mkdir") == -1);
    assert(errno == ENOENT);

    assert(mkdirat(123, TESTDIR "/dir-mkdirat", 0777) == 0);
    assert(unlinkat(123, TESTDIR "/dir-mkdirat", 0) == -1); // it's a directory!
#ifndef __APPLE__
    assert(errno == EISDIR);
#endif
    assert(unlinkat(123, TESTDIR "/dir-mkdirat", AT_REMOVEDIR) == 0);

    strncpy(buf, TESTDIR "/tempXXXXXX", PATH_MAX);
    testfd = mkstemp(buf);
    assert(testfd > 0);
    assert_mktemp_path(TESTDIR "/temp", "", buf);
    close(testfd);

    strncpy(buf, TESTDIR "/tempXXXXXX", PATH_MAX);
    testfd = mkostemp(buf, 0);
    assert(testfd > 0);
    assert_mktemp_path(TESTDIR "/temp", "", buf);
    close(testfd);

    strncpy(buf, TESTDIR "/tempXXXXXX.test", PATH_MAX);
    testfd = mkstemps(buf, strlen(".test"));
    assert(testfd > 0);
    assert_mktemp_path(TESTDIR "/temp", ".test", buf);
    close(testfd);

    strncpy(buf, TESTDIR "/tempXXXXXX.test", PATH_MAX);
    testfd = mkostemps(buf, strlen(".test"), 0);
    assert(testfd > 0);
    assert_mktemp_path(TESTDIR "/temp", ".test", buf);
    close(testfd);

    strncpy(buf, TESTDIR "/tempXXXXXX", PATH_MAX);
    assert(mkdtemp(buf) == buf);
    assert_mktemp_path(TESTDIR "/temp", "", buf);

    strncpy(buf, TESTDIR "/tempXXXXXX", PATH_MAX);
    assert(mktemp(buf) == buf);
    assert_mktemp_path(TESTDIR "/temp", "", buf);

    test_spawn();
    test_system();
    test_stat_with_null_path();

    // Only run subprocess if no arguments are given
    // as the subprocess will be called without argument
    // otherwise we will have infinite recursion
    if (argc == 1) {
        test_subprocess();
    }

    test_execv();

    /* If all goes well, this is never reached because test_execv() replaces
     * the current process.
     */
    return 0;
}
