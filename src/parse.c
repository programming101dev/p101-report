#include "parse.h"
#include "constants.h"
#include "model.h"
#include "text.h"
#include <limits.h>
#include <p101_c/p101_string.h>
#include <stdlib.h>

enum line_status p101_report_parse_resource_line(const struct p101_env *env, struct p101_error *err, char *line, struct resource_event *event, struct report_model *model, size_t sequence)
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
    long             child_pid;
    long             fd_or_line;
    long             line_number;
    size_t           size;

    status = LINE_MALFORMED;

    if(!p101_report_strip_line(env, line))
    {
        goto done;
    }

    if(!p101_report_line_has_prefix(env, line, FD_PREFIX) && !p101_report_line_has_prefix(env, line, ALLOC_PREFIX) && !p101_report_line_has_prefix(env, line, FORK_PREFIX))
    {
        status = LINE_OTHER;
        goto done;
    }

    cursor       = line;
    magic        = p101_report_split_tab(&cursor);
    version_text = p101_report_split_tab(&cursor);
    pid_text     = p101_report_split_tab(&cursor);
    kind_text    = p101_report_split_tab(&cursor);

    if(!p101_report_parse_long_field(version_text, LOG_VERSION, LOG_VERSION, &version))
    {
        if(p101_report_parse_long_field(version_text, LONG_MIN, LONG_MAX, &version))
        {
            status = LINE_BAD_VERSION;
        }
        goto done;
    }

    if(!p101_report_parse_long_field(pid_text, LONG_MIN, LONG_MAX, &pid))
    {
        goto done;
    }

    if(p101_strcmp(env, magic, "P101FORK") == 0)
    {
        line_text     = p101_report_split_tab(&cursor);
        function_name = p101_report_split_tab(&cursor);
        file_name     = p101_report_split_tab(&cursor);

        if(cursor != NULL || kind_text == NULL || line_text == NULL || function_name == NULL || file_name == NULL)
        {
            goto done;
        }

        if(!p101_report_parse_long_field(kind_text, LONG_MIN, LONG_MAX, &child_pid) || !p101_report_parse_long_field(line_text, INT_MIN, INT_MAX, &line_number))
        {
            goto done;
        }

        event->kind      = RESOURCE_FORK;
        event->pid       = pid;
        event->child_pid = child_pid;
        event->site      = p101_report_intern_site(env, err, model, file_name, function_name, (int)line_number);
        event->sequence  = sequence;
        status           = LINE_OK;
    }
    else if(p101_strcmp(env, magic, "P101FD") == 0)
    {
        primary_text  = p101_report_split_tab(&cursor);
        line_text     = p101_report_split_tab(&cursor);
        function_name = p101_report_split_tab(&cursor);
        file_name     = p101_report_split_tab(&cursor);

        if(cursor != NULL || primary_text == NULL || line_text == NULL || function_name == NULL || file_name == NULL)
        {
            goto done;
        }

        if(!p101_report_parse_long_field(primary_text, INT_MIN, INT_MAX, &fd_or_line) || !p101_report_parse_long_field(line_text, INT_MIN, INT_MAX, &line_number))
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
        event->site     = p101_report_intern_site(env, err, model, file_name, function_name, (int)line_number);
        event->sequence = sequence;
        status          = LINE_OK;
    }
    else
    {
        primary_text  = p101_report_split_tab(&cursor);
        new_ptr_text  = p101_report_split_tab(&cursor);
        size_text     = p101_report_split_tab(&cursor);
        line_text     = p101_report_split_tab(&cursor);
        function_name = p101_report_split_tab(&cursor);
        file_name     = p101_report_split_tab(&cursor);

        if(cursor != NULL || primary_text == NULL || new_ptr_text == NULL || size_text == NULL || line_text == NULL || function_name == NULL || file_name == NULL)
        {
            goto done;
        }

        if(!p101_report_parse_size_field(size_text, &size) || !p101_report_parse_long_field(line_text, INT_MIN, INT_MAX, &line_number))
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
        event->ptr      = p101_report_dup_text(env, err, primary_text);
        event->new_ptr  = p101_report_dup_text(env, err, new_ptr_text);
        event->size     = size;
        event->site     = p101_report_intern_site(env, err, model, file_name, function_name, (int)line_number);
        event->sequence = sequence;
        if(p101_error_has_no_error(err))
        {
            status = LINE_OK;
        }
    }

done:
    return status;
}

enum line_status p101_report_parse_call_line(const struct p101_env *env, struct p101_error *err, char *line, struct call_event *event, size_t sequence)
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

    if(!p101_report_strip_line(env, line))
    {
        goto done;
    }

    if(!p101_report_line_has_prefix(env, line, CALL_PREFIX))
    {
        status = LINE_OTHER;
        goto done;
    }

    cursor        = line;
    magic         = p101_report_split_tab(&cursor);
    version_text  = p101_report_split_tab(&cursor);
    pid_text      = p101_report_split_tab(&cursor);
    kind_text     = p101_report_split_tab(&cursor);
    line_text     = p101_report_split_tab(&cursor);
    function_name = p101_report_split_tab(&cursor);
    call_name     = p101_report_split_tab(&cursor);
    arguments     = p101_report_split_tab(&cursor);
    result        = p101_report_split_tab(&cursor);
    file_name     = p101_report_split_tab(&cursor);

    if(cursor != NULL || magic == NULL || version_text == NULL || pid_text == NULL || kind_text == NULL || line_text == NULL || function_name == NULL || call_name == NULL || arguments == NULL || result == NULL || file_name == NULL)
    {
        goto done;
    }

    if(!p101_report_parse_long_field(version_text, LOG_VERSION, LOG_VERSION, &version))
    {
        if(p101_report_parse_long_field(version_text, LONG_MIN, LONG_MAX, &version))
        {
            status = LINE_BAD_VERSION;
        }
        goto done;
    }

    if(!p101_report_parse_long_field(pid_text, LONG_MIN, LONG_MAX, &pid) || !p101_report_parse_long_field(line_text, INT_MIN, INT_MAX, &line_number))
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
    event->function_name = p101_report_dup_text(env, err, function_name);
    event->call_name     = p101_report_dup_text(env, err, call_name);
    event->arguments     = p101_report_dup_text(env, err, arguments);
    event->result        = p101_report_dup_text(env, err, result);
    event->file_name     = p101_report_dup_text(env, err, file_name);
    event->sequence      = sequence;
    if(p101_error_has_no_error(err))
    {
        status = LINE_OK;
    }

done:
    return status;
}
