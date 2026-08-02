#include "arguments.h"
#include "constants.h"
#include "finding.h"
#include "io.h"
#include "json.h"
#include "lifetime.h"
#include "lifetime_common.h"
#include "lifetime_mermaid.h"
#include "memory.h"
#include "model.h"
#include "model_generic.h"
#include "model_support.h"
#include "output.h"
#include "parse.h"
#include "types.h"
#include "unity.h"
#include <p101_c/p101_stdio.h>
#include <p101_c/p101_stdlib.h>
#include <p101_c/p101_string.h>
#include <p101_env/env.h>
#include <p101_error/error.h>
#include <p101_tool_event/event.h>
#include <stdint.h>

enum line_status             p101_report_test_map_parse_status(p101_tool_event_parse_status status);
const char                  *p101_report_test_generic_operation_name(p101_tool_event_resource_kind kind);
enum finding_kind            p101_report_test_generic_finding_kind(const struct p101_env *env, p101_tool_event_lifecycle_finding_kind kind, char *resource_class);
void                         p101_report_test_set_generic_fault(int fault);
void                         p101_report_test_set_grow_failure_countdown(size_t countdown);
void                         p101_report_test_rollback_failed_exec(const struct p101_env *env, struct report_model *model, const struct resource_event *event);
size_t                       p101_report_test_find_generic_sequence(const struct p101_env *env, const struct report_model *model, const struct p101_tool_event_lifecycle_finding *finding, bool previous);
const struct resource_event *p101_report_test_find_lifecycle_event(const struct report_model *model, const struct p101_tool_event_lifecycle_finding *finding);
bool                         p101_report_test_exec_attempt_matches(const struct p101_env *env, const struct resource_event *candidate, const struct resource_event *event);
bool                         p101_report_test_exec_finding_matches(const struct p101_env *env, const struct finding *finding, const struct resource_event *event, size_t first_sequence);

static struct p101_error *more_error;
static struct p101_env   *more_env;

static void more_setup(void)
{
    more_error = p101_error_create(false);
    more_env   = p101_env_create(more_error, NULL);
}

static void more_teardown(void)
{
    p101_env_destroy(more_env);
    p101_error_destroy(more_error);
}

static void test_finding_names_ids_and_site_matching(void)
{
    struct source_site site;
    struct call_event  call;

    for(int kind = FINDING_FD_LEAK; kind <= FINDING_RESOURCE_DUPLICATE_ACQUIRE; kind++)
    {
        TEST_ASSERT_NOT_NULL(p101_report_finding_name((enum finding_kind)kind));
        TEST_ASSERT_NOT_NULL(p101_report_finding_id((enum finding_kind)kind));
    }
    TEST_ASSERT_EQUAL_STRING("unknown finding", p101_report_finding_name((enum finding_kind)999));
    TEST_ASSERT_EQUAL_STRING("P101-UNKNOWN-000", p101_report_finding_id((enum finding_kind)999));

    p101_memset(more_env, &site, 0, sizeof(site));
    p101_memset(more_env, &call, 0, sizeof(call));
    site.file_name     = "same.c";
    site.function_name = "same_function";
    site.line_number   = 10;
    call.file_name     = "same.c";
    call.function_name = "same_function";
    call.line_number   = 10;
    call.pid           = 1;
    call.context_id    = 2U;
    TEST_ASSERT_FALSE(p101_report_site_matches_call(more_env, &site, &call, 2, 2U));
    TEST_ASSERT_FALSE(p101_report_site_matches_call(more_env, &site, &call, 1, 3U));
    TEST_ASSERT_TRUE(p101_report_site_matches_call(more_env, &site, &call, 1, 2U));
    call.line_number = 11;
    TEST_ASSERT_TRUE(p101_report_site_matches_call(more_env, &site, &call, 1, 2U));
    call.function_name = "other";
    TEST_ASSERT_FALSE(p101_report_site_matches_call(more_env, &site, &call, 1, 2U));
    call.line_number = 10;
    call.file_name   = "other.c";
    TEST_ASSERT_FALSE(p101_report_site_matches_call(more_env, &site, &call, 1, 2U));
    call.line_number = 11;
    TEST_ASSERT_FALSE(p101_report_site_matches_call(more_env, &site, &call, 1, 2U));
}

static void test_io_json_and_memory_helpers(void)
{
    char   path[64];
    char  *copy;
    FILE  *stream;
    bool   owned;
    void  *items;
    size_t capacity;

    p101_report_join_path(more_env, more_error, path, sizeof(path), "/tmp/", "x");
    TEST_ASSERT_EQUAL_STRING("/tmp/x", path);
    p101_report_join_path(more_env, more_error, path, sizeof(path), "/tmp", "x");
    TEST_ASSERT_EQUAL_STRING("/tmp/x", path);
    stream = p101_report_open_input(more_env, more_error, "-", &owned);
    TEST_ASSERT_EQUAL_PTR(stdin, stream);
    TEST_ASSERT_FALSE(owned);
    TEST_ASSERT_EQUAL_INT(0, p101_report_close_if_owned(more_env, more_error, NULL, true));
    TEST_ASSERT_EQUAL_INT(0, p101_report_close_if_owned(more_env, more_error, stdin, false));
    p101_report_join_path(more_env, more_error, path, sizeof(path), "", "x");
    TEST_ASSERT_EQUAL_STRING("/x", path);

    stream = p101_tmpfile(more_env, more_error);
    p101_report_json_string(more_env, more_error, stream, NULL);
    p101_report_json_string(more_env, more_error, stream, "\"\\\n\r\t\1\200z");
    p101_fclose(more_env, more_error, stream);

    copy = p101_report_dup_text(more_env, more_error, NULL);
    TEST_ASSERT_EQUAL_STRING("", copy);
    p101_free(more_env, copy);

    items    = NULL;
    capacity = 0U;
    TEST_ASSERT_TRUE(p101_report_grow_array_internal(more_env, more_error, &items, &capacity, 0U, sizeof(int)));
    TEST_ASSERT_TRUE(p101_report_grow_array_internal(more_env, more_error, &items, &capacity, 0U, sizeof(int)));
    p101_free(more_env, items);
    items    = NULL;
    capacity = (SIZE_MAX / 2U) + 1U;
    TEST_ASSERT_FALSE(p101_report_grow_array_internal(more_env, more_error, &items, &capacity, capacity, 2U));
    TEST_ASSERT_TRUE(p101_error_has_error(more_error));
    p101_error_reset(more_error);
    capacity = 8U;
    TEST_ASSERT_FALSE(p101_report_grow_array_internal(more_env, more_error, &items, &capacity, capacity, SIZE_MAX));
    TEST_ASSERT_TRUE(p101_error_has_error(more_error));
    p101_error_reset(more_error);
    capacity = 8U;
    TEST_ASSERT_FALSE(p101_report_grow_array_internal(more_env, more_error, &items, &capacity, capacity, 0U));
    TEST_ASSERT_TRUE(p101_error_has_error(more_error));
}

