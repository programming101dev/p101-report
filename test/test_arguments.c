#include "cli.h"
#include "constants.h"
#include "errors.h"
#include "memory.h"
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
void                      p101_report_run_more_tests(void);

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
    char            *argv[] = {"p101-report", "run-report", NULL};
    struct arguments args;
    char             resources[PATH_MAX_BYTES];
    char             calls[PATH_MAX_BYTES];

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
    char            *argv[] = {"p101-report", "-j", "-r", "resources.log", "-c", "calls.log", NULL};
    struct arguments args;
    char             resources[PATH_MAX_BYTES];
    char             calls[PATH_MAX_BYTES];

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
    char            *argv[] = {"p101-report", "-m", "-r", "resources.log", "-c", "calls.log", NULL};
    struct arguments args;
    char             resources[PATH_MAX_BYTES];
    char             calls[PATH_MAX_BYTES];

    reset_getopt();
    p101_memset(env, &args, 0, sizeof(args));

    p101_report_parse_arguments(env, error, 6, argv, &args);
    p101_report_check_arguments(env, error, &args, resources, sizeof(resources), calls, sizeof(calls));

    TEST_ASSERT_FALSE(p101_error_has_error(error));
    TEST_ASSERT_EQUAL_INT(REPORT_FORMAT_MERMAID, args.format);
}

static void test_parse_rejects_missing_call_log(void)
{
    char            *argv[] = {"p101-report", "-r", "resources.log", NULL};
    struct arguments args;
    char             resources[PATH_MAX_BYTES];
    char             calls[PATH_MAX_BYTES];

    reset_getopt();
    p101_memset(env, &args, 0, sizeof(args));

    p101_report_parse_arguments(env, error, 3, argv, &args);
    p101_report_check_arguments(env, error, &args, resources, sizeof(resources), calls, sizeof(calls));

    TEST_ASSERT_TRUE(p101_error_is_error(error, P101_ERROR_USER, ERR_USAGE));
}

static void test_parse_resource_line_accepts_fd_open(void)
{
    char                  line[] = "P101FD\t4\t42\t7\t1\t100\t200\tOPEN\t3\t17\tmain\tserver.c\n";
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
    TEST_ASSERT_EQUAL_UINT(1, event.event_sequence);
    TEST_ASSERT_EQUAL_UINT(100, event.monotonic_ns);
    TEST_ASSERT_EQUAL_UINT(200, event.wall_unix_ns);
    TEST_ASSERT_EQUAL_STRING("server.c", model.sites[event.site].file_name);
    TEST_ASSERT_EQUAL_STRING("main", model.sites[event.site].function_name);
    TEST_ASSERT_EQUAL_INT(17, model.sites[event.site].line_number);

    p101_report_free_resource_event(env, &event);
    p101_report_free_model(env, &model);
}

static void test_parse_resource_line_rejects_old_fd_open(void)
{
    char                  line[] = "P101FD\t2\t42\t1\t100\t200\tOPEN\t3\t17\tmain\tserver.c\n";
    struct report_model   model;
    struct resource_event event;
    enum line_status      status;

    p101_memset(env, &model, 0, sizeof(model));
    p101_memset(env, &event, 0, sizeof(event));

    status = p101_report_parse_resource_line(env, error, line, &event, &model, 1);

    TEST_ASSERT_EQUAL_INT(LINE_BAD_VERSION, status);

    p101_report_free_resource_event(env, &event);
    p101_report_free_model(env, &model);
}

