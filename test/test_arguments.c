#include "cli.h"
#include "constants.h"
#include "errors.h"
#include "model.h"
#include "parse.h"
#include "reader.h"
#include "types.h"
#include "unity.h"
#include <p101_c/p101_stdio.h>
#include <p101_c/p101_string.h>
#include <p101_env/env.h>
#include <p101_error/error.h>
#include <p101_posix/p101_stdio.h>
#include <p101_posix/p101_stdlib.h>
#include <p101_posix/p101_unistd.h>
#include <stdbool.h>
#include <stdio.h>

static struct p101_error *error;
static struct p101_env   *env;

void setUp(void)
{
    error = p101_error_create(false);
    env   = p101_env_create(error, NULL);
}

void tearDown(void)
{
    p101_env_destroy(env);
    p101_error_destroy(error);
}

static void reset_getopt(void)
{
#ifdef __GLIBC__
    optind = 0;
#else
    extern int optreset;
    optreset = 1;
    optind   = 1;
#endif
}

static void test_parse_accepts_report_directory(void)
{
    char             *argv[] = {"p101-report", "run-report", NULL};
    struct arguments  args;
    char              resources[PATH_MAX_BYTES];
    char              calls[PATH_MAX_BYTES];

    reset_getopt();
    p101_memset(env, &args, 0, sizeof(args));

    p101_report_parse_arguments(env, error, 2, argv, &args);
    p101_report_check_arguments(env, error, &args, resources, sizeof(resources), calls, sizeof(calls));

    TEST_ASSERT_FALSE(p101_error_has_error(error));
    TEST_ASSERT_EQUAL_STRING("run-report", args.report_dir);
    TEST_ASSERT_EQUAL_STRING("run-report/resources.log", args.resource_log);
    TEST_ASSERT_EQUAL_STRING("run-report/calls.log", args.call_log);
}

static void test_parse_accepts_explicit_logs(void)
{
    char             *argv[] = {"p101-report", "-j", "-r", "resources.log", "-c", "calls.log", NULL};
    struct arguments  args;
    char              resources[PATH_MAX_BYTES];
    char              calls[PATH_MAX_BYTES];

    reset_getopt();
    p101_memset(env, &args, 0, sizeof(args));

    p101_report_parse_arguments(env, error, 6, argv, &args);
    p101_report_check_arguments(env, error, &args, resources, sizeof(resources), calls, sizeof(calls));

    TEST_ASSERT_FALSE(p101_error_has_error(error));
    TEST_ASSERT_NULL(args.report_dir);
    TEST_ASSERT_EQUAL_INT(REPORT_FORMAT_JSON, args.format);
    TEST_ASSERT_EQUAL_STRING("resources.log", args.resource_log);
    TEST_ASSERT_EQUAL_STRING("calls.log", args.call_log);
}

static void test_parse_accepts_mermaid_output(void)
{
    char             *argv[] = {"p101-report", "-m", "-r", "resources.log", "-c", "calls.log", NULL};
    struct arguments  args;
    char              resources[PATH_MAX_BYTES];
    char              calls[PATH_MAX_BYTES];

    reset_getopt();
    p101_memset(env, &args, 0, sizeof(args));

    p101_report_parse_arguments(env, error, 6, argv, &args);
    p101_report_check_arguments(env, error, &args, resources, sizeof(resources), calls, sizeof(calls));

    TEST_ASSERT_FALSE(p101_error_has_error(error));
    TEST_ASSERT_EQUAL_INT(REPORT_FORMAT_MERMAID, args.format);
}

static void test_parse_rejects_missing_call_log(void)
{
    char             *argv[] = {"p101-report", "-r", "resources.log", NULL};
    struct arguments  args;
    char              resources[PATH_MAX_BYTES];
    char              calls[PATH_MAX_BYTES];

    reset_getopt();
    p101_memset(env, &args, 0, sizeof(args));

    p101_report_parse_arguments(env, error, 3, argv, &args);
    p101_report_check_arguments(env, error, &args, resources, sizeof(resources), calls, sizeof(calls));

    TEST_ASSERT_TRUE(p101_error_is_error(error, P101_ERROR_USER, ERR_USAGE));
}

static void test_parse_resource_line_accepts_fd_open(void)
{
    char                  line[] = "P101FD\t1\t42\tOPEN\t3\t17\tmain\tserver.c\n";
    struct report_model   model;
    struct resource_event event;
    enum line_status      status;

    p101_memset(env, &model, 0, sizeof(model));
    p101_memset(env, &event, 0, sizeof(event));

    status = p101_report_parse_resource_line(env, error, line, &event, &model, 1);

    TEST_ASSERT_EQUAL_INT(LINE_OK, status);
    TEST_ASSERT_EQUAL_INT64(42, event.pid);
    TEST_ASSERT_EQUAL_INT(RESOURCE_FD_OPEN, event.kind);
    TEST_ASSERT_EQUAL_INT(3, event.fd);
    TEST_ASSERT_EQUAL_STRING("server.c", model.sites[event.site].file_name);
    TEST_ASSERT_EQUAL_STRING("main", model.sites[event.site].function_name);
    TEST_ASSERT_EQUAL_INT(17, model.sites[event.site].line_number);

    p101_report_free_resource_event(env, &event);
    p101_report_free_model(env, &model);
}