static void test_parse_statuses_and_cross_stream_records(void)
{
    struct report_model   model;
    struct resource_event resource;
    struct call_event     call;
    char                  call_line[]         = "P101CALL\t5\ttest-run\t1\t1\t1\t10\t20\tENTER\t3\tf\tp101_open\tx\t-\ta.c\n";
    char                  fd_line[]           = "P101FD\t5\ttest-run\t1\t1\t2\t11\t21\tOPEN\t3\t3\tf\ta.c\n";
    char                  complete_resource[] = "P101COMPLETE\t5\ttest-run\t1\t1\t3\t12\t22\t2\t0\t0\n";
    char                  complete_call[]     = "P101COMPLETE\t5\ttest-run\t1\t1\t3\t12\t22\t2\t0\t0\n";
    char                  null_call[]         = "P101CALL\t5\ttest-run\t1\t1\t1\t10\t20\tENTER\t3\tf\tp101_open\tx\t-\ta.c\n";
    char                  rejected_fd_line[]  = "P101FD\t5\ttest-run\t1\t1\t2\t11\t21\tOPEN\t3\t3\tf\ta.c\n";
    char                  rejected_call_line[] = "P101CALL\t5\ttest-run\t1\t1\t1\t10\t20\tENTER\t3\tf\tp101_open\tx\t-\ta.c\n";
    char                  finished_fd_line[] = "P101FD\t5\ttest-run\t1\t1\t2\t11\t21\tOPEN\t3\t3\tf\ta.c\n";
    char                  finished_call_line[] = "P101CALL\t5\ttest-run\t1\t1\t1\t10\t20\tENTER\t3\tf\tp101_open\tx\t-\ta.c\n";

    p101_memset(more_env, &model, 0, sizeof(model));
    p101_memset(more_env, &resource, 0, sizeof(resource));
    p101_memset(more_env, &call, 0, sizeof(call));
    TEST_ASSERT_EQUAL_INT(LINE_MALFORMED, p101_report_parse_resource_line(more_env, more_error, NULL, &resource, &model, 1U));
    TEST_ASSERT_EQUAL_INT(LINE_MALFORMED, p101_report_parse_resource_line(more_env, more_error, fd_line, NULL, &model, 1U));
    TEST_ASSERT_EQUAL_INT(LINE_MALFORMED, p101_report_parse_resource_line(more_env, more_error, fd_line, &resource, NULL, 1U));
    TEST_ASSERT_EQUAL_INT(LINE_OTHER, p101_report_parse_resource_line(more_env, more_error, call_line, &resource, &model, 1U));
    TEST_ASSERT_EQUAL_INT(LINE_OTHER, p101_report_parse_call_line(more_env, more_error, fd_line, &call, &model, 1U));
    TEST_ASSERT_EQUAL_INT(LINE_MALFORMED, p101_report_parse_call_line(more_env, more_error, NULL, &call, &model, 1U));
    TEST_ASSERT_EQUAL_INT(LINE_MALFORMED, p101_report_parse_call_line(more_env, more_error, null_call, NULL, &model, 1U));
    TEST_ASSERT_EQUAL_INT(LINE_MALFORMED, p101_report_parse_call_line(more_env, more_error, null_call, &call, NULL, 1U));
    TEST_ASSERT_EQUAL_INT(LINE_COMPLETE, p101_report_parse_resource_line(more_env, more_error, complete_resource, &resource, &model, 1U));
    TEST_ASSERT_EQUAL_INT(LINE_COMPLETE, p101_report_parse_call_line(more_env, more_error, complete_call, &call, &model, 1U));
    TEST_ASSERT_EQUAL_INT(LINE_OTHER, p101_report_test_map_parse_status(P101_TOOL_EVENT_PARSE_OTHER));
    TEST_ASSERT_EQUAL_INT(LINE_OK, p101_report_test_map_parse_status(P101_TOOL_EVENT_PARSE_OK));
    TEST_ASSERT_EQUAL_INT(LINE_BAD_VERSION, p101_report_test_map_parse_status(P101_TOOL_EVENT_PARSE_BAD_VERSION));
    TEST_ASSERT_EQUAL_INT(LINE_MALFORMED, p101_report_test_map_parse_status(P101_TOOL_EVENT_PARSE_MALFORMED));
    TEST_ASSERT_EQUAL_INT(LINE_MALFORMED, p101_report_test_map_parse_status((p101_tool_event_parse_status)999));
    p101_report_free_model(more_env, &model);

    p101_memset(more_env, &model, 0, sizeof(model));
    model.run_model = p101_tool_model_create(more_error);
    TEST_ASSERT_NOT_NULL(model.run_model);
    TEST_ASSERT_EQUAL_INT(LINE_OK, p101_report_parse_resource_line(more_env, more_error, rejected_fd_line, &resource, &model, 1U));
    p101_report_free_model(more_env, &model);

    p101_memset(more_env, &model, 0, sizeof(model));
    model.run_model = p101_tool_model_create(more_error);
    TEST_ASSERT_NOT_NULL(model.run_model);
    TEST_ASSERT_EQUAL_INT(LINE_OK, p101_report_parse_call_line(more_env, more_error, rejected_call_line, &call, &model, 1U));
    p101_report_free_call_event(more_env, &call);
    p101_report_free_model(more_env, &model);

    p101_memset(more_env, &model, 0, sizeof(model));
    model.run_model = p101_tool_model_create(more_error);
    TEST_ASSERT_NOT_NULL(model.run_model);
    TEST_ASSERT_EQUAL_INT(0, p101_tool_model_finish(more_error, model.run_model));
    TEST_ASSERT_EQUAL_INT(LINE_MALFORMED, p101_report_parse_resource_line(more_env, more_error, finished_fd_line, &resource, &model, 1U));
    p101_error_reset(more_error);
    TEST_ASSERT_EQUAL_INT(LINE_MALFORMED, p101_report_parse_call_line(more_env, more_error, finished_call_line, &call, &model, 1U));
    p101_error_reset(more_error);
    p101_report_free_model(more_env, &model);
}

