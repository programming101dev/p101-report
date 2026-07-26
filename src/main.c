#include "arguments.h"
#include "errors.h"
#include <errno.h>
#include <limits.h>
#include <p101_c/p101_ctype.h>
#include <p101_c/p101_stdio.h>
#include <p101_c/p101_stdlib.h>
#include <p101_c/p101_string.h>
#include <p101_posix/p101_string.h>
#include <p101_posix/p101_unistd.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

enum resource_kind
{
    RESOURCE_FD_OPEN = 0,
    RESOURCE_FD_CLOSE,
    RESOURCE_ALLOC,
    RESOURCE_FREE,
    RESOURCE_REALLOC
};

enum call_kind
{
    CALL_ENTER = 0,
    CALL_EXIT
};

enum line_status
{
    LINE_OTHER = 0,
    LINE_OK,
    LINE_MALFORMED,
    LINE_BAD_VERSION
};

enum finding_kind
{
    FINDING_FD_LEAK = 0,
    FINDING_ALLOC_LEAK,
    FINDING_DOUBLE_CLOSE,
    FINDING_STRAY_CLOSE,
    FINDING_DOUBLE_FREE,
    FINDING_STRAY_FREE,
    FINDING_BAD_REALLOC
};

struct source_site
{
    char *file_name;
    char *function_name;
    int   line_number;
};

struct resource_event
{
    enum resource_kind kind;
    long               pid;
    int                fd;
    char              *ptr;
    char              *new_ptr;
    size_t             size;
    size_t             site;
    size_t             sequence;
};

struct call_event
{
    enum call_kind kind;
    long           pid;
    int            line_number;
    char          *function_name;
    char          *call_name;
    char          *arguments;
    char          *result;
    char          *file_name;
    size_t         sequence;
};

struct live_fd
{
    long   pid;
    int    fd;
    size_t site;
    size_t sequence;
};

struct closed_fd
{
    long   pid;
    int    fd;
    size_t site;
    size_t sequence;
};

struct live_alloc
{
    long   pid;
    char  *ptr;
    size_t size;
    size_t site;
    size_t sequence;
};

struct freed_alloc
{
    long   pid;
    char  *ptr;
    size_t site;
    size_t sequence;
};

struct finding
{
    enum finding_kind kind;
    long              pid;
    int               fd;
    char             *ptr;
    size_t            size;
    size_t            site;
    size_t            sequence;
    size_t            previous_site;
    size_t            previous_sequence;
};

struct report_model
{
    struct source_site *sites;
    size_t              site_count;
    size_t              site_capacity;

    struct call_event *calls;
    size_t             call_count;
    size_t             call_capacity;

    struct live_fd   *fds;
    size_t            fd_count;
    size_t            fd_capacity;
    struct closed_fd *closed_fds;
    size_t            closed_fd_count;
    size_t            closed_fd_capacity;

    struct live_alloc  *allocs;
    size_t              alloc_count;
    size_t              alloc_capacity;
    struct freed_alloc *freed_allocs;
    size_t              freed_alloc_count;
    size_t              freed_alloc_capacity;

    struct finding *findings;
    size_t          finding_count;
    size_t          finding_capacity;

    size_t resource_records;
    size_t call_records;
    size_t resource_skipped;
    size_t call_skipped;
    size_t resource_malformed;
    size_t call_malformed;
    size_t resource_bad_version;
    size_t call_bad_version;
};

static void             parse_arguments(const struct p101_env *env, struct p101_error *err, int argc, char *argv[], struct arguments *args);
static void             check_arguments(const struct p101_env *env, struct p101_error *err, struct arguments *args, char *resource_buf, size_t resource_buf_size, char *call_buf, size_t call_buf_size);
static int              run_report(const struct p101_env *env, struct p101_error *err, const struct arguments *args);
static void             read_resources(const struct p101_env *env, struct p101_error *err, const char *path, struct report_model *model);
static void             read_calls(const struct p101_env *env, struct p101_error *err, const char *path, struct report_model *model);
static enum line_status parse_resource_line(const struct p101_env *env, struct p101_error *err, char *line, struct resource_event *event, struct report_model *model, size_t sequence);
static enum line_status parse_call_line(const struct p101_env *env, struct p101_error *err, char *line, struct call_event *event, size_t sequence);
static void             ingest_resource(const struct p101_env *env, struct p101_error *err, struct report_model *model, const struct resource_event *event);
static void             finalize_leaks(const struct p101_env *env, struct p101_error *err, struct report_model *model);
static void             print_report(const struct p101_env *env, struct p101_error *err, const struct arguments *args, const struct report_model *model);
static void             print_text_report(const struct p101_env *env, struct p101_error *err, const struct arguments *args, const struct report_model *model);
static void             print_json_report(const struct p101_env *env, struct p101_error *err, const struct arguments *args, const struct report_model *model);
static void             print_finding(const struct p101_env *env, struct p101_error *err, const struct report_model *model, const struct finding *finding, size_t ordinal);
static void             print_trace_context(const struct p101_env *env, struct p101_error *err, const struct report_model *model, const struct finding *finding);
static void             print_json_finding(const struct p101_env *env, struct p101_error *err, const struct report_model *model, const struct finding *finding);
static void             print_json_trace_context(const struct p101_env *env, struct p101_error *err, const struct report_model *model, const struct finding *finding);
static void             json_string(const struct p101_env *env, struct p101_error *err, FILE *stream, const char *text);
static size_t           intern_site(const struct p101_env *env, struct p101_error *err, struct report_model *model, const char *file_name, const char *function_name, int line_number);
static void             add_finding(const struct p101_env *env, struct p101_error *err, struct report_model *model, const struct finding *finding);
static void             add_call(const struct p101_env *env, struct p101_error *err, struct report_model *model, const struct call_event *event);
static bool             grow_array(const struct p101_env *env, struct p101_error *err, void **items, size_t *capacity, size_t count, size_t item_size);
static char            *dup_text(const struct p101_env *env, struct p101_error *err, const char *text);
static void             free_model(const struct p101_env *env, struct report_model *model);
static void             free_resource_event(const struct p101_env *env, struct resource_event *event);
static void             free_call_event(const struct p101_env *env, struct call_event *event);
static char            *split_tab(char **cursor);
static bool             strip_line(const struct p101_env *env, char *line);
static bool             parse_long_field(const char *text, long min, long max, long *out);
static bool             parse_size_field(const char *text, size_t *out);
static bool             line_has_prefix(const struct p101_env *env, const char *line, const char *prefix);
static bool             site_matches_call(const struct p101_env *env, const struct source_site *site, const struct call_event *call, long pid);
static const char      *finding_name(enum finding_kind kind);
static int              close_if_owned(const struct p101_env *env, struct p101_error *err, FILE *stream, bool owned);
static FILE            *open_input(const struct p101_env *env, struct p101_error *err, const char *path, bool *owned);
static void             join_path(const struct p101_env *env, struct p101_error *err, char *out, size_t out_size, const char *dir, const char *leaf);
_Noreturn static void   usage(const struct p101_env *env, struct p101_error *err, const char *program_name, int exit_code, const char *message);