static void test_parse_resource_line_accepts_spawn_boundary(void)
{
    char                  line[] = "P101SPAWN\t4\t42\t1\t2\t110\t210\t43\t18\tp101_posix_spawn\tspawn.c\t/usr/bin/true\n";
    struct report_model   model;
    struct resource_event event;
    enum line_status      status;

    p101_memset(env, &model, 0, sizeof(model));
    p101_memset(env, &event, 0, sizeof(event));

    status = p101_report_parse_resource_line(env, error, line, &event, &model, 1);

    TEST_ASSERT_EQUAL_INT(LINE_OK, status);
    TEST_ASSERT_EQUAL_INT(RESOURCE_SPAWN, event.kind);
    TEST_ASSERT_EQUAL_INT64(42, event.pid);
    TEST_ASSERT_EQUAL_INT64(43, event.child_pid);
    TEST_ASSERT_EQUAL_STRING("/usr/bin/true", event.target);
    TEST_ASSERT_EQUAL_STRING("spawn.c", model.sites[event.site].file_name);
    TEST_ASSERT_EQUAL_STRING("p101_posix_spawn", model.sites[event.site].function_name);
    TEST_ASSERT_EQUAL_INT(18, model.sites[event.site].line_number);

    p101_report_ingest_resource(env, error, &model, &event);
    TEST_ASSERT_EQUAL_UINT64(1, model.resource_count);
    TEST_ASSERT_EQUAL_UINT64(0, model.fd_count);
    TEST_ASSERT_EQUAL_UINT64(0, model.finding_count);

    p101_report_free_resource_event(env, &event);
    p101_report_free_model(env, &model);
}

static void test_generic_resource_lifecycle_produces_source_backed_finding(void)
{
    char                  acquire_line[] = "P101RESOURCE\t4\t42\t7\t8\t100\t200\tACQUIRE\tmapping\t0x1000\t-\t4096\tprivate\t21\tmap_file\tmap.c\n";
    struct report_model   model;
    struct resource_event event;

    p101_memset(env, &model, 0, sizeof(model));
    p101_memset(env, &event, 0, sizeof(event));

    TEST_ASSERT_EQUAL_INT(LINE_OK, p101_report_parse_resource_line(env, error, acquire_line, &event, &model, 1));
    TEST_ASSERT_EQUAL_INT(RESOURCE_GENERIC, event.kind);
    TEST_ASSERT_EQUAL_INT(P101_TOOL_EVENT_RESOURCE_ACQUIRE, event.generic_kind);
    TEST_ASSERT_EQUAL_STRING("mapping", event.resource_class);
    TEST_ASSERT_EQUAL_STRING("0x1000", event.resource_id);
    TEST_ASSERT_EQUAL_UINT64(7, event.context_id);
    p101_report_ingest_resource(env, error, &model, &event);
    p101_report_free_resource_event(env, &event);

    p101_report_finalize_leaks(env, error, &model);

    TEST_ASSERT_FALSE(p101_error_has_error(error));
    TEST_ASSERT_EQUAL_UINT64(1, model.finding_count);
    TEST_ASSERT_EQUAL_INT(FINDING_RESOURCE_LEAK, model.findings[0].kind);
    TEST_ASSERT_EQUAL_STRING("mapping", model.findings[0].resource_class);
    TEST_ASSERT_EQUAL_STRING("0x1000", model.findings[0].resource_id);
    TEST_ASSERT_EQUAL_UINT64(7, model.findings[0].context_id);
    TEST_ASSERT_EQUAL_UINT64(1, model.findings[0].sequence);
    TEST_ASSERT_EQUAL_STRING("map.c", model.sites[model.findings[0].site].file_name);
    TEST_ASSERT_EQUAL_STRING("map_file", model.sites[model.findings[0].site].function_name);
    TEST_ASSERT_EQUAL_INT(21, model.sites[model.findings[0].site].line_number);

    p101_report_free_model(env, &model);
}