static void test_model_support_and_output_defaults(void)
{
    struct report_model model;
    struct source_site *site;
    struct finding      finding;
    struct call_event   call;
    struct call_event   other_call;
    struct call_event   exit_call;
    struct call_event   open_call;
    struct arguments    args;
    size_t              first;

    p101_memset(more_env, &model, 0, sizeof(model));
    p101_memset(more_env, &finding, 0, sizeof(finding));
    p101_memset(more_env, &call, 0, sizeof(call));
    p101_memset(more_env, &other_call, 0, sizeof(other_call));
    p101_memset(more_env, &exit_call, 0, sizeof(exit_call));
    p101_memset(more_env, &open_call, 0, sizeof(open_call));
    p101_memset(more_env, &args, 0, sizeof(args));
    first = p101_report_intern_site(more_env, more_error, &model, "a.c", "f", 1);
    TEST_ASSERT_EQUAL_size_t(first, p101_report_intern_site(more_env, more_error, &model, "a.c", "f", 1));
    TEST_ASSERT_NOT_EQUAL(first, p101_report_intern_site(more_env, more_error, &model, "a.c", "g", 1));
    TEST_ASSERT_NOT_EQUAL(first, p101_report_intern_site(more_env, more_error, &model, "b.c", "f", 1));
    site = &model.sites[first];

    call.pid            = 1;
    call.context_id     = 1U;
    call.sequence       = 1U;
    call.event_sequence = 1U;
    call.kind           = CALL_ENTER;
    call.line_number    = site->line_number;
    call.function_name  = p101_report_dup_text(more_env, more_error, site->function_name);
    call.call_name      = p101_report_dup_text(more_env, more_error, "call");
    call.arguments      = p101_report_dup_text(more_env, more_error, "arg");
    call.result         = p101_report_dup_text(more_env, more_error, "-");
    call.file_name      = p101_report_dup_text(more_env, more_error, site->file_name);
    p101_report_add_call(more_env, more_error, &model, &call);
    other_call                = call;
    other_call.pid            = 2;
    other_call.kind           = CALL_ENTER;
    other_call.sequence       = 2U;
    other_call.event_sequence = 2U;
    other_call.function_name  = p101_report_dup_text(more_env, more_error, call.function_name);
    other_call.call_name      = p101_report_dup_text(more_env, more_error, call.call_name);
    other_call.arguments      = p101_report_dup_text(more_env, more_error, call.arguments);
    other_call.result         = p101_report_dup_text(more_env, more_error, call.result);
    other_call.file_name      = p101_report_dup_text(more_env, more_error, call.file_name);
    p101_report_add_call(more_env, more_error, &model, &other_call);
    exit_call                = call;
    exit_call.kind           = CALL_EXIT;
    exit_call.sequence       = 3U;
    exit_call.event_sequence = 3U;
    exit_call.function_name  = p101_report_dup_text(more_env, more_error, call.function_name);
    exit_call.call_name      = p101_report_dup_text(more_env, more_error, call.call_name);
    exit_call.arguments      = p101_report_dup_text(more_env, more_error, call.arguments);
    exit_call.result         = p101_report_dup_text(more_env, more_error, call.result);
    exit_call.file_name      = p101_report_dup_text(more_env, more_error, call.file_name);
    p101_report_add_call(more_env, more_error, &model, &exit_call);
    open_call                = call;
    open_call.kind           = CALL_ENTER;
    open_call.sequence       = 4U;
    open_call.event_sequence = 4U;
    open_call.function_name  = p101_report_dup_text(more_env, more_error, call.function_name);
    open_call.call_name      = p101_report_dup_text(more_env, more_error, "open");
    open_call.arguments      = p101_report_dup_text(more_env, more_error, call.arguments);
    open_call.result         = p101_report_dup_text(more_env, more_error, call.result);
    open_call.file_name      = p101_report_dup_text(more_env, more_error, call.file_name);
    p101_report_add_call(more_env, more_error, &model, &open_call);
    open_call.context_id     = 2U;
    open_call.kind           = CALL_ENTER;
    open_call.sequence       = 9U;
    open_call.event_sequence = 9U;
    open_call.function_name  = p101_report_dup_text(more_env, more_error, call.function_name);
    open_call.call_name      = p101_report_dup_text(more_env, more_error, "other-context");
    open_call.arguments      = p101_report_dup_text(more_env, more_error, call.arguments);
    open_call.result         = p101_report_dup_text(more_env, more_error, call.result);
    open_call.file_name      = p101_report_dup_text(more_env, more_error, call.file_name);
    p101_report_add_call(more_env, more_error, &model, &open_call);
    open_call.context_id     = 1U;
    open_call.kind           = CALL_ENTER;
    open_call.sequence       = 8U;
    open_call.event_sequence = 8U;
    open_call.function_name  = p101_report_dup_text(more_env, more_error, call.function_name);
    open_call.call_name      = p101_report_dup_text(more_env, more_error, "unclosed");
    open_call.arguments      = p101_report_dup_text(more_env, more_error, call.arguments);
    open_call.result         = p101_report_dup_text(more_env, more_error, call.result);
    open_call.file_name      = p101_report_dup_text(more_env, more_error, call.file_name);
    p101_report_add_call(more_env, more_error, &model, &open_call);
    open_call.kind           = CALL_ENTER;
    open_call.sequence       = 5U;
    open_call.event_sequence = 5U;
    open_call.function_name  = p101_report_dup_text(more_env, more_error, call.function_name);
    open_call.call_name      = p101_report_dup_text(more_env, more_error, "open");
    open_call.arguments      = p101_report_dup_text(more_env, more_error, call.arguments);
    open_call.result         = p101_report_dup_text(more_env, more_error, call.result);
    open_call.file_name      = p101_report_dup_text(more_env, more_error, call.file_name);
    p101_report_add_call(more_env, more_error, &model, &open_call);
    open_call.kind           = CALL_EXIT;
    open_call.sequence       = 6U;
    open_call.event_sequence = 6U;
    open_call.function_name  = p101_report_dup_text(more_env, more_error, call.function_name);
    open_call.call_name      = p101_report_dup_text(more_env, more_error, "open");
    open_call.arguments      = p101_report_dup_text(more_env, more_error, call.arguments);
    open_call.result         = p101_report_dup_text(more_env, more_error, call.result);
    open_call.file_name      = p101_report_dup_text(more_env, more_error, call.file_name);
    p101_report_add_call(more_env, more_error, &model, &open_call);
    open_call.sequence       = 7U;
    open_call.event_sequence = 7U;
    open_call.function_name  = p101_report_dup_text(more_env, more_error, call.function_name);
    open_call.call_name      = p101_report_dup_text(more_env, more_error, "open");
    open_call.arguments      = p101_report_dup_text(more_env, more_error, call.arguments);
    open_call.result         = p101_report_dup_text(more_env, more_error, call.result);
    open_call.file_name      = p101_report_dup_text(more_env, more_error, call.file_name);
    p101_report_add_call(more_env, more_error, &model, &open_call);

    finding.kind           = FINDING_RESOURCE_LEAK;
    finding.pid            = 1;
    finding.context_id     = 1U;
    finding.site           = first;
    finding.sequence       = 1U;
    finding.event_sequence = 1U;
    finding.ptr            = "ptr";
    finding.resource_class = "class";
    finding.resource_id    = "id";
    p101_report_add_finding_internal(more_env, more_error, &model, &finding);
    args.resource_log = "r";
    args.call_log     = "c";
    args.format       = (enum report_format)999;
    p101_report_print_report(more_env, more_error, &args, &model);
    p101_report_print_json_report(more_env, more_error, &args, &model);
    p101_report_print_mermaid_report(more_env, more_error, &args, &model);
    finding.event_sequence = 4U;
    p101_report_print_trace_context(more_env, more_error, &model, &finding);
    p101_report_print_json_trace_context(more_env, more_error, &model, &finding);
    finding.event_sequence = 8U;
    p101_report_print_trace_context(more_env, more_error, &model, &finding);
    p101_report_print_json_trace_context(more_env, more_error, &model, &finding);
    P101_ERROR_RAISE_CHECK(more_error);
    p101_report_print_trace_context(more_env, more_error, &model, &finding);
    p101_report_print_json_trace_context(more_env, more_error, &model, &finding);
    p101_error_reset(more_error);
    p101_report_free_model(more_env, &model);
}

static void test_lifetime_rendering_limits_and_missing_times(void)
{
    struct report_model   model;
    struct source_site    sites[1];
    struct resource_event resources[45];

    p101_memset(more_env, &model, 0, sizeof(model));
    p101_memset(more_env, sites, 0, sizeof(sites));
    p101_memset(more_env, resources, 0, sizeof(resources));
    sites[0].file_name     = "life.c";
    sites[0].function_name = "life";
    sites[0].line_number   = 1;
    model.sites            = sites;
    model.site_count       = 1U;
    model.resources        = resources;
    model.resource_count   = 45U;
    for(size_t index = 0U; index < 41U; index++)
    {
        resources[index].kind           = RESOURCE_FD_OPEN;
        resources[index].pid            = 1;
        resources[index].fd             = (int)index;
        resources[index].site           = 0U;
        resources[index].sequence       = index + 1U;
        resources[index].event_sequence = index + 1U;
    }
    resources[41].kind           = RESOURCE_FD_CLOSE;
    resources[41].pid            = 1;
    resources[41].fd             = 0;
    resources[41].site           = 0U;
    resources[41].sequence       = 42U;
    resources[41].event_sequence = 42U;
    resources[42].kind           = RESOURCE_ALLOC;
    resources[42].ptr            = "ptr";
    resources[42].pid            = 1;
    resources[42].site           = 0U;
    resources[42].sequence       = 43U;
    resources[42].event_sequence = 43U;
    resources[43].kind           = RESOURCE_GENERIC;
    resources[43].generic_kind   = P101_TOOL_EVENT_RESOURCE_TRANSFER;
    resources[43].resource_class = "mapping";
    resources[43].resource_id    = "old";
    resources[43].related_id     = "new";
    resources[43].pid            = 1;
    resources[43].site           = 0U;
    resources[43].sequence       = 44U;
    resources[43].event_sequence = 44U;
    resources[44].kind           = RESOURCE_GENERIC;
    resources[44].generic_kind   = P101_TOOL_EVENT_RESOURCE_RELEASE;
    resources[44].resource_class = "mapping";
    resources[44].resource_id    = "new";
    resources[44].pid            = 1;
    resources[44].site           = 0U;
    resources[44].sequence       = 45U;
    resources[44].event_sequence = 45U;

    p101_report_print_text_lifetimes(more_env, more_error, &model);
    p101_report_print_json_lifetimes(more_env, more_error, &model);
    p101_report_print_mermaid_lifetimes(more_env, more_error, &model);
    P101_ERROR_RAISE_CHECK(more_error);
    p101_report_print_text_lifetimes(more_env, more_error, &model);
    p101_report_print_json_lifetimes(more_env, more_error, &model);
    p101_report_print_mermaid_lifetimes(more_env, more_error, &model);
    p101_error_reset(more_error);
    model.resources = NULL;
    model.sites     = NULL;
}