static const char FD_PREFIX[]    = "P101FD\t";
static const char ALLOC_PREFIX[] = "P101ALLOC\t";
static const char CALL_PREFIX[]  = "P101CALL\t";

enum
{
    LOG_VERSION         = 1,
    LINE_MAX_BYTES      = 4096,
    PATH_MAX_BYTES      = 4096,
    MSG_LEN             = 256,
    DECIMAL_BASE        = 10,
    JSON_CONTROL_LIMIT  = 0x20U,
    FIRST_CAPACITY      = 16,
    TRACE_CONTEXT_LIMIT = 5,
    EXIT_CLEAN          = 0,
    EXIT_FINDINGS       = 1,
    EXIT_TROUBLE        = 2
};

int main(int argc, char *argv[])
{
    struct p101_error *err;
    struct p101_env   *env;
    struct arguments   args;
    char               resource_buf[PATH_MAX_BYTES];
    char               call_buf[PATH_MAX_BYTES];
    int                ret_val;

    ret_val = EXIT_TROUBLE;
    err     = p101_error_create(false);
    env     = p101_env_create(err, NULL);
    p101_memset(env, &args, 0, sizeof(args));
    p101_memset(env, resource_buf, 0, sizeof(resource_buf));
    p101_memset(env, call_buf, 0, sizeof(call_buf));

    parse_arguments(env, err, argc, argv, &args);

    if(p101_error_has_error(err))
    {
        goto done;
    }

    if(args.verbose)
    {
        p101_env_set_tracer(env, p101_env_default_tracer);
    }

    check_arguments(env, err, &args, resource_buf, sizeof(resource_buf), call_buf, sizeof(call_buf));

    if(p101_error_has_error(err))
    {
        goto done;
    }

    ret_val = run_report(env, err, &args);

done:
    if(p101_error_has_error(err))
    {
        if(p101_error_is_error(err, P101_ERROR_USER, ERR_USAGE))
        {
            const char *msg;

            msg = p101_error_get_message(err);
            usage(env, err, argv[0], EXIT_TROUBLE, msg);
        }

        p101_fprintf(env, err, stderr, "%s\n", p101_error_get_message(err));
        ret_val = EXIT_TROUBLE;
    }

    p101_env_destroy(env);
    p101_error_destroy(err);

    return ret_val;
}

static void parse_arguments(const struct p101_env *env, struct p101_error *err, int argc, char *argv[], struct arguments *args)
{
    int opt;

    P101_TRACE(env);
    opterr = 0;

    while((opt = p101_getopt(env, argc, argv, ":hvjd:r:c:")) != -1 && p101_error_has_no_error(err))
    {
        switch(opt)
        {
            case 'h':
            {
                usage(env, err, argv[0], EXIT_CLEAN, NULL);
            }
            case 'v':
            {
                args->verbose = true;
                break;
            }
            case 'j':
            {
                args->format = REPORT_FORMAT_JSON;
                break;
            }
            case 'd':
            {
                args->report_dir = optarg;
                break;
            }
            case 'r':
            {
                args->resource_log = optarg;
                break;
            }
            case 'c':
            {
                args->call_log = optarg;
                break;
            }
            case ':':
            {
                char msg[MSG_LEN];

                p101_snprintf(env, err, msg, sizeof(msg), "Option '-%c' requires an argument.", optopt ? optopt : '?');
                P101_ERROR_RAISE_USER(err, msg, ERR_USAGE);
                break;
            }
            case '?':
            {
                char msg[MSG_LEN];

                if(p101_isprint(env, optopt))
                {
                    p101_snprintf(env, err, msg, sizeof(msg), "Unknown option '-%c'.", optopt);
                }
                else
                {
                    p101_snprintf(env, err, msg, sizeof(msg), "Unknown option character 0x%02X.", (unsigned)(unsigned char)optopt);
                }

                P101_ERROR_RAISE_USER(err, msg, ERR_USAGE);
                break;
            }
            default:
            {
                char msg[MSG_LEN];

                p101_snprintf(env, err, msg, sizeof(msg), "Internal error: unhandled option '-%c'.", p101_isprint(env, opt) ? opt : '?');
                P101_ERROR_RAISE_USER(err, msg, ERR_USAGE);
                break;
            }
        }
    }

    if(p101_error_has_error(err))
    {
        goto done;
    }

    if(optind < argc)
    {
        if(args->report_dir != NULL)
        {
            P101_ERROR_RAISE_USER(err, "Report directory was supplied twice.", ERR_USAGE);
            goto done;
        }

        args->report_dir = argv[optind];
        optind++;
    }

    if(optind < argc)
    {
        P101_ERROR_RAISE_USER(err, "Too many positional arguments.", ERR_USAGE);
    }

done:
    return;
}

static void check_arguments(const struct p101_env *env, struct p101_error *err, struct arguments *args, char *resource_buf, size_t resource_buf_size, char *call_buf, size_t call_buf_size)
{
    P101_TRACE(env);

    if(args->report_dir == NULL && (args->resource_log == NULL || args->call_log == NULL))
    {
        P101_ERROR_RAISE_USER(err, "Provide a report directory, or both -r resources.log and -c calls.log.", ERR_USAGE);
        goto done;
    }

    if(args->report_dir != NULL && args->report_dir[0] == '\0')
    {
        P101_ERROR_RAISE_USER(err, "The report directory must not be empty.", ERR_USAGE);
        goto done;
    }

    if(args->resource_log == NULL)
    {
        join_path(env, err, resource_buf, resource_buf_size, args->report_dir, "resources.log");
        args->resource_log = resource_buf;
    }

    if(args->call_log == NULL)
    {
        join_path(env, err, call_buf, call_buf_size, args->report_dir, "calls.log");
        args->call_log = call_buf;
    }

    if(args->resource_log == NULL || args->resource_log[0] == '\0')
    {
        P101_ERROR_RAISE_USER(err, "The resource log path must not be empty.", ERR_USAGE);
        goto done;
    }

    if(args->call_log == NULL || args->call_log[0] == '\0')
    {
        P101_ERROR_RAISE_USER(err, "The call log path must not be empty.", ERR_USAGE);
        goto done;
    }

done:
    return;
}

