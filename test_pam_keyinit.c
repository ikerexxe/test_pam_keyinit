/*
 * Copyright (c) 2022 Iker Pedrosa <ipedrosa@redhat.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <stddef.h>
#include <stdarg.h>
#include <stdio.h>
#include <setjmp.h>
#include <pthread.h>
#include <sys/syscall.h>
#include <unistd.h>
#include <pwd.h>
#include <errno.h>

#include <cmocka.h>
#include <libpamtest.h>

#define MAX_USERNAME_SIZE	256
#define MAX_THREADS 		2
#define SERVICE_NAME		"myapp"

static void test_pam_session(void **state)
{
    int ret;
    char username[MAX_USERNAME_SIZE];
    enum pamtest_err perr;
    struct pam_testcase tests[] = {
        pam_test(PAMTEST_OPEN_SESSION, PAM_SUCCESS),
        pam_test(PAMTEST_GETENVLIST, PAM_SUCCESS),
        pam_test(PAMTEST_CLOSE_SESSION, PAM_SUCCESS),
        pam_test(PAMTEST_GETENVLIST, PAM_SUCCESS),
    };

    ret = getlogin_r(username, MAX_USERNAME_SIZE);
    assert_int_equal(ret, 0);

    perr = run_pamtest(SERVICE_NAME, username, NULL, tests, NULL);

    assert_int_equal(perr, PAMTEST_ERR_OK);
}

static int pam_setreuid(uid_t ruid, uid_t euid)
{
#if defined(HAVE_LINUX_32BIT_SYSCALLS)
    return syscall(SYS_setreuid32, ruid, euid);
#else
    return syscall(SYS_setreuid, ruid, euid);
#endif
}

static int pam_setregid(gid_t rgid, gid_t egid)
{
#if defined(HAVE_LINUX_32BIT_SYSCALLS)
    return syscall(SYS_setregid32, rgid, egid);
#else
    return syscall(SYS_setregid, rgid, egid);
#endif
}

static void *change_uids_and_sleep(void *param)
{
    int ret;

    ret = sleep(3);

    *(int*)param = errno;
}

static void *open_session(void *param)
{
    int ret;
    char username[MAX_USERNAME_SIZE];
    struct passwd *pw;
    enum pamtest_err perr;
    struct pam_testcase tests[] = {
        pam_test(PAMTEST_OPEN_SESSION, PAM_SUCCESS),
    };

    pw = getpwnam("nobody");
    assert_non_null(pw);
    ret = getlogin_r(username, MAX_USERNAME_SIZE);
    assert_int_equal(ret, 0);
    ret = pam_setregid(pw->pw_uid, -1);
    assert_int_equal(ret, 0);
    ret = pam_setreuid(pw->pw_gid, -1);
    assert_int_equal(ret, 0);

    perr = run_pamtest(SERVICE_NAME, username, NULL, tests, NULL);

    *(int*)param = perr;
}

static void test_thread_pam_session(void **state)
{
    int i;
    pthread_t thread_id[MAX_THREADS];
    int ret;

    for (i = 0; i < MAX_THREADS; i++) {
        if (i == 0) {
            pthread_create(&thread_id[i], NULL, change_uids_and_sleep, &ret);
        } else {
            pthread_create(&thread_id[i], NULL, open_session, &ret);
        }
    }

    for (i = 0; i < MAX_THREADS; i++) {
        pthread_join(thread_id[i], NULL);
        if (i == 0) {
            assert_int_not_equal(ret, EINTR);
        } else {
            assert_int_equal(ret, PAMTEST_ERR_OK);
        }
    }
}

int main(int argc, const char *argv[])
{
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_pam_session),
        cmocka_unit_test(test_thread_pam_session)
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