static void test_lifetime_rendering_variants(void)
{
    struct report_model   model;
    struct source_site    sites[1];
    struct resource_event resources[9];

    p101_memset(more_env, &model, 0, sizeof(model));
    p101_memset(more_env, sites, 0, sizeof(sites));
    p101_memset(more_env, resources, 0, sizeof(resources));
    sites[0].file_name     = "life.c";
    sites[0].function_name = "life";
    sites[0].line_number   = 1;
    model.sites            = sites;
    model.site_count       = 1U;
    model.resources        = resources;
    model.resource_count   = sizeof(resources) / sizeof(resources[0]);

    resources[0].kind           = RESOURCE_ALLOC;
    resources[0].pid            = 1;
    resources[0].site           = 0U;
    resources[0].size           = 1U;
    resources[0].sequence       = 1U;
    resources[0].event_sequence = 1U;

    resources[1].kind                   = RESOURCE_REALLOC;
    resources[1].pid                    = 1;
    resources[1].ptr                    = "old";
    resources[1].new_ptr                = "new";
    resources[1].site                   = 0U;
    resources[1].size                   = 2U;
    resources[1].sequence               = 2U;
    resources[1].event_sequence         = 2U;
    resources[1].monotonic_ns_available = true;
    resources[1].monotonic_ns           = 20U;
    resources[2].kind                   = RESOURCE_FREE;
    resources[2].pid                    = 1;
    resources[2].ptr                    = "new";
    resources[2].site                   = 0U;
    resources[2].sequence               = 3U;
    resources[2].event_sequence         = 3U;
    resources[2].monotonic_ns_available = true;
    resources[2].monotonic_ns           = 30U;
    resources[2].wall_unix_ns_available = true;
    resources[2].wall_unix_ns           = 40U;

    resources[3].kind           = RESOURCE_GENERIC;
    resources[3].generic_kind   = P101_TOOL_EVENT_RESOURCE_REPLACE;
    resources[3].resource_class = "mapping";
    resources[3].resource_id    = "old-map";
    resources[3].related_id     = "new-map";
    resources[3].pid            = 1;
    resources[3].site           = 0U;
    resources[3].sequence       = 4U;
    resources[3].event_sequence = 4U;
    resources[4].kind           = RESOURCE_GENERIC;
    resources[4].generic_kind   = P101_TOOL_EVENT_RESOURCE_RELEASE;
    resources[4].resource_class = "mapping";
    resources[4].resource_id    = "new-map";
    resources[4].pid            = 1;
    resources[4].site           = 0U;
    resources[4].sequence       = 5U;
    resources[4].event_sequence = 5U;

    resources[5].kind           = RESOURCE_REALLOC;
    resources[5].new_ptr        = "-";
    resources[5].site           = 0U;
    resources[5].sequence       = 6U;
    resources[5].event_sequence = 6U;
    resources[6].kind           = RESOURCE_GENERIC;
    resources[6].generic_kind   = P101_TOOL_EVENT_RESOURCE_TRANSFER;
    resources[6].site           = 0U;
    resources[6].sequence       = 7U;
    resources[6].event_sequence = 7U;
    resources[7].kind           = RESOURCE_REALLOC;
    resources[7].site           = 0U;
    resources[7].sequence       = 8U;
    resources[7].event_sequence = 8U;
    resources[8].kind           = RESOURCE_GENERIC;
    resources[8].generic_kind   = P101_TOOL_EVENT_RESOURCE_ACQUIRE;
    resources[8].site           = 0U;
    resources[8].sequence       = 9U;
    resources[8].event_sequence = 9U;

    p101_report_print_text_lifetimes(more_env, more_error, &model);
    p101_report_print_json_lifetimes(more_env, more_error, &model);
    p101_report_print_mermaid_lifetimes(more_env, more_error, &model);
    model.resources = NULL;
    model.sites     = NULL;
}

static void test_lifetime_common_edge_cases(void)
{
    struct resource_event birth;
    struct resource_event death;
    size_t                duration;

    for(int kind = RESOURCE_FD_OPEN; kind <= RESOURCE_GENERIC; kind++)
    {
        TEST_ASSERT_NOT_NULL(p101_report_resource_kind_name((enum resource_kind)kind));
    }
    TEST_ASSERT_EQUAL_STRING("unknown", p101_report_resource_kind_name((enum resource_kind)999));
    TEST_ASSERT_EQUAL_STRING("acquire", p101_report_test_generic_operation_name(P101_TOOL_EVENT_RESOURCE_ACQUIRE));
    TEST_ASSERT_EQUAL_STRING("release", p101_report_test_generic_operation_name(P101_TOOL_EVENT_RESOURCE_RELEASE));
    TEST_ASSERT_EQUAL_STRING("replace", p101_report_test_generic_operation_name(P101_TOOL_EVENT_RESOURCE_REPLACE));
    TEST_ASSERT_EQUAL_STRING("transfer", p101_report_test_generic_operation_name(P101_TOOL_EVENT_RESOURCE_TRANSFER));
    TEST_ASSERT_EQUAL_STRING("resource", p101_report_test_generic_operation_name((p101_tool_event_resource_kind)999));
    p101_memset(more_env, &birth, 0, sizeof(birth));
    p101_memset(more_env, &death, 0, sizeof(death));
    TEST_ASSERT_FALSE(p101_report_lifetime_duration_ns(&birth, NULL, &duration));
    birth.monotonic_ns_available = true;
    death.monotonic_ns_available = true;
    birth.monotonic_ns           = 20U;
    death.monotonic_ns           = 10U;
    TEST_ASSERT_FALSE(p101_report_lifetime_duration_ns(&birth, &death, &duration));
    death.monotonic_ns = 30U;
    TEST_ASSERT_TRUE(p101_report_lifetime_duration_ns(&birth, &death, &duration));
    TEST_ASSERT_EQUAL_size_t(10U, duration);

    birth.kind           = RESOURCE_GENERIC;
    birth.generic_kind   = P101_TOOL_EVENT_RESOURCE_REPLACE;
    birth.resource_class = "mapping";
    birth.related_id     = NULL;
    {
        struct report_model   model;
        struct resource_event candidate;

        p101_memset(more_env, &model, 0, sizeof(model));
        p101_memset(more_env, &candidate, 0, sizeof(candidate));
        TEST_ASSERT_NULL(p101_report_find_generic_lifetime_end(more_env, &model, &birth));
        birth.related_id         = "id";
        candidate.kind           = RESOURCE_GENERIC;
        candidate.generic_kind   = P101_TOOL_EVENT_RESOURCE_RELEASE;
        candidate.resource_class = "other";
        candidate.resource_id    = "id";
        candidate.sequence       = 2U;
        model.resources          = &candidate;
        model.resource_count     = 1U;
        TEST_ASSERT_NULL(p101_report_find_generic_lifetime_end(more_env, &model, &birth));
    }
}