static int run_report(const struct p101_env *env, struct p101_error *err, const struct arguments *args)
{
    struct report_model model;
    int                 ret_val;

    P101_TRACE(env);
    p101_memset(env, &model, 0, sizeof(model));
    ret_val = EXIT_TROUBLE;

    read_resources(env, err, args->resource_log, &model);
    read_calls(env, err, args->call_log, &model);

    if(p101_error_has_error(err))
    {
        goto done;
    }

    finalize_leaks(env, err, &model);

    if(p101_error_has_error(err))
    {
        goto done;
    }

    print_report(env, err, args, &model);

    if(p101_error_has_error(err))
    {
        goto done;
    }

    ret_val = model.finding_count == 0 ? EXIT_CLEAN : EXIT_FINDINGS;

done:
    free_model(env, &model);
    return ret_val;
}

static void read_resources(const struct p101_env *env, struct p101_error *err, const char *path, struct report_model *model)
{
    FILE *stream;
    bool  owned;
    char  line[LINE_MAX_BYTES];

    P101_TRACE(env);
    stream = open_input(env, err, path, &owned);

    while(p101_error_has_no_error(err) && p101_fgets(env, err, line, sizeof(line), stream) != NULL)
    {
        struct resource_event event;
        enum line_status      status;

        p101_memset(env, &event, 0, sizeof(event));
        status = parse_resource_line(env, err, line, &event, model, model->resource_records + 1U);

        switch(status)
        {
            case LINE_OTHER:
            {
                model->resource_skipped++;
                break;
            }
            case LINE_OK:
            {
                model->resource_records++;
                ingest_resource(env, err, model, &event);
                break;
            }
            case LINE_MALFORMED:
            {
                model->resource_malformed++;
                break;
            }
            case LINE_BAD_VERSION:
            {
                model->resource_bad_version++;
                break;
            }
        }

        free_resource_event(env, &event);
    }

    close_if_owned(env, err, stream, owned);
}

static void read_calls(const struct p101_env *env, struct p101_error *err, const char *path, struct report_model *model)
{
    FILE *stream;
    bool  owned;
    char  line[LINE_MAX_BYTES];

    P101_TRACE(env);
    stream = open_input(env, err, path, &owned);

    while(p101_error_has_no_error(err) && p101_fgets(env, err, line, sizeof(line), stream) != NULL)
    {
        struct call_event event;
        enum line_status  status;

        p101_memset(env, &event, 0, sizeof(event));
        status = parse_call_line(env, err, line, &event, model->call_records + 1U);

        switch(status)
        {
            case LINE_OTHER:
            {
                model->call_skipped++;
                break;
            }
            case LINE_OK:
            {
                model->call_records++;
                add_call(env, err, model, &event);
                p101_memset(env, &event, 0, sizeof(event));
                break;
            }
            case LINE_MALFORMED:
            {
                model->call_malformed++;
                break;
            }
            case LINE_BAD_VERSION:
            {
                model->call_bad_version++;
                break;
            }
        }

        free_call_event(env, &event);
    }

    close_if_owned(env, err, stream, owned);
}

static enum line_status parse_resource_line(const struct p101_env *env, struct p101_error *err, char *line, struct resource_event *event, struct report_model *model, size_t sequence)
{
    enum line_status status;
    char            *cursor;
    const char      *magic;
    const char      *version_text;
    const char      *pid_text;
    const char      *kind_text;
    const char      *primary_text;
    const char      *new_ptr_text;
    const char      *size_text;
    const char      *line_text;
    const char      *function_name;
    const char      *file_name;
    long             version;
    long             pid;
    long             fd_or_line;
    long             line_number;
    size_t           size;

    status = LINE_MALFORMED;

    if(!strip_line(env, line))
    {
        goto done;
    }

    if(!line_has_prefix(env, line, FD_PREFIX) && !line_has_prefix(env, line, ALLOC_PREFIX))
    {
        status = LINE_OTHER;
        goto done;
    }

    cursor       = line;
    magic        = split_tab(&cursor);
    version_text = split_tab(&cursor);
    pid_text     = split_tab(&cursor);
    kind_text    = split_tab(&cursor);

    if(!parse_long_field(version_text, LOG_VERSION, LOG_VERSION, &version))
    {
        if(parse_long_field(version_text, LONG_MIN, LONG_MAX, &version))
        {
            status = LINE_BAD_VERSION;
        }
        goto done;
    }

    if(!parse_long_field(pid_text, LONG_MIN, LONG_MAX, &pid))
    {
        goto done;
    }

    if(p101_strcmp(env, magic, "P101FD") == 0)
    {
        primary_text  = split_tab(&cursor);
        line_text     = split_tab(&cursor);
        function_name = split_tab(&cursor);
        file_name     = split_tab(&cursor);

        if(cursor != NULL || primary_text == NULL || line_text == NULL || function_name == NULL || file_name == NULL)
        {
            goto done;
        }

        if(!parse_long_field(primary_text, INT_MIN, INT_MAX, &fd_or_line) || !parse_long_field(line_text, INT_MIN, INT_MAX, &line_number))
        {
            goto done;
        }

        if(p101_strcmp(env, kind_text, "OPEN") == 0)
        {
            event->kind = RESOURCE_FD_OPEN;
        }
        else if(p101_strcmp(env, kind_text, "CLOSE") == 0)
        {
            event->kind = RESOURCE_FD_CLOSE;
        }
        else
        {
            goto done;
        }

        event->pid      = pid;
        event->fd       = (int)fd_or_line;
        event->site     = intern_site(env, err, model, file_name, function_name, (int)line_number);
        event->sequence = sequence;
        status          = LINE_OK;
    }
    else
    {
        primary_text  = split_tab(&cursor);
        new_ptr_text  = split_tab(&cursor);
        size_text     = split_tab(&cursor);
        line_text     = split_tab(&cursor);
        function_name = split_tab(&cursor);
        file_name     = split_tab(&cursor);

        if(cursor != NULL || primary_text == NULL || new_ptr_text == NULL || size_text == NULL || line_text == NULL || function_name == NULL || file_name == NULL)
        {
            goto done;
        }

        if(!parse_size_field(size_text, &size) || !parse_long_field(line_text, INT_MIN, INT_MAX, &line_number))
        {
            goto done;
        }

        if(p101_strcmp(env, kind_text, "ALLOC") == 0)
        {
            event->kind = RESOURCE_ALLOC;
        }
        else if(p101_strcmp(env, kind_text, "FREE") == 0)
        {
            event->kind = RESOURCE_FREE;
        }
        else if(p101_strcmp(env, kind_text, "REALLOC") == 0)
        {
            event->kind = RESOURCE_REALLOC;
        }
        else
        {
            goto done;
        }

        event->pid      = pid;
        event->ptr      = dup_text(env, err, primary_text);
        event->new_ptr  = dup_text(env, err, new_ptr_text);
        event->size     = size;
        event->site     = intern_site(env, err, model, file_name, function_name, (int)line_number);
        event->sequence = sequence;
        if(p101_error_has_no_error(err))
        {
            status = LINE_OK;
        }
    }

done:
    return status;
}

