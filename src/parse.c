#include "parse.h"
#include "model.h"
#include <p101_c/p101_string.h>

static enum line_status map_parse_status(p101_env_event_parse_status status);
static void             copy_resource_metadata(const struct p101_env_event_record *record, struct resource_event *event, size_t sequence);
static void             copy_call_metadata(const struct p101_env_event_record *record, struct call_event *event, size_t sequence);
static enum line_status unknown_parse_status(void);

enum line_status p101_report_parse_resource_line(const struct p101_env *env, struct p101_error *err, char *line, struct resource_event *event, struct report_model *model, size_t sequence)
{
    enum line_status             status;
    p101_env_event_parse_status  parse_status;
    struct p101_env_event_record record;

    status = LINE_MALFORMED;

    if(line == NULL || event == NULL || model == NULL)
    {
        goto done;
    }

    parse_status = p101_env_parse_event_line(line, &record);
    status       = map_parse_status(parse_status);

    if(status != LINE_OK)
    {
        goto done;
    }

    copy_resource_metadata(&record, event, sequence);

#ifdef __clang__
    #pragma clang diagnostic push
    #pragma clang diagnostic ignored "-Wcovered-switch-default"
#endif
    switch(record.record_kind)
    {
        case P101_ENV_EVENT_RECORD_FD:
        {
            event->kind = (record.fd_kind == P101_ENV_EVENT_FD_OPEN) ? RESOURCE_FD_OPEN : RESOURCE_FD_CLOSE;
            event->fd   = record.fd;
            event->site = p101_report_intern_site(env, err, model, record.file_name, record.function_name, record.line_number);
            break;
        }
        case P101_ENV_EVENT_RECORD_ALLOC:
        {
            if(record.alloc_kind == P101_ENV_EVENT_ALLOC_ALLOC)
            {
                event->kind = RESOURCE_ALLOC;
            }
            else if(record.alloc_kind == P101_ENV_EVENT_ALLOC_FREE)
            {
                event->kind = RESOURCE_FREE;
            }
            else
            {
                event->kind = RESOURCE_REALLOC;
            }
            event->ptr     = p101_report_dup_text(env, err, record.ptr);
            event->new_ptr = p101_report_dup_text(env, err, record.new_ptr == NULL ? "-" : record.new_ptr);
            event->size    = record.size;
            event->site    = p101_report_intern_site(env, err, model, record.file_name, record.function_name, record.line_number);
            break;
        }
        case P101_ENV_EVENT_RECORD_FORK:
        {
            event->kind      = RESOURCE_FORK;
            event->child_pid = record.child_pid;
            event->site      = p101_report_intern_site(env, err, model, record.file_name, record.function_name, record.line_number);
            break;
        }
        case P101_ENV_EVENT_RECORD_SPAWN:
        {
            event->kind      = RESOURCE_SPAWN;
            event->child_pid = record.child_pid;
            event->target    = p101_report_dup_text(env, err, record.target);
            event->site      = p101_report_intern_site(env, err, model, record.file_name, record.function_name, record.line_number);
            break;
        }
        case P101_ENV_EVENT_RECORD_EXEC:
        {
            event->kind    = RESOURCE_EXEC;
            event->fd      = record.fd;
            event->cloexec = record.cloexec;
            event->target  = p101_report_dup_text(env, err, record.target);
            event->site    = p101_report_intern_site(env, err, model, record.file_name, record.function_name, record.line_number);
            break;
        }
        case P101_ENV_EVENT_RECORD_EXEC_FAIL:
        {
            event->kind   = RESOURCE_EXEC_FAIL;
            event->target = p101_report_dup_text(env, err, record.target);
            event->site   = p101_report_intern_site(env, err, model, record.file_name, record.function_name, record.line_number);
            break;
        }
        case P101_ENV_EVENT_RECORD_CALL:
        {
            status = LINE_OTHER;
            break;
        }
        default:
        {
            status = LINE_MALFORMED;
            break;
        }
    }
#ifdef __clang__
    #pragma clang diagnostic pop
#endif

    if(status == LINE_OK && p101_error_has_error(err))
    {
        status = LINE_MALFORMED;
    }

done:
    return status;
}

enum line_status p101_report_parse_call_line(const struct p101_env *env, struct p101_error *err, char *line, struct call_event *event, size_t sequence)
{
    enum line_status             status;
    p101_env_event_parse_status  parse_status;
    struct p101_env_event_record record;

    status = LINE_MALFORMED;

    if(line == NULL || event == NULL)
    {
        goto done;
    }

    parse_status = p101_env_parse_event_line(line, &record);
    status       = map_parse_status(parse_status);

    if(status != LINE_OK)
    {
        goto done;
    }

    if(record.record_kind != P101_ENV_EVENT_RECORD_CALL)
    {
        status = LINE_OTHER;
        goto done;
    }

    copy_call_metadata(&record, event, sequence);
    event->kind          = (record.call_kind == P101_ENV_EVENT_CALL_ENTER) ? CALL_ENTER : CALL_EXIT;
    event->line_number   = record.line_number;
    event->function_name = p101_report_dup_text(env, err, record.function_name);
    event->call_name     = p101_report_dup_text(env, err, record.call_name);
    event->arguments     = p101_report_dup_text(env, err, record.arguments);
    event->result        = p101_report_dup_text(env, err, record.result);
    event->file_name     = p101_report_dup_text(env, err, record.file_name);

    if(p101_error_has_error(err))
    {
        status = LINE_MALFORMED;
    }

done:
    return status;
}

static enum line_status map_parse_status(p101_env_event_parse_status status)
{
    enum line_status mapped;

#ifdef __clang__
    #pragma clang diagnostic push
    #pragma clang diagnostic ignored "-Wcovered-switch-default"
#endif
    switch(status)
    {
        case P101_ENV_EVENT_PARSE_OTHER:
        {
            mapped = LINE_OTHER;
            break;
        }
        case P101_ENV_EVENT_PARSE_OK:
        {
            mapped = LINE_OK;
            break;
        }
        case P101_ENV_EVENT_PARSE_BAD_VERSION:
        {
            mapped = LINE_BAD_VERSION;
            break;
        }
        case P101_ENV_EVENT_PARSE_MALFORMED:
        {
            mapped = LINE_MALFORMED;
            break;
        }
        default:
        {
            mapped = unknown_parse_status();
            break;
        }
    }
#ifdef __clang__
    #pragma clang diagnostic pop
#endif

    return mapped;
}

static enum line_status unknown_parse_status(void)
{
    return LINE_MALFORMED;
}

static void copy_resource_metadata(const struct p101_env_event_record *record, struct resource_event *event, size_t sequence)
{
    event->pid                    = record->pid;
    event->child_pid              = -1;
    event->sequence               = sequence;
    event->event_sequence         = record->sequence;
    event->monotonic_ns           = record->monotonic_ns;
    event->wall_unix_ns           = record->wall_unix_ns;
    event->monotonic_ns_available = record->monotonic_ns_available;
    event->wall_unix_ns_available = record->wall_unix_ns_available;
}

static void copy_call_metadata(const struct p101_env_event_record *record, struct call_event *event, size_t sequence)
{
    event->pid                    = record->pid;
    event->sequence               = sequence;
    event->event_sequence         = record->sequence;
    event->monotonic_ns           = record->monotonic_ns;
    event->wall_unix_ns           = record->wall_unix_ns;
    event->monotonic_ns_available = record->monotonic_ns_available;
    event->wall_unix_ns_available = record->wall_unix_ns_available;
}