static void test_lifetime_lookup_variants(void)
{
    struct report_model   model;
    struct resource_event birth;
    struct resource_event events[10];
    size_t                duration;

    p101_memset(more_env, &model, 0, sizeof(model));
    p101_memset(more_env, &birth, 0, sizeof(birth));
    p101_memset(more_env, events, 0, sizeof(events));
    model.resources      = events;
    model.resource_count = sizeof(events) / sizeof(events[0]);

    birth.kind         = RESOURCE_FD_OPEN;
    birth.pid          = 1;
    birth.fd           = 3;
    birth.sequence     = 5U;
    events[0].sequence = 4U;
    events[0].kind     = RESOURCE_FD_CLOSE;
    events[0].pid      = 1;
    events[0].fd       = 3;
    events[1].sequence = 6U;
    events[1].kind     = RESOURCE_FD_OPEN;
    events[1].pid      = 1;
    events[1].fd       = 3;
    events[2].sequence = 7U;
    events[2].kind     = RESOURCE_FD_CLOSE;
    events[2].pid      = 2;
    events[2].fd       = 3;
    events[3].sequence = 8U;
    events[3].kind     = RESOURCE_FD_CLOSE;
    events[3].pid      = 1;
    events[3].fd       = 4;
    events[4].sequence = 9U;
    events[4].kind     = RESOURCE_FD_CLOSE;
    events[4].pid      = 1;
    events[4].fd       = 3;
    TEST_ASSERT_EQUAL_PTR(&events[4], p101_report_find_fd_lifetime_end(more_env, &model, &birth));

    p101_memset(more_env, events, 0, sizeof(events));
    birth.kind     = RESOURCE_ALLOC;
    birth.pid      = 1;
    birth.sequence = 5U;
    TEST_ASSERT_NULL(p101_report_find_alloc_lifetime_end(more_env, &model, &birth));
    birth.ptr = "ptr";

    events[0].sequence = 4U;
    events[0].pid      = 1;
    events[0].kind     = RESOURCE_FREE;
    events[0].ptr      = "ptr";
    events[1].sequence = 6U;
    events[1].pid      = 2;
    events[1].kind     = RESOURCE_FREE;
    events[1].ptr      = "ptr";
    events[2].sequence = 7U;
    events[2].pid      = 1;
    events[2].kind     = RESOURCE_FD_OPEN;
    events[2].ptr      = "ptr";
    events[3].sequence = 8U;
    events[3].pid      = 1;
    events[3].kind     = RESOURCE_FREE;
    events[3].ptr      = NULL;
    events[4].sequence = 9U;
    events[4].pid      = 1;
    events[4].kind     = RESOURCE_FREE;
    events[4].ptr      = "other";
    events[5].sequence = 10U;
    events[5].pid      = 1;
    events[5].kind     = RESOURCE_REALLOC;
    events[5].ptr      = "ptr";
    TEST_ASSERT_EQUAL_PTR(&events[5], p101_report_find_alloc_lifetime_end(more_env, &model, &birth));

    p101_memset(more_env, events, 0, sizeof(events));
    birth.kind               = RESOURCE_GENERIC;
    birth.generic_kind       = P101_TOOL_EVENT_RESOURCE_ACQUIRE;
    birth.resource_class     = "mapping";
    birth.resource_id        = "id";
    birth.sequence           = 5U;
    birth.pid                = 1;
    events[0].sequence       = 4U;
    events[1].sequence       = 6U;
    events[1].kind           = RESOURCE_FD_OPEN;
    events[2].sequence       = 7U;
    events[2].kind           = RESOURCE_GENERIC;
    events[2].pid            = 2;
    events[3].sequence       = 8U;
    events[3].kind           = RESOURCE_GENERIC;
    events[3].pid            = 1;
    events[4]                = events[3];
    events[4].sequence       = 9U;
    events[4].resource_class = "mapping";
    events[5]                = events[4];
    events[5].sequence       = 10U;
    events[5].resource_id    = "id";
    events[5].generic_kind   = P101_TOOL_EVENT_RESOURCE_ACQUIRE;
    events[6]                = events[5];
    events[6].sequence       = 11U;
    events[6].generic_kind   = P101_TOOL_EVENT_RESOURCE_REPLACE;
    events[7]                = events[6];
    events[7].sequence       = 12U;
    events[7].related_id     = "new";
    events[7].resource_class = "other";
    events[8]                = events[7];
    events[8].sequence       = 13U;
    events[8].resource_class = "mapping";
    events[8].resource_id    = "other";
    events[9]                = events[8];
    events[9].sequence       = 14U;
    events[9].resource_id    = "id";
    TEST_ASSERT_EQUAL_PTR(&events[9], p101_report_find_generic_lifetime_end(more_env, &model, &birth));

    TEST_ASSERT_FALSE(p101_report_lifetime_duration_ns(NULL, &birth, &duration));
    TEST_ASSERT_FALSE(p101_report_lifetime_duration_ns(&birth, &events[9], NULL));
    birth.monotonic_ns_available     = false;
    events[9].monotonic_ns_available = true;
    TEST_ASSERT_FALSE(p101_report_lifetime_duration_ns(&birth, &events[9], &duration));
    birth.monotonic_ns_available     = true;
    events[9].monotonic_ns_available = false;
    TEST_ASSERT_FALSE(p101_report_lifetime_duration_ns(&birth, &events[9], &duration));

    model.resources = NULL;
}

static void test_output_finding_variants(void)
{
    struct report_model model;
    struct finding      finding;
    size_t              site;
    size_t              previous;

    p101_memset(more_env, &model, 0, sizeof(model));
    site     = p101_report_intern_site(more_env, more_error, &model, "now.c", "now", 10);
    previous = p101_report_intern_site(more_env, more_error, &model, "before.c", "before", 9);
    p101_memset(more_env, &finding, 0, sizeof(finding));
    finding.pid      = 1;
    finding.site     = site;
    finding.sequence = 1U;

    finding.kind = FINDING_RESOURCE_LEAK;
    p101_report_print_finding(more_env, more_error, &model, &finding, 1U);
    p101_report_print_json_finding(more_env, more_error, &model, &finding);
    finding.kind = FINDING_RESOURCE_DOUBLE_RELEASE;
    p101_report_print_finding(more_env, more_error, &model, &finding, 1U);
    finding.kind = FINDING_RESOURCE_STRAY_RELEASE;
    p101_report_print_finding(more_env, more_error, &model, &finding, 1U);
    finding.kind = FINDING_RESOURCE_BAD_REPLACE;
    p101_report_print_finding(more_env, more_error, &model, &finding, 1U);
    finding.kind = FINDING_RESOURCE_DUPLICATE_ACQUIRE;
    p101_report_print_finding(more_env, more_error, &model, &finding, 1U);

    finding.kind              = FINDING_EXEC_INHERIT;
    finding.fd                = 3;
    finding.previous_sequence = 2U;
    finding.previous_site     = previous;
    p101_report_print_finding(more_env, more_error, &model, &finding, 2U);
    p101_report_print_json_finding(more_env, more_error, &model, &finding);

    finding.kind              = FINDING_ALLOC_LEAK;
    finding.size              = 1U;
    finding.previous_sequence = 3U;
    finding.previous_site     = model.site_count;
    p101_report_print_finding(more_env, more_error, &model, &finding, 3U);
    p101_report_print_json_finding(more_env, more_error, &model, &finding);
    finding.size = 2U;
    p101_report_print_finding(more_env, more_error, &model, &finding, 4U);

    p101_report_free_model(more_env, &model);
}

static void test_trace_context_limit(void)
{
    struct report_model model;
    struct source_site  site;
    struct call_event   calls[TRACE_CONTEXT_LIMIT + 2U];
    struct finding      finding;

    p101_memset(more_env, &model, 0, sizeof(model));
    p101_memset(more_env, &site, 0, sizeof(site));
    p101_memset(more_env, calls, 0, sizeof(calls));
    p101_memset(more_env, &finding, 0, sizeof(finding));
    site.file_name         = "trace.c";
    site.function_name     = "trace";
    site.line_number       = 1;
    model.sites            = &site;
    model.site_count       = 1U;
    model.calls            = calls;
    model.call_count       = sizeof(calls) / sizeof(calls[0]);
    finding.pid            = 1;
    finding.context_id     = 1U;
    finding.site           = 0U;
    finding.event_sequence = 100U;

    for(size_t index = 0U; index < model.call_count; index++)
    {
        calls[index].kind           = CALL_ENTER;
        calls[index].pid            = 1;
        calls[index].context_id     = 1U;
        calls[index].sequence       = index + 1U;
        calls[index].event_sequence = index == 0U ? 100U : index;
        calls[index].line_number    = 1;
        calls[index].function_name  = "trace";
        calls[index].call_name      = "repeat";
        calls[index].arguments      = "-";
        calls[index].result         = "-";
        calls[index].file_name      = "trace.c";
    }

    p101_report_print_trace_context(more_env, more_error, &model, &finding);
    p101_report_print_json_trace_context(more_env, more_error, &model, &finding);
    model.calls = NULL;
    model.sites = NULL;
}