static enum line_status parse_call_line(const struct p101_env *env, struct p101_error *err, char *line, struct call_event *event, size_t sequence)
{
    enum line_status status;
    char            *cursor;
    const char      *magic;
    const char      *version_text;
    const char      *pid_text;
    const char      *kind_text;
    const char      *line_text;
    const char      *function_name;
    const char      *call_name;
    const char      *arguments;
    const char      *result;
    const char      *file_name;
    long             version;
    long             pid;
    long             line_number;

    status = LINE_MALFORMED;

    if(!strip_line(env, line))
    {
        goto done;
    }

    if(!line_has_prefix(env, line, CALL_PREFIX))
    {
        status = LINE_OTHER;
        goto done;
    }

    cursor        = line;
    magic         = split_tab(&cursor);
    version_text  = split_tab(&cursor);
    pid_text      = split_tab(&cursor);
    kind_text     = split_tab(&cursor);
    line_text     = split_tab(&cursor);
    function_name = split_tab(&cursor);
    call_name     = split_tab(&cursor);
    arguments     = split_tab(&cursor);
    result        = split_tab(&cursor);
    file_name     = split_tab(&cursor);

    if(cursor != NULL || magic == NULL || version_text == NULL || pid_text == NULL || kind_text == NULL || line_text == NULL || function_name == NULL || call_name == NULL || arguments == NULL || result == NULL || file_name == NULL)
    {
        goto done;
    }

    if(!parse_long_field(version_text, LOG_VERSION, LOG_VERSION, &version))
    {
        if(parse_long_field(version_text, LONG_MIN, LONG_MAX, &version))
        {
            status = LINE_BAD_VERSION;
        }
        goto done;
    }

    if(!parse_long_field(pid_text, LONG_MIN, LONG_MAX, &pid) || !parse_long_field(line_text, INT_MIN, INT_MAX, &line_number))
    {
        goto done;
    }

    if(p101_strcmp(env, kind_text, "ENTER") == 0)
    {
        event->kind = CALL_ENTER;
    }
    else if(p101_strcmp(env, kind_text, "EXIT") == 0)
    {
        event->kind = CALL_EXIT;
    }
    else
    {
        goto done;
    }

    event->pid           = pid;
    event->line_number   = (int)line_number;
    event->function_name = dup_text(env, err, function_name);
    event->call_name     = dup_text(env, err, call_name);
    event->arguments     = dup_text(env, err, arguments);
    event->result        = dup_text(env, err, result);
    event->file_name     = dup_text(env, err, file_name);
    event->sequence      = sequence;
    if(p101_error_has_no_error(err))
    {
        status = LINE_OK;
    }

done:
    return status;
}