static void test_generic_resource_balanced_lifecycle_has_no_finding(void)
{
    char                  acquire_line[] = "P101RESOURCE\t4\t42\t7\t8\t100\t200\tACQUIRE\tmapping\t0x1000\t-\t4096\tprivate\t21\tmap_file\tmap.c\n";
    char                  release_line[] = "P101RESOURCE\t4\t42\t8\t9\t110\t210\tRELEASE\tmapping\t0x1000\t-\t0\t-\t22\tunmap_file\tmap.c\n";
    struct report_model   model;
    struct resource_event event;

    p101_memset(env, &model, 0, sizeof(model));
    p101_memset(env, &event, 0, sizeof(event));

    TEST_ASSERT_EQUAL_INT(LINE_OK, p101_report_parse_resource_line(env, error, acquire_line, &event, &model, 1));
    p101_report_ingest_resource(env, error, &model, &event);
    p101_report_free_resource_event(env, &event);

    p101_memset(env, &event, 0, sizeof(event));
    TEST_ASSERT_EQUAL_INT(LINE_OK, p101_report_parse_resource_line(env, error, release_line, &event, &model, 2));
    p101_report_ingest_resource(env, error, &model, &event);
    p101_report_free_resource_event(env, &event);

    p101_report_finalize_leaks(env, error, &model);

    TEST_ASSERT_FALSE(p101_error_has_error(error));
    TEST_ASSERT_EQUAL_UINT64(0, model.finding_count);
    p101_report_free_model(env, &model);
}

static void test_parse_call_line_accepts_exit(void)
{
    char                line[] = "P101CALL\t4\t42\t7\t1\t100\t200\tEXIT\t17\tmain\tp101_open\t-\t3\tserver.c\n";
    struct call_event   event;
    struct report_model model;
    enum line_status    status;

    p101_memset(env, &event, 0, sizeof(event));
    p101_memset(env, &model, 0, sizeof(model));

    status = p101_report_parse_call_line(env, error, line, &event, &model, 1);

    TEST_ASSERT_EQUAL_INT(LINE_OK, status);
    TEST_ASSERT_EQUAL_INT64(42, event.pid);
    TEST_ASSERT_EQUAL_INT(CALL_EXIT, event.kind);
    TEST_ASSERT_EQUAL_UINT(1, event.event_sequence);
    TEST_ASSERT_EQUAL_UINT(100, event.monotonic_ns);
    TEST_ASSERT_EQUAL_UINT(200, event.wall_unix_ns);
    TEST_ASSERT_EQUAL_STRING("p101_open", event.call_name);
    TEST_ASSERT_EQUAL_STRING("3", event.result);

    p101_report_free_call_event(env, &event);
    p101_report_free_model(env, &model);
}

static void test_parse_call_line_rejects_old_exit(void)
{
    char                line[] = "P101CALL\t2\t42\t1\t100\t200\tEXIT\t17\tmain\tp101_open\t-\t3\tserver.c\n";
    struct call_event   event;
    struct report_model model;
    enum line_status    status;

    p101_memset(env, &event, 0, sizeof(event));
    p101_memset(env, &model, 0, sizeof(model));

    status = p101_report_parse_call_line(env, error, line, &event, &model, 1);

    TEST_ASSERT_EQUAL_INT(LINE_BAD_VERSION, status);

    p101_report_free_call_event(env, &event);
    p101_report_free_model(env, &model);
}