static void test_generic_finding_mapping(void)
{
    static const p101_tool_event_lifecycle_finding_kind kinds[] = {P101_TOOL_EVENT_LIFECYCLE_FINDING_LEAK,
                                                                   P101_TOOL_EVENT_LIFECYCLE_FINDING_DOUBLE_RELEASE,
                                                                   P101_TOOL_EVENT_LIFECYCLE_FINDING_STRAY_RELEASE,
                                                                   P101_TOOL_EVENT_LIFECYCLE_FINDING_BAD_REPLACE,
                                                                   P101_TOOL_EVENT_LIFECYCLE_FINDING_DUPLICATE_ACQUIRE};

    for(size_t index = 0U; index < sizeof(kinds) / sizeof(kinds[0]); index++)
    {
        (void)p101_report_test_generic_finding_kind(more_env, kinds[index], "mapping");
        (void)p101_report_test_generic_finding_kind(more_env, kinds[index], "fd");
        (void)p101_report_test_generic_finding_kind(more_env, kinds[index], "allocation");
    }
    TEST_ASSERT_EQUAL_INT(FINDING_RESOURCE_BAD_REPLACE, p101_report_test_generic_finding_kind(more_env, (p101_tool_event_lifecycle_finding_kind)999, "mapping"));
}

static void test_generic_dependency_failures(void)
{
    struct report_model   model;
    struct resource_event event;

    p101_memset(more_env, &model, 0, sizeof(model));
    p101_memset(more_env, &event, 0, sizeof(event));
    (void)p101_report_intern_site(more_env, more_error, &model, "x.c", "f", 1);
    event.kind           = RESOURCE_GENERIC;
    event.generic_kind   = P101_TOOL_EVENT_RESOURCE_ACQUIRE;
    event.pid            = 1;
    event.context_id     = 1U;
    event.event_sequence = 1U;
    event.resource_class = "mapping";
    event.resource_id    = "id";
    event.site           = 0U;

    p101_report_test_set_generic_fault(1);
    p101_report_ingest_generic(more_env, more_error, &model, &event);
    TEST_ASSERT_TRUE(p101_error_has_error(more_error));
    p101_error_reset(more_error);

    p101_report_test_set_generic_fault(4);
    p101_report_ingest_generic(more_env, more_error, &model, &event);
    TEST_ASSERT_NULL(model.generic_lifecycle);
    p101_report_test_set_generic_fault(0);

    p101_report_test_set_generic_fault(0);
    p101_report_ingest_generic(more_env, more_error, &model, &event);
    p101_report_test_set_generic_fault(2);
    p101_report_finalize_generic(more_env, more_error, &model);
    p101_report_test_set_generic_fault(0);
    P101_ERROR_RAISE_CHECK(more_error);
    p101_report_finalize_generic(more_env, more_error, &model);
    p101_error_reset(more_error);
    p101_report_finalize_generic(more_env, more_error, &model);
    p101_report_finalize_generic(more_env, more_error, &model);
    p101_report_free_model(more_env, &model);

    p101_memset(more_env, &model, 0, sizeof(model));
    (void)p101_report_intern_site(more_env, more_error, &model, "x.c", "f", 1);
    p101_report_ingest_generic(more_env, more_error, &model, &event);
    p101_report_test_set_generic_fault(3);
    p101_report_finalize_generic(more_env, more_error, &model);
    TEST_ASSERT_TRUE(p101_error_has_error(more_error));
    p101_error_reset(more_error);
    p101_report_test_set_generic_fault(0);
    p101_report_free_model(more_env, &model);
}

static void test_model_growth_failures(void)
{
    struct report_model   model;
    struct resource_event event;
    struct finding        finding;
    struct call_event     call;

    p101_memset(more_env, &model, 0, sizeof(model));
    p101_report_test_set_grow_failure_countdown(1U);
    (void)p101_report_intern_site(more_env, more_error, &model, "x.c", "f", 1);
    TEST_ASSERT_TRUE(p101_error_has_error(more_error));
    p101_error_reset(more_error);
    (void)p101_report_intern_site(more_env, more_error, &model, "x.c", "f", 1);

    p101_memset(more_env, &finding, 0, sizeof(finding));
    p101_report_test_set_grow_failure_countdown(1U);
    p101_report_add_finding_internal(more_env, more_error, &model, &finding);
    TEST_ASSERT_TRUE(p101_error_has_error(more_error));
    p101_error_reset(more_error);

    p101_memset(more_env, &call, 0, sizeof(call));
    p101_report_test_set_grow_failure_countdown(1U);
    p101_report_add_call(more_env, more_error, &model, &call);
    TEST_ASSERT_TRUE(p101_error_has_error(more_error));
    p101_error_reset(more_error);

    p101_memset(more_env, &event, 0, sizeof(event));
    event.kind           = RESOURCE_EXEC_FAIL;
    event.pid            = 1;
    event.sequence       = 1U;
    event.event_sequence = 1U;
    p101_report_test_set_grow_failure_countdown(1U);
    p101_report_ingest_resource(more_env, more_error, &model, &event);
    TEST_ASSERT_TRUE(p101_error_has_error(more_error));
    TEST_ASSERT_EQUAL_size_t(0U, model.resource_count);
    p101_error_reset(more_error);

    event.kind           = RESOURCE_FD_OPEN;
    event.pid            = 1;
    event.fd             = 3;
    event.site           = 0U;
    event.sequence       = 1U;
    event.event_sequence = 1U;
    p101_report_test_set_grow_failure_countdown(1U);
    p101_report_ingest_resource(more_env, more_error, &model, &event);
    TEST_ASSERT_TRUE(p101_error_has_error(more_error));
    p101_error_reset(more_error);

    p101_report_test_set_grow_failure_countdown(2U);
    p101_report_ingest_resource(more_env, more_error, &model, &event);
    TEST_ASSERT_TRUE(p101_error_has_error(more_error));
    p101_error_reset(more_error);

    p101_report_test_set_grow_failure_countdown(0U);
    p101_report_ingest_resource(more_env, more_error, &model, &event);
    event.kind           = RESOURCE_FD_CLOSE;
    event.sequence       = 2U;
    event.event_sequence = 2U;
    p101_report_test_set_grow_failure_countdown(2U);
    p101_report_ingest_resource(more_env, more_error, &model, &event);
    TEST_ASSERT_TRUE(p101_error_has_error(more_error));
    p101_error_reset(more_error);

    event.kind           = RESOURCE_ALLOC;
    event.ptr            = "ptr";
    event.sequence       = 3U;
    event.event_sequence = 3U;
    p101_report_test_set_grow_failure_countdown(2U);
    p101_report_ingest_resource(more_env, more_error, &model, &event);
    TEST_ASSERT_TRUE(p101_error_has_error(more_error));
    p101_error_reset(more_error);

    p101_report_test_set_grow_failure_countdown(0U);
    p101_report_free_model(more_env, &model);
}