static void ingest_resource(const struct p101_env *env, struct p101_error *err, struct report_model *model, const struct resource_event *event)
{
    size_t index;

    P101_TRACE(env);

    switch(event->kind)
    {
        case RESOURCE_FD_OPEN:
        {
            if(grow_array(env, err, (void **)&model->fds, &model->fd_capacity, model->fd_count, sizeof(*model->fds)))
            {
                model->fds[model->fd_count].pid      = event->pid;
                model->fds[model->fd_count].fd       = event->fd;
                model->fds[model->fd_count].site     = event->site;
                model->fds[model->fd_count].sequence = event->sequence;
                model->fd_count++;
            }
            break;
        }
        case RESOURCE_FD_CLOSE:
        {
            bool found;

            found = false;
            for(index = 0; index < model->fd_count; index++)
            {
                if(model->fds[index].pid == event->pid && model->fds[index].fd == event->fd)
                {
                    if(grow_array(env, err, (void **)&model->closed_fds, &model->closed_fd_capacity, model->closed_fd_count, sizeof(*model->closed_fds)))
                    {
                        model->closed_fds[model->closed_fd_count].pid      = event->pid;
                        model->closed_fds[model->closed_fd_count].fd       = event->fd;
                        model->closed_fds[model->closed_fd_count].site     = event->site;
                        model->closed_fds[model->closed_fd_count].sequence = event->sequence;
                        model->closed_fd_count++;
                    }
                    model->fds[index] = model->fds[model->fd_count - 1U];
                    model->fd_count--;
                    found = true;
                    break;
                }
            }

            if(!found)
            {
                struct finding finding;

                p101_memset(env, &finding, 0, sizeof(finding));
                finding.kind     = FINDING_STRAY_CLOSE;
                finding.pid      = event->pid;
                finding.fd       = event->fd;
                finding.site     = event->site;
                finding.sequence = event->sequence;

                for(index = 0; index < model->closed_fd_count; index++)
                {
                    if(model->closed_fds[index].pid == event->pid && model->closed_fds[index].fd == event->fd)
                    {
                        finding.kind              = FINDING_DOUBLE_CLOSE;
                        finding.previous_site     = model->closed_fds[index].site;
                        finding.previous_sequence = model->closed_fds[index].sequence;
                        break;
                    }
                }

                add_finding(env, err, model, &finding);
            }
            break;
        }
        case RESOURCE_ALLOC:
        {
            if(event->ptr != NULL && p101_strcmp(env, event->ptr, "-") != 0 && grow_array(env, err, (void **)&model->allocs, &model->alloc_capacity, model->alloc_count, sizeof(*model->allocs)))
            {
                model->allocs[model->alloc_count].pid      = event->pid;
                model->allocs[model->alloc_count].ptr      = dup_text(env, err, event->ptr);
                model->allocs[model->alloc_count].size     = event->size;
                model->allocs[model->alloc_count].site     = event->site;
                model->allocs[model->alloc_count].sequence = event->sequence;
                model->alloc_count++;
            }
            break;
        }
        case RESOURCE_FREE:
        {
            bool found;

            found = false;
            for(index = 0; index < model->alloc_count; index++)
            {
                if(model->allocs[index].pid == event->pid && p101_strcmp(env, model->allocs[index].ptr, event->ptr) == 0)
                {
                    if(grow_array(env, err, (void **)&model->freed_allocs, &model->freed_alloc_capacity, model->freed_alloc_count, sizeof(*model->freed_allocs)))
                    {
                        model->freed_allocs[model->freed_alloc_count].pid      = event->pid;
                        model->freed_allocs[model->freed_alloc_count].ptr      = dup_text(env, err, event->ptr);
                        model->freed_allocs[model->freed_alloc_count].site     = event->site;
                        model->freed_allocs[model->freed_alloc_count].sequence = event->sequence;
                        model->freed_alloc_count++;
                    }
                    p101_free(env, model->allocs[index].ptr);
                    model->allocs[index] = model->allocs[model->alloc_count - 1U];
                    model->alloc_count--;
                    found = true;
                    break;
                }
            }

            if(!found)
            {
                struct finding finding;

                p101_memset(env, &finding, 0, sizeof(finding));
                finding.kind     = FINDING_STRAY_FREE;
                finding.pid      = event->pid;
                finding.ptr      = event->ptr;
                finding.site     = event->site;
                finding.sequence = event->sequence;

                for(index = 0; index < model->freed_alloc_count; index++)
                {
                    if(model->freed_allocs[index].pid == event->pid && p101_strcmp(env, model->freed_allocs[index].ptr, event->ptr) == 0)
                    {
                        finding.kind              = FINDING_DOUBLE_FREE;
                        finding.previous_site     = model->freed_allocs[index].site;
                        finding.previous_sequence = model->freed_allocs[index].sequence;
                        break;
                    }
                }

                add_finding(env, err, model, &finding);
            }
            break;
        }
        case RESOURCE_REALLOC:
        {
            bool found;

            found = false;
            if(event->ptr == NULL || p101_strcmp(env, event->ptr, "-") == 0)
            {
                found = true;
            }

            for(index = 0; !found && index < model->alloc_count; index++)
            {
                if(model->allocs[index].pid == event->pid && p101_strcmp(env, model->allocs[index].ptr, event->ptr) == 0)
                {
                    p101_free(env, model->allocs[index].ptr);
                    model->allocs[index] = model->allocs[model->alloc_count - 1U];
                    model->alloc_count--;
                    found = true;
                }
            }

            if(!found)
            {
                struct finding finding;

                p101_memset(env, &finding, 0, sizeof(finding));
                finding.kind     = FINDING_BAD_REALLOC;
                finding.pid      = event->pid;
                finding.ptr      = event->ptr;
                finding.site     = event->site;
                finding.sequence = event->sequence;
                add_finding(env, err, model, &finding);
            }

            if(event->new_ptr != NULL && p101_strcmp(env, event->new_ptr, "-") != 0 && grow_array(env, err, (void **)&model->allocs, &model->alloc_capacity, model->alloc_count, sizeof(*model->allocs)))
            {
                model->allocs[model->alloc_count].pid      = event->pid;
                model->allocs[model->alloc_count].ptr      = dup_text(env, err, event->new_ptr);
                model->allocs[model->alloc_count].size     = event->size;
                model->allocs[model->alloc_count].site     = event->site;
                model->allocs[model->alloc_count].sequence = event->sequence;
                model->alloc_count++;
            }
            break;
        }
    }
}

static void finalize_leaks(const struct p101_env *env, struct p101_error *err, struct report_model *model)
{
    size_t index;

    P101_TRACE(env);

    for(index = 0; index < model->fd_count && p101_error_has_no_error(err); index++)
    {
        struct finding finding;

        p101_memset(env, &finding, 0, sizeof(finding));
        finding.kind     = FINDING_FD_LEAK;
        finding.pid      = model->fds[index].pid;
        finding.fd       = model->fds[index].fd;
        finding.site     = model->fds[index].site;
        finding.sequence = model->fds[index].sequence;
        add_finding(env, err, model, &finding);
    }

    for(index = 0; index < model->alloc_count && p101_error_has_no_error(err); index++)
    {
        struct finding finding;

        p101_memset(env, &finding, 0, sizeof(finding));
        finding.kind     = FINDING_ALLOC_LEAK;
        finding.pid      = model->allocs[index].pid;
        finding.ptr      = model->allocs[index].ptr;
        finding.size     = model->allocs[index].size;
        finding.site     = model->allocs[index].site;
        finding.sequence = model->allocs[index].sequence;
        add_finding(env, err, model, &finding);
    }
}

static void print_report(const struct p101_env *env, struct p101_error *err, const struct arguments *args, const struct report_model *model)
{
#ifdef __clang__
    #pragma clang diagnostic push
    #pragma clang diagnostic ignored "-Wcovered-switch-default"
#endif
    switch(args->format)
    {
        case REPORT_FORMAT_TEXT:
        {
            print_text_report(env, err, args, model);
            break;
        }
        case REPORT_FORMAT_JSON:
        {
            print_json_report(env, err, args, model);
            break;
        }
        default:
        {
            print_text_report(env, err, args, model);
            break;
        }
    }
#ifdef __clang__
    #pragma clang diagnostic pop
#endif
}

static void print_text_report(const struct p101_env *env, struct p101_error *err, const struct arguments *args, const struct report_model *model)
{
    size_t index;

    P101_TRACE(env);
    p101_printf(env, err, "p101-report\n");
    p101_printf(env, err, "resource log: %s\n", args->resource_log);
    p101_printf(env, err, "call log:     %s\n\n", args->call_log);

    p101_printf(env, err, "Summary\n");
    p101_printf(env, err, "  resource records: %zu parsed, %zu skipped, %zu malformed, %zu unsupported version\n", model->resource_records, model->resource_skipped, model->resource_malformed, model->resource_bad_version);
    p101_printf(env, err, "  call records:     %zu parsed, %zu skipped, %zu malformed, %zu unsupported version\n", model->call_records, model->call_skipped, model->call_malformed, model->call_bad_version);
    p101_printf(env, err, "  findings:         %zu\n\n", model->finding_count);

    if(model->finding_count == 0)
    {
        p101_printf(env, err, "No resource findings. Nice and boring.\n");
        goto done;
    }

    p101_printf(env, err, "Findings with trace context\n");
    for(index = 0; index < model->finding_count && p101_error_has_no_error(err); index++)
    {
        print_finding(env, err, model, &model->findings[index], index + 1U);
    }

done:
    return;
}