static void test_parse_resource_line_accepts_v2_fd_open(void)
{
    char                  line[] = "P101FD\t2\t42\t1\t100\t200\tOPEN\t3\t17\tmain\tserver.c\n";
    struct report_model   model;
    struct resource_event event;
    enum line_status      status;

    p101_memset(env, &model, 0, sizeof(model));
    p101_memset(env, &event, 0, sizeof(event));

    status = p101_report_parse_resource_line(env, error, line, &event, &model, 1);

    TEST_ASSERT_EQUAL_INT(LINE_OK, status);
    TEST_ASSERT_EQUAL_INT64(42, event.pid);
    TEST_ASSERT_EQUAL_INT(RESOURCE_FD_OPEN, event.kind);
    TEST_ASSERT_EQUAL_INT(3, event.fd);
    TEST_ASSERT_EQUAL_STRING("server.c", model.sites[event.site].file_name);

    p101_report_free_resource_event(env, &event);
    p101_report_free_model(env, &model);
}

static void test_parse_call_line_accepts_exit(void)
{
    char              line[] = "P101CALL\t1\t42\tEXIT\t17\tmain\tp101_open\t-\t3\tserver.c\n";
    struct call_event event;
    enum line_status  status;

    p101_memset(env, &event, 0, sizeof(event));

    status = p101_report_parse_call_line(env, error, line, &event, 1);

    TEST_ASSERT_EQUAL_INT(LINE_OK, status);
    TEST_ASSERT_EQUAL_INT64(42, event.pid);
    TEST_ASSERT_EQUAL_INT(CALL_EXIT, event.kind);
    TEST_ASSERT_EQUAL_STRING("p101_open", event.call_name);
    TEST_ASSERT_EQUAL_STRING("3", event.result);

    p101_report_free_call_event(env, &event);
}

static void test_parse_call_line_accepts_v2_exit(void)
{
    char              line[] = "P101CALL\t2\t42\t1\t100\t200\tEXIT\t17\tmain\tp101_open\t-\t3\tserver.c\n";
    struct call_event event;
    enum line_status  status;

    p101_memset(env, &event, 0, sizeof(event));

    status = p101_report_parse_call_line(env, error, line, &event, 1);

    TEST_ASSERT_EQUAL_INT(LINE_OK, status);
    TEST_ASSERT_EQUAL_INT64(42, event.pid);
    TEST_ASSERT_EQUAL_INT(CALL_EXIT, event.kind);
    TEST_ASSERT_EQUAL_STRING("p101_open", event.call_name);
    TEST_ASSERT_EQUAL_STRING("3", event.result);

    p101_report_free_call_event(env, &event);
}

static void write_temp_bytes(char *path, size_t path_size, const char *bytes, size_t byte_count)
{
    FILE *stream;
    int   fd;

    p101_strncpy(env, path, "/tmp/p101-report-test-XXXXXX", path_size);
    path[path_size - 1U] = '\0';

    fd = p101_mkstemp(env, error, path);
    TEST_ASSERT_FALSE(p101_error_has_error(error));
    TEST_ASSERT_NOT_EQUAL(-1, fd);

    stream = p101_fdopen(env, error, fd, "wb");
    TEST_ASSERT_FALSE(p101_error_has_error(error));
    TEST_ASSERT_NOT_NULL(stream);

    TEST_ASSERT_EQUAL_UINT(byte_count, p101_fwrite(env, error, bytes, 1U, byte_count, stream));
    TEST_ASSERT_FALSE(p101_error_has_error(error));

    p101_fclose(env, error, stream);
    TEST_ASSERT_FALSE(p101_error_has_error(error));
}

static void test_resource_reader_counts_embedded_nul_as_malformed(void)
{
    static const char      bytes[] = {'P', '1', '0', '1', 'F', 'D', '\t', '1', '\t', '4', '2', '\0', '\t', 'O', 'P', 'E', 'N', '\t', '3', '\t', '1', '7', '\t', 'm', 'a', 'i', 'n', '\t', 's', 'e', 'r', 'v', 'e', 'r', '.', 'c', '\n'};
    char                   path[PATH_MAX_BYTES];
    struct report_model    model;

    p101_memset(env, &model, 0, sizeof(model));
    write_temp_bytes(path, sizeof(path), bytes, sizeof(bytes));

    p101_report_read_resources(env, error, path, &model);

    TEST_ASSERT_FALSE(p101_error_has_error(error));
    TEST_ASSERT_EQUAL_UINT64(0, model.resource_records);
    TEST_ASSERT_EQUAL_UINT64(1, model.resource_malformed);

    p101_unlink(env, error, path);
    p101_report_free_model(env, &model);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_parse_accepts_report_directory);
    RUN_TEST(test_parse_accepts_explicit_logs);
    RUN_TEST(test_parse_accepts_mermaid_output);
    RUN_TEST(test_parse_rejects_missing_call_log);
    RUN_TEST(test_parse_resource_line_accepts_fd_open);
    RUN_TEST(test_parse_resource_line_accepts_v2_fd_open);
    RUN_TEST(test_parse_call_line_accepts_exit);
    RUN_TEST(test_parse_call_line_accepts_v2_exit);
    RUN_TEST(test_resource_reader_counts_embedded_nul_as_malformed);
    return UNITY_END();
}