static void test_generic_lookup_variants(void)
{
    struct report_model                      model;
    struct resource_event                    events[7];
    struct p101_tool_event_lifecycle_finding finding;

    p101_memset(more_env, &model, 0, sizeof(model));
    p101_memset(more_env, events, 0, sizeof(events));
    p101_memset(more_env, &finding, 0, sizeof(finding));
    model.resources        = events;
    model.resource_count   = sizeof(events) / sizeof(events[0]);
    finding.pid            = 1;
    finding.context_id     = 2U;
    finding.sequence       = 3U;
    finding.resource_class = "mapping";

    events[0].pid            = 2;
    events[0].context_id     = 2U;
    events[0].event_sequence = 3U;
    events[1].pid            = 1;
    events[1].context_id     = 2U;
    events[1].event_sequence = 4U;
    events[2].pid            = 1;
    events[2].context_id     = 3U;
    events[2].event_sequence = 3U;
    events[3].pid            = 1;
    events[3].context_id     = 2U;
    events[3].event_sequence = 3U;
    events[3].resource_class = "other";
    events[4]                = events[3];
    events[4].resource_class = "mapping";
    events[4].sequence       = 44U;
    TEST_ASSERT_EQUAL_size_t(44U, p101_report_test_find_generic_sequence(more_env, &model, &finding, false));
    TEST_ASSERT_EQUAL_PTR(&events[3], p101_report_test_find_lifecycle_event(&model, &finding));

    finding.previous_context_id = 5U;
    finding.previous_sequence   = 6U;
    events[5].pid               = 1;
    events[5].context_id        = 5U;
    events[5].event_sequence    = 6U;
    events[5].kind              = RESOURCE_FD_OPEN;
    events[5].sequence          = 55U;
    finding.resource_class      = "fd";
    TEST_ASSERT_EQUAL_size_t(55U, p101_report_test_find_generic_sequence(more_env, &model, &finding, true));

    events[5].kind         = RESOURCE_ALLOC;
    events[5].sequence     = 66U;
    finding.resource_class = "allocation";
    TEST_ASSERT_EQUAL_size_t(66U, p101_report_test_find_generic_sequence(more_env, &model, &finding, true));
    finding.resource_class = "other";
    TEST_ASSERT_EQUAL_size_t(6U, p101_report_test_find_generic_sequence(more_env, &model, &finding, true));

    events[6].pid            = 1;
    events[6].context_id     = 2U;
    events[6].event_sequence = 3U;
    TEST_ASSERT_NOT_NULL(p101_report_test_find_lifecycle_event(&model, &finding));
    finding.context_id = 9U;
    TEST_ASSERT_NULL(p101_report_test_find_lifecycle_event(&model, &finding));

    model.resources = NULL;
}

static void set_resource_event(struct resource_event *event, enum resource_kind kind, long pid, size_t sequence)
{
    p101_memset(more_env, event, 0, sizeof(*event));
    event->kind           = kind;
    event->pid            = pid;
    event->site           = 0U;
    event->sequence       = sequence;
    event->event_sequence = sequence;
    event->context_id     = 1U;
}

static void test_model_event_edge_variants(void)
{
    struct report_model   model;
    struct resource_event event;

    p101_memset(more_env, &model, 0, sizeof(model));
    (void)p101_report_intern_site(more_env, more_error, &model, "edge.c", "edge", 1);

    set_resource_event(&event, (enum resource_kind)999, 1, 1U);
    p101_report_ingest_resource(more_env, more_error, &model, &event);

    set_resource_event(&event, RESOURCE_FD_OPEN, 1, 2U);
    event.fd = 3;
    p101_report_ingest_resource(more_env, more_error, &model, &event);
    set_resource_event(&event, RESOURCE_FD_OPEN, 1, 3U);
    event.fd = 4;
    p101_report_ingest_resource(more_env, more_error, &model, &event);
    set_resource_event(&event, RESOURCE_FD_CLOSE, 1, 4U);
    event.fd = 9;
    p101_report_ingest_resource(more_env, more_error, &model, &event);
    set_resource_event(&event, RESOURCE_FD_CLOSE, 1, 5U);
    event.fd = 3;
    p101_report_test_set_grow_failure_countdown(2U);
    p101_report_ingest_resource(more_env, more_error, &model, &event);
    TEST_ASSERT_TRUE(p101_error_has_error(more_error));
    p101_error_reset(more_error);

    set_resource_event(&event, RESOURCE_ALLOC, 1, 6U);
    p101_report_ingest_resource(more_env, more_error, &model, &event);
    set_resource_event(&event, RESOURCE_ALLOC, 1, 7U);
    event.ptr = "a";
    p101_report_ingest_resource(more_env, more_error, &model, &event);
    set_resource_event(&event, RESOURCE_ALLOC, 1, 8U);
    event.ptr = "b";
    p101_report_ingest_resource(more_env, more_error, &model, &event);
    set_resource_event(&event, RESOURCE_ALLOC, 2, 81U);
    event.ptr = "other-pid";
    p101_report_ingest_resource(more_env, more_error, &model, &event);
    set_resource_event(&event, RESOURCE_FREE, 1, 9U);
    event.ptr = "missing";
    p101_report_ingest_resource(more_env, more_error, &model, &event);
    set_resource_event(&event, RESOURCE_FREE, 1, 10U);
    event.ptr = "a";
    p101_report_test_set_grow_failure_countdown(2U);
    p101_report_ingest_resource(more_env, more_error, &model, &event);
    TEST_ASSERT_TRUE(p101_error_has_error(more_error));
    p101_error_reset(more_error);

    set_resource_event(&event, RESOURCE_REALLOC, 1, 11U);
    p101_report_ingest_resource(more_env, more_error, &model, &event);
    set_resource_event(&event, RESOURCE_REALLOC, 1, 12U);
    event.ptr     = "missing";
    event.new_ptr = "-";
    p101_report_ingest_resource(more_env, more_error, &model, &event);
    set_resource_event(&event, RESOURCE_REALLOC, 1, 13U);
    event.ptr     = "b";
    event.new_ptr = "c";
    p101_report_ingest_resource(more_env, more_error, &model, &event);
    set_resource_event(&event, RESOURCE_REALLOC, 1, 131U);
    event.ptr     = "missing";
    event.new_ptr = "new";
    p101_report_test_set_grow_failure_countdown(2U);
    p101_report_ingest_resource(more_env, more_error, &model, &event);
    TEST_ASSERT_TRUE(p101_error_has_error(more_error));
    p101_error_reset(more_error);

    set_resource_event(&event, RESOURCE_FD_OPEN, 2, 14U);
    event.fd = 4;
    p101_report_ingest_resource(more_env, more_error, &model, &event);
    set_resource_event(&event, RESOURCE_FD_OPEN, 1, 15U);
    event.fd = 5;
    p101_report_ingest_resource(more_env, more_error, &model, &event);
    set_resource_event(&event, RESOURCE_FD_CLOSE, 2, 16U);
    event.fd = 5;
    p101_report_ingest_resource(more_env, more_error, &model, &event);
    set_resource_event(&event, RESOURCE_FD_OPEN, 1, 17U);
    event.fd = 6;
    p101_report_ingest_resource(more_env, more_error, &model, &event);
    set_resource_event(&event, RESOURCE_FORK, 1, 18U);
    event.child_pid = 2;
    p101_report_test_set_grow_failure_countdown(2U);
    p101_report_ingest_resource(more_env, more_error, &model, &event);
    TEST_ASSERT_TRUE(p101_error_has_error(more_error));
    p101_error_reset(more_error);

    set_resource_event(&event, RESOURCE_EXEC, 1, 19U);
    event.fd     = 4;
    event.target = "/ok";
    p101_report_test_set_grow_failure_countdown(0U);
    p101_report_ingest_resource(more_env, more_error, &model, &event);
    set_resource_event(&event, RESOURCE_EXEC, 1, 20U);
    event.fd      = 4;
    event.cloexec = 1;
    event.target  = "/ok";
    p101_report_ingest_resource(more_env, more_error, &model, &event);
    set_resource_event(&event, RESOURCE_SPAWN, 1, 21U);
    p101_report_test_set_grow_failure_countdown(1U);
    p101_report_ingest_resource(more_env, more_error, &model, &event);
    TEST_ASSERT_TRUE(p101_error_has_error(more_error));
    p101_error_reset(more_error);

    p101_report_test_set_grow_failure_countdown(0U);
    p101_report_free_model(more_env, &model);

    p101_memset(more_env, &model, 0, sizeof(model));
    (void)p101_report_intern_site(more_env, more_error, &model, "edge.c", "edge", 1);
    set_resource_event(&event, RESOURCE_EXEC, 1, 1U);
    event.fd     = 99;
    event.target = "/none";
    p101_report_ingest_resource(more_env, more_error, &model, &event);
    p101_report_free_model(more_env, &model);
}