static void print_json_report(const struct p101_env *env, struct p101_error *err, const struct arguments *args, const struct report_model *model)
{
    p101_fputs(env, err, "{\n", stdout);
    p101_fputs(env, err, "  \"resource_log\": ", stdout);
    json_string(env, err, stdout, args->resource_log);
    p101_fputs(env, err, ",\n  \"call_log\": ", stdout);
    json_string(env, err, stdout, args->call_log);
    p101_fputs(env, err, ",\n  \"summary\": {\n", stdout);
    p101_printf(env, err, "    \"resource_records\": %zu,\n", model->resource_records);
    p101_printf(env, err, "    \"resource_skipped\": %zu,\n", model->resource_skipped);
    p101_printf(env, err, "    \"resource_malformed\": %zu,\n", model->resource_malformed);
    p101_printf(env, err, "    \"resource_bad_version\": %zu,\n", model->resource_bad_version);
    p101_printf(env, err, "    \"call_records\": %zu,\n", model->call_records);
    p101_printf(env, err, "    \"call_skipped\": %zu,\n", model->call_skipped);
    p101_printf(env, err, "    \"call_malformed\": %zu,\n", model->call_malformed);
    p101_printf(env, err, "    \"call_bad_version\": %zu,\n", model->call_bad_version);
    p101_printf(env, err, "    \"findings\": %zu\n", model->finding_count);
    p101_fputs(env, err, "  },\n  \"findings\": [", stdout);

    for(size_t i = 0; i < model->finding_count && p101_error_has_no_error(err); i++)
    {
        if(i > 0U)
        {
            p101_fputs(env, err, ",", stdout);
        }

        p101_fputs(env, err, "\n", stdout);
        print_json_finding(env, err, model, &model->findings[i]);
    }

    if(model->finding_count > 0U)
    {
        p101_fputs(env, err, "\n", stdout);
    }

    p101_fputs(env, err, "  ]\n}\n", stdout);
}

static void print_finding(const struct p101_env *env, struct p101_error *err, const struct report_model *model, const struct finding *finding, size_t ordinal)
{
    const struct source_site *site;

    site = &model->sites[finding->site];
    p101_printf(env, err, "%zu. %s\n", ordinal, finding_name(finding->kind));

    if(finding->kind == FINDING_FD_LEAK || finding->kind == FINDING_DOUBLE_CLOSE || finding->kind == FINDING_STRAY_CLOSE)
    {
        p101_printf(env, err, "   resource: fd %d, pid %ld, resource event #%zu\n", finding->fd, finding->pid, finding->sequence);
    }
    else
    {
        p101_printf(env, err, "   resource: ptr %s, %zu byte%s, pid %ld, resource event #%zu\n", finding->ptr == NULL ? "-" : finding->ptr, finding->size, finding->size == 1U ? "" : "s", finding->pid, finding->sequence);
    }

    p101_printf(env, err, "   site:     %s:%d in %s\n", site->file_name, site->line_number, site->function_name);

    if(finding->previous_sequence != 0U && finding->previous_site < model->site_count)
    {
        const struct source_site *previous;

        previous = &model->sites[finding->previous_site];
        p101_printf(env, err, "   previous: %s:%d in %s at resource event #%zu\n", previous->file_name, previous->line_number, previous->function_name, finding->previous_sequence);
    }

    print_trace_context(env, err, model, finding);
    p101_printf(env, err, "\n");
}

static void print_trace_context(const struct p101_env *env, struct p101_error *err, const struct report_model *model, const struct finding *finding)
{
    const struct source_site *site;
    size_t                    printed;
    size_t                    index;

    site    = &model->sites[finding->site];
    printed = 0;

    p101_printf(env, err, "   trace:\n");
    for(index = 0; index < model->call_count && printed < TRACE_CONTEXT_LIMIT && p101_error_has_no_error(err); index++)
    {
        const struct call_event *call;

        call = &model->calls[index];
        if(site_matches_call(env, site, call, finding->pid))
        {
            p101_printf(env, err, "     #%zu %s %s(%s) -> %s at %s:%d in %s\n", call->sequence, call->kind == CALL_ENTER ? "ENTER" : "EXIT ", call->call_name, call->arguments, call->result, call->file_name, call->line_number, call->function_name);
            printed++;
        }
    }

    if(printed == 0)
    {
        p101_printf(env, err, "     no matching call records for this source site\n");
    }
}

static void print_json_finding(const struct p101_env *env, struct p101_error *err, const struct report_model *model, const struct finding *finding)
{
    const struct source_site *site;

    site = &model->sites[finding->site];

    p101_fputs(env, err, "    {\"kind\": ", stdout);
    json_string(env, err, stdout, finding_name(finding->kind));
    p101_printf(env, err, ", \"pid\": %ld", finding->pid);

    if(finding->kind == FINDING_FD_LEAK || finding->kind == FINDING_DOUBLE_CLOSE || finding->kind == FINDING_STRAY_CLOSE)
    {
        p101_printf(env, err, ", \"fd\": %d", finding->fd);
    }
    else
    {
        p101_fputs(env, err, ", \"ptr\": ", stdout);
        json_string(env, err, stdout, finding->ptr == NULL ? "-" : finding->ptr);
        p101_printf(env, err, ", \"bytes\": %zu", finding->size);
    }

    p101_printf(env, err, ", \"resource_event\": %zu", finding->sequence);
    p101_fputs(env, err, ", \"site\": {\"file\": ", stdout);
    json_string(env, err, stdout, site->file_name);
    p101_printf(env, err, ", \"line\": %d, \"function\": ", site->line_number);
    json_string(env, err, stdout, site->function_name);
    p101_fputs(env, err, "}", stdout);

    if(finding->previous_sequence != 0U && finding->previous_site < model->site_count)
    {
        const struct source_site *previous;

        previous = &model->sites[finding->previous_site];
        p101_fprintf(env, err, stdout, ", \"previous\": {\"resource_event\": %zu, \"file\": ", finding->previous_sequence);
        json_string(env, err, stdout, previous->file_name);
        p101_printf(env, err, ", \"line\": %d, \"function\": ", previous->line_number);
        json_string(env, err, stdout, previous->function_name);
        p101_fputs(env, err, "}", stdout);
    }

    p101_fputs(env, err, ", \"trace\": [", stdout);
    print_json_trace_context(env, err, model, finding);
    p101_fputs(env, err, "]}", stdout);
}