static void test_failed_exec_removes_only_its_inheritance_findings(void)
{
    char                  open_line[] = "P101FD\t4\t42\t1\t1\t100\t200\tOPEN\t3\t17\tmain\tserver.c\n";
    char                  exec_line[] = "P101EXEC\t4\t42\t1\t2\t110\t210\t3\t0\t18\tmain\tserver.c\tmissing\n";
    char                  fail_line[] = "P101EXECFAIL\t4\t42\t1\t3\t120\t220\t18\tmain\tserver.c\tmissing\n";
    struct report_model   model;
    struct resource_event event;

    p101_memset(env, &model, 0, sizeof(model));
    p101_memset(env, &event, 0, sizeof(event));

    TEST_ASSERT_EQUAL_INT(LINE_OK, p101_report_parse_resource_line(env, error, open_line, &event, &model, 1));
    p101_report_ingest_resource(env, error, &model, &event);
    p101_report_free_resource_event(env, &event);

    p101_memset(env, &event, 0, sizeof(event));
    TEST_ASSERT_EQUAL_INT(LINE_OK, p101_report_parse_resource_line(env, error, exec_line, &event, &model, 2));
    p101_report_ingest_resource(env, error, &model, &event);
    p101_report_free_resource_event(env, &event);
    TEST_ASSERT_EQUAL_UINT64(1, model.finding_count);

    p101_memset(env, &event, 0, sizeof(event));
    TEST_ASSERT_EQUAL_INT(LINE_OK, p101_report_parse_resource_line(env, error, fail_line, &event, &model, 3));
    p101_report_ingest_resource(env, error, &model, &event);
    p101_report_free_resource_event(env, &event);

    TEST_ASSERT_FALSE(p101_error_has_error(error));
    TEST_ASSERT_EQUAL_UINT64(0, model.finding_count);
    TEST_ASSERT_EQUAL_UINT64(1, model.fd_count);
    p101_report_free_model(env, &model);
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
    static const char   bytes[] = {'P', '1', '0', '1', 'F', 'D', '\t', '1', '\t', '4', '2', '\0', '\t', 'O', 'P', 'E', 'N', '\t', '3', '\t', '1', '7', '\t', 'm', 'a', 'i', 'n', '\t', 's', 'e', 'r', 'v', 'e', 'r', '.', 'c', '\n'};
    char                path[PATH_MAX_BYTES];
    struct report_model model;

    p101_memset(env, &model, 0, sizeof(model));
    write_temp_bytes(path, sizeof(path), bytes, sizeof(bytes));

    p101_report_read_resources(env, error, path, &model);

    TEST_ASSERT_FALSE(p101_error_has_error(error));
    TEST_ASSERT_EQUAL_UINT64(0, model.resource_records);
    TEST_ASSERT_EQUAL_UINT64(1, model.resource_malformed);

    p101_unlink(env, error, path);
    p101_report_free_model(env, &model);
}

static void test_call_reader_counts_embedded_nul_as_malformed(void)
{
    static const char   bytes[] = {'P', '1', '0', '1', 'C', 'A',  'L', 'L',  '\t', '4', '\t', '4', '2',  '\0', '\t', '1', '\t', '1', '0', '0',  '\t', '2',  '0', '0', '\t',
                                   'E', 'N', 'T', 'E', 'R', '\t', '1', '\t', 'm',  'a', 'i',  'n', '\t', 'm',  'a',  'i', 'n',  '.', 'c', '\t', '-',  '\t', '-', '\n'};
    char                path[PATH_MAX_BYTES];
    struct report_model model;

    p101_memset(env, &model, 0, sizeof(model));
    write_temp_bytes(path, sizeof(path), bytes, sizeof(bytes));

    p101_report_read_calls(env, error, path, &model);

    TEST_ASSERT_FALSE(p101_error_has_error(error));
    TEST_ASSERT_EQUAL_UINT64(0, model.call_records);
    TEST_ASSERT_EQUAL_UINT64(1, model.call_malformed);

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
    RUN_TEST(test_parse_resource_line_rejects_old_fd_open);
    RUN_TEST(test_parse_resource_line_accepts_spawn_boundary);
    RUN_TEST(test_generic_resource_lifecycle_produces_source_backed_finding);
    RUN_TEST(test_generic_resource_balanced_lifecycle_has_no_finding);
    RUN_TEST(test_parse_call_line_accepts_exit);
    RUN_TEST(test_parse_call_line_rejects_old_exit);
    RUN_TEST(test_failed_exec_removes_only_its_inheritance_findings);
    RUN_TEST(test_resource_reader_counts_embedded_nul_as_malformed);
    RUN_TEST(test_call_reader_counts_embedded_nul_as_malformed);
    p101_report_run_more_tests();
    return UNITY_END();
}