static void test_exec_rollback_predicates(void)
{
    struct resource_event candidate;
    struct resource_event event;
    struct finding        finding;

    p101_memset(more_env, &candidate, 0, sizeof(candidate));
    p101_memset(more_env, &event, 0, sizeof(event));
    p101_memset(more_env, &finding, 0, sizeof(finding));
    event.kind   = RESOURCE_EXEC_FAIL;
    event.pid    = 1;
    event.site   = 2U;
    event.target = "/target";

    TEST_ASSERT_FALSE(p101_report_test_exec_attempt_matches(more_env, &candidate, &event));
    candidate.kind = RESOURCE_EXEC;
    candidate.pid  = 2;
    TEST_ASSERT_FALSE(p101_report_test_exec_attempt_matches(more_env, &candidate, &event));
    candidate.pid  = 1;
    candidate.site = 3U;
    TEST_ASSERT_FALSE(p101_report_test_exec_attempt_matches(more_env, &candidate, &event));
    candidate.site = 2U;
    TEST_ASSERT_FALSE(p101_report_test_exec_attempt_matches(more_env, &candidate, &event));
    candidate.target = "/target";
    event.target     = NULL;
    TEST_ASSERT_FALSE(p101_report_test_exec_attempt_matches(more_env, &candidate, &event));
    event.target = "/other";
    TEST_ASSERT_FALSE(p101_report_test_exec_attempt_matches(more_env, &candidate, &event));
    event.target = "/target";
    TEST_ASSERT_TRUE(p101_report_test_exec_attempt_matches(more_env, &candidate, &event));

    TEST_ASSERT_FALSE(p101_report_test_exec_finding_matches(more_env, &finding, &event, 5U));
    finding.kind = FINDING_EXEC_INHERIT;
    finding.pid  = 2;
    TEST_ASSERT_FALSE(p101_report_test_exec_finding_matches(more_env, &finding, &event, 5U));
    finding.pid      = 1;
    finding.sequence = 4U;
    TEST_ASSERT_FALSE(p101_report_test_exec_finding_matches(more_env, &finding, &event, 5U));
    finding.sequence = 5U;
    TEST_ASSERT_FALSE(p101_report_test_exec_finding_matches(more_env, &finding, &event, 5U));
    finding.ptr  = "/target";
    event.target = NULL;
    TEST_ASSERT_FALSE(p101_report_test_exec_finding_matches(more_env, &finding, &event, 5U));
    event.target = "/other";
    TEST_ASSERT_FALSE(p101_report_test_exec_finding_matches(more_env, &finding, &event, 5U));
    event.target = "/target";
    TEST_ASSERT_TRUE(p101_report_test_exec_finding_matches(more_env, &finding, &event, 5U));
}

static void ingest_simple(struct report_model *model, enum resource_kind kind, long pid, long child_pid, int fd, int cloexec, size_t sequence)
{
    struct resource_event event;

    p101_memset(more_env, &event, 0, sizeof(event));
    event.kind           = kind;
    event.pid            = pid;
    event.child_pid      = child_pid;
    event.fd             = fd;
    event.cloexec        = cloexec;
    event.site           = 0U;
    event.sequence       = sequence;
    event.event_sequence = sequence;
    event.context_id     = 1U;
    event.target         = "/target";
    p101_report_ingest_resource(more_env, more_error, model, &event);
}

static void test_fork_exec_and_rollback_edges(void)
{
    struct report_model   model;
    struct finding        retained;
    struct resource_event allocation;

    p101_memset(more_env, &model, 0, sizeof(model));
    p101_memset(more_env, &allocation, 0, sizeof(allocation));
    allocation.kind = RESOURCE_EXEC_FAIL;
    p101_report_test_rollback_failed_exec(more_env, &model, &allocation);
    (void)p101_report_intern_site(more_env, more_error, &model, "x.c", "f", 1);
    ingest_simple(&model, RESOURCE_EXEC_FAIL, 1, -1, 0, 0, 1U);
    ingest_simple(&model, RESOURCE_FD_OPEN, 2, -1, 4, 0, 2U);
    ingest_simple(&model, RESOURCE_FD_OPEN, 3, -1, 4, 0, 3U);
    ingest_simple(&model, RESOURCE_FORK, 2, 3, 0, 0, 4U);
    ingest_simple(&model, RESOURCE_FD_CLOSE, 3, -1, 4, 0, 5U);
    ingest_simple(&model, RESOURCE_FD_OPEN, 2, -1, 5, 0, 6U);
    ingest_simple(&model, RESOURCE_FD_CLOSE, 3, -1, 5, 0, 7U);
    ingest_simple(&model, RESOURCE_FORK, 2, 3, 0, 0, 8U);
    ingest_simple(&model, RESOURCE_FD_OPEN, 4, -1, 6, 0, 9U);
    ingest_simple(&model, RESOURCE_EXEC, 4, -1, 6, 0, 10U);
    p101_memset(more_env, &allocation, 0, sizeof(allocation));
    allocation.kind           = RESOURCE_ALLOC;
    allocation.pid            = 6;
    allocation.ptr            = "live";
    allocation.new_ptr        = "-";
    allocation.site           = 0U;
    allocation.sequence       = 11U;
    allocation.event_sequence = 11U;
    p101_report_ingest_resource(more_env, more_error, &model, &allocation);
    allocation.kind           = RESOURCE_FREE;
    allocation.ptr            = "other";
    allocation.sequence       = 12U;
    allocation.event_sequence = 12U;
    p101_report_ingest_resource(more_env, more_error, &model, &allocation);
    p101_report_finalize_leaks(more_env, more_error, &model);
    TEST_ASSERT_FALSE(p101_error_has_error(more_error));
    p101_report_free_model(more_env, &model);

    p101_memset(more_env, &model, 0, sizeof(model));
    p101_memset(more_env, &retained, 0, sizeof(retained));
    (void)p101_report_intern_site(more_env, more_error, &model, "x.c", "f", 1);
    ingest_simple(&model, RESOURCE_FD_OPEN, 5, -1, 7, 0, 1U);
    ingest_simple(&model, RESOURCE_EXEC, 5, -1, 7, 0, 2U);
    retained.kind     = FINDING_STRAY_CLOSE;
    retained.pid      = 99;
    retained.fd       = 9;
    retained.site     = 0U;
    retained.sequence = 3U;
    p101_report_add_finding_internal(more_env, more_error, &model, &retained);
    ingest_simple(&model, RESOURCE_EXEC_FAIL, 5, -1, 0, 0, 3U);
    TEST_ASSERT_EQUAL_size_t(1U, model.finding_count);
    TEST_ASSERT_EQUAL_INT(FINDING_STRAY_CLOSE, model.findings[0].kind);
    p101_report_free_model(more_env, &model);
}

void p101_report_run_more_tests(void)
{
    more_setup();
    RUN_TEST(test_finding_names_ids_and_site_matching);
    more_teardown();
    more_setup();
    RUN_TEST(test_io_json_and_memory_helpers);
    more_teardown();
    more_setup();
    RUN_TEST(test_parse_statuses_and_cross_stream_records);
    more_teardown();
    more_setup();
    RUN_TEST(test_model_support_and_output_defaults);
    more_teardown();
    more_setup();
    RUN_TEST(test_lifetime_common_edge_cases);
    more_teardown();
    more_setup();
    RUN_TEST(test_lifetime_lookup_variants);
    more_teardown();
    more_setup();
    RUN_TEST(test_generic_finding_mapping);
    more_teardown();
    more_setup();
    RUN_TEST(test_output_finding_variants);
    more_teardown();
    more_setup();
    RUN_TEST(test_trace_context_limit);
    more_teardown();
    more_setup();
    RUN_TEST(test_generic_dependency_failures);
    more_teardown();
    more_setup();
    RUN_TEST(test_model_growth_failures);
    more_teardown();
    more_setup();
    RUN_TEST(test_generic_lookup_variants);
    more_teardown();
    more_setup();
    RUN_TEST(test_model_event_edge_variants);
    more_teardown();
    more_setup();
    RUN_TEST(test_exec_rollback_predicates);
    more_teardown();
    more_setup();
    RUN_TEST(test_fork_exec_and_rollback_edges);
    more_teardown();
    more_setup();
    RUN_TEST(test_lifetime_rendering_limits_and_missing_times);
    more_teardown();
    more_setup();
    RUN_TEST(test_lifetime_rendering_variants);
    more_teardown();
}