static void print_json_trace_context(const struct p101_env *env, struct p101_error *err, const struct report_model *model, const struct finding *finding)
{
    const struct source_site *site;
    size_t                    printed;

    site    = &model->sites[finding->site];
    printed = 0;

    for(size_t i = 0; i < model->call_count && printed < TRACE_CONTEXT_LIMIT && p101_error_has_no_error(err); i++)
    {
        const struct call_event *call;

        call = &model->calls[i];

        if(!site_matches_call(env, site, call, finding->pid))
        {
            continue;
        }

        if(printed > 0U)
        {
            p101_fputs(env, err, ", ", stdout);
        }

        p101_printf(env, err, "{\"sequence\": %zu, \"event\": ", call->sequence);
        json_string(env, err, stdout, call->kind == CALL_ENTER ? "ENTER" : "EXIT");
        p101_fputs(env, err, ", \"call\": ", stdout);
        json_string(env, err, stdout, call->call_name);
        p101_fputs(env, err, ", \"arguments\": ", stdout);
        json_string(env, err, stdout, call->arguments);
        p101_fputs(env, err, ", \"result\": ", stdout);
        json_string(env, err, stdout, call->result);
        p101_fputs(env, err, ", \"file\": ", stdout);
        json_string(env, err, stdout, call->file_name);
        p101_printf(env, err, ", \"line\": %d, \"function\": ", call->line_number);
        json_string(env, err, stdout, call->function_name);
        p101_fputs(env, err, "}", stdout);
        printed++;
    }
}

static void json_string(const struct p101_env *env, struct p101_error *err, FILE *stream, const char *text)
{
    const unsigned char *cursor;

    p101_fputc(env, err, '\"', stream);

    cursor = (const unsigned char *)(text == NULL ? "" : text);
    while(*cursor != '\0' && p101_error_has_no_error(err))
    {
        if(*cursor == '\"' || *cursor == '\\')
        {
            p101_fputc(env, err, '\\', stream);
            p101_fputc(env, err, (int)*cursor, stream);
        }
        else if(*cursor == '\n')
        {
            p101_fputs(env, err, "\\n", stream);
        }
        else if(*cursor == '\r')
        {
            p101_fputs(env, err, "\\r", stream);
        }
        else if(*cursor == '\t')
        {
            p101_fputs(env, err, "\\t", stream);
        }
        else if(*cursor < JSON_CONTROL_LIMIT)
        {
            p101_fprintf(env, err, stream, "\\u%04x", (unsigned)*cursor);
        }
        else
        {
            p101_fputc(env, err, (int)*cursor, stream);
        }

        cursor++;
    }

    p101_fputc(env, err, '\"', stream);
}

static size_t intern_site(const struct p101_env *env, struct p101_error *err, struct report_model *model, const char *file_name, const char *function_name, int line_number)
{
    size_t index;

    for(index = 0; index < model->site_count; index++)
    {
        if(model->sites[index].line_number == line_number && p101_strcmp(env, model->sites[index].file_name, file_name) == 0 && p101_strcmp(env, model->sites[index].function_name, function_name) == 0)
        {
            goto done;
        }
    }

    if(grow_array(env, err, (void **)&model->sites, &model->site_capacity, model->site_count, sizeof(*model->sites)))
    {
        index                             = model->site_count;
        model->sites[index].file_name     = dup_text(env, err, file_name);
        model->sites[index].function_name = dup_text(env, err, function_name);
        model->sites[index].line_number   = line_number;
        model->site_count++;
    }
    else
    {
        index = 0;
    }

done:
    return index;
}

static void add_finding(const struct p101_env *env, struct p101_error *err, struct report_model *model, const struct finding *finding)
{
    if(grow_array(env, err, (void **)&model->findings, &model->finding_capacity, model->finding_count, sizeof(*model->findings)))
    {
        model->findings[model->finding_count] = *finding;
        if(finding->ptr != NULL)
        {
            model->findings[model->finding_count].ptr = dup_text(env, err, finding->ptr);
        }
        model->finding_count++;
    }
}

static void add_call(const struct p101_env *env, struct p101_error *err, struct report_model *model, const struct call_event *event)
{
    if(grow_array(env, err, (void **)&model->calls, &model->call_capacity, model->call_count, sizeof(*model->calls)))
    {
        model->calls[model->call_count] = *event;
        model->call_count++;
    }
}

static bool grow_array(const struct p101_env *env, struct p101_error *err, void **items, size_t *capacity, size_t count, size_t item_size)
{
    bool   ok;
    size_t new_capacity;
    void  *new_items;

    ok = true;

    if(count < *capacity)
    {
        goto done;
    }

    new_capacity = (*capacity == 0U) ? FIRST_CAPACITY : (*capacity * 2U);
    if(new_capacity < *capacity || (item_size != 0U && new_capacity > (SIZE_MAX / item_size)))
    {
        P101_ERROR_RAISE_ERRNO(err, ENOMEM);
        ok = false;
        goto done;
    }

    new_items = p101_realloc(env, err, *items, new_capacity * item_size);
    if(new_items == NULL)
    {
        ok = false;
        goto done;
    }

    *items    = new_items;
    *capacity = new_capacity;

done:
    return ok;
}

static char *dup_text(const struct p101_env *env, struct p101_error *err, const char *text)
{
    return p101_strdup(env, err, text == NULL ? "" : text);
}

static void free_model(const struct p101_env *env, struct report_model *model)
{
    size_t index;

    for(index = 0; index < model->site_count; index++)
    {
        p101_free(env, model->sites[index].file_name);
        p101_free(env, model->sites[index].function_name);
    }
    p101_free(env, model->sites);

    for(index = 0; index < model->call_count; index++)
    {
        free_call_event(env, &model->calls[index]);
    }
    p101_free(env, model->calls);

    for(index = 0; index < model->alloc_count; index++)
    {
        p101_free(env, model->allocs[index].ptr);
    }
    p101_free(env, model->allocs);

    for(index = 0; index < model->freed_alloc_count; index++)
    {
        p101_free(env, model->freed_allocs[index].ptr);
    }
    p101_free(env, model->freed_allocs);
    p101_free(env, model->fds);
    p101_free(env, model->closed_fds);

    for(index = 0; index < model->finding_count; index++)
    {
        p101_free(env, model->findings[index].ptr);
    }
    p101_free(env, model->findings);
}

static void free_resource_event(const struct p101_env *env, struct resource_event *event)
{
    p101_free(env, event->ptr);
    p101_free(env, event->new_ptr);
    event->ptr     = NULL;
    event->new_ptr = NULL;
}

static void free_call_event(const struct p101_env *env, struct call_event *event)
{
    p101_free(env, event->function_name);
    p101_free(env, event->call_name);
    p101_free(env, event->arguments);
    p101_free(env, event->result);
    p101_free(env, event->file_name);
    p101_memset(env, event, 0, sizeof(*event));
}

static char *split_tab(char **cursor)
{
    char *start;
    char *tab;

    start = *cursor;
    if(start == NULL)
    {
        goto done;
    }

    tab = start;
    while(*tab != '\0' && *tab != '\t')
    {
        tab++;
    }

    if(*tab == '\0')
    {
        *cursor = NULL;
    }
    else
    {
        *tab    = '\0';
        *cursor = tab + 1;
    }

done:
    return start;
}

static bool strip_line(const struct p101_env *env, char *line)
{
    size_t length;

    if(line == NULL)
    {
        return false;
    }

    length = p101_strlen(env, line);
    while(length > 0U && (line[length - 1U] == '\n' || line[length - 1U] == '\r'))
    {
        length--;
        line[length] = '\0';
    }

    return true;
}

static bool parse_long_field(const char *text, long min, long max, long *out)
{
    bool        ok;
    bool        negative;
    long        value;
    const char *cursor;

    ok     = false;
    value  = 0;
    cursor = text;

    if(cursor == NULL || *cursor == '\0')
    {
        goto done;
    }

    negative = (*cursor == '-');
    if(negative)
    {
        cursor++;
    }

    if(*cursor == '\0')
    {
        goto done;
    }

    while(*cursor != '\0')
    {
        int digit;

        if(*cursor < '0' || *cursor > '9')
        {
            goto done;
        }

        digit = *cursor - '0';
        if(value > (LONG_MAX - (long)digit) / DECIMAL_BASE)
        {
            goto done;
        }

        value = (value * DECIMAL_BASE) + digit;
        cursor++;
    }

    if(negative)
    {
        value = -value;
    }

    if(value < min || value > max)
    {
        goto done;
    }

    *out = value;
    ok   = true;

done:
    return ok;
}

static bool parse_size_field(const char *text, size_t *out)
{
    bool        ok;
    size_t      value;
    const char *cursor;

    ok     = false;
    value  = 0;
    cursor = text;

    if(cursor == NULL || *cursor == '\0')
    {
        goto done;
    }

    while(*cursor != '\0')
    {
        size_t digit;

        if(*cursor < '0' || *cursor > '9')
        {
            goto done;
        }

        digit = (size_t)(*cursor - '0');
        if(value > (SIZE_MAX - digit) / (size_t)DECIMAL_BASE)
        {
            goto done;
        }

        value = (value * (size_t)DECIMAL_BASE) + digit;
        cursor++;
    }

    *out = value;
    ok   = true;

done:
    return ok;
}

static bool line_has_prefix(const struct p101_env *env, const char *line, const char *prefix)
{
    return p101_strncmp(env, line, prefix, p101_strlen(env, prefix)) == 0;
}

static bool site_matches_call(const struct p101_env *env, const struct source_site *site, const struct call_event *call, long pid)
{
    bool result;

    result = false;

    if(call->pid != pid)
    {
        goto done;
    }

    if(site->line_number == call->line_number && p101_strcmp(env, site->file_name, call->file_name) == 0)
    {
        result = true;
        goto done;
    }

    if(p101_strcmp(env, site->file_name, call->file_name) == 0 && p101_strcmp(env, site->function_name, call->function_name) == 0)
    {
        result = true;
    }

done:
    return result;
}

static const char *finding_name(enum finding_kind kind)
{
    const char *name;

    switch(kind)
    {
        case FINDING_FD_LEAK:
        {
            name = "leaked descriptor";
            break;
        }
        case FINDING_ALLOC_LEAK:
        {
            name = "leaked allocation";
            break;
        }
        case FINDING_DOUBLE_CLOSE:
        {
            name = "double close";
            break;
        }
        case FINDING_STRAY_CLOSE:
        {
            name = "close of unknown descriptor";
            break;
        }
        case FINDING_DOUBLE_FREE:
        {
            name = "double free";
            break;
        }
        case FINDING_STRAY_FREE:
        {
            name = "free of unknown pointer";
            break;
        }
        case FINDING_BAD_REALLOC:
        {
            name = "realloc of unknown pointer";
            break;
        }
    }

    return name;
}

static int close_if_owned(const struct p101_env *env, struct p101_error *err, FILE *stream, bool owned)
{
    int ret_val;

    ret_val = 0;
    if(owned && stream != NULL)
    {
        ret_val = p101_fclose(env, err, stream);
    }

    return ret_val;
}

static FILE *open_input(const struct p101_env *env, struct p101_error *err, const char *path, bool *owned)
{
    FILE *stream;

    stream = NULL;
    *owned = false;

    if(path != NULL && p101_strcmp(env, path, "-") == 0)
    {
        stream = stdin;
    }
    else
    {
        stream = p101_fopen(env, err, path, "r");
        *owned = stream != NULL;
    }

    return stream;
}

static void join_path(const struct p101_env *env, struct p101_error *err, char *out, size_t out_size, const char *dir, const char *leaf)
{
    size_t dir_len;

    dir_len = p101_strlen(env, dir);
    if(dir_len > 0U && dir[dir_len - 1U] == '/')
    {
        p101_snprintf(env, err, out, out_size, "%s%s", dir, leaf);
    }
    else
    {
        p101_snprintf(env, err, out, out_size, "%s/%s", dir, leaf);
    }
}

_Noreturn static void usage(const struct p101_env *env, struct p101_error *err, const char *program_name, int exit_code, const char *message)
{
    FILE *stream;

    stream = (exit_code == EXIT_CLEAN) ? stdout : stderr;

    if(message != NULL)
    {
        p101_fprintf(env, err, stream, "%s\n\n", message);
    }

    p101_fprintf(env, err, stream, "Usage: %s [-h] [-v] [-j] [-d <report-dir>] [-r <resources.log> -c <calls.log>] [report-dir]\n", program_name);
    p101_fputs(env, err, "\n", stream);
    p101_fputs(env, err, "Correlate p101 resource and call logs into one report.\n", stream);
    p101_fputs(env, err, "\n", stream);
    p101_fputs(env, err, "Options:\n", stream);
    p101_fputs(env, err, "  -h                  show this help\n", stream);
    p101_fputs(env, err, "  -v                  trace p101-report itself\n", stream);
    p101_fputs(env, err, "  -j                  write JSON instead of the text report\n", stream);
    p101_fputs(env, err, "  -d <report-dir>     read resources.log and calls.log from a p101-observe report directory\n", stream);
    p101_fputs(env, err, "  -r <resources.log>  resource log to replay\n", stream);
    p101_fputs(env, err, "  -c <calls.log>      call log to correlate\n", stream);

    p101_exit(env, exit_code);
}
