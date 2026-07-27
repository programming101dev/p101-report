#include "reader.h"
#include "constants.h"
#include "io.h"
#include "model.h"
#include "parse.h"
#include <p101_c/p101_stdio.h>
#include <p101_c/p101_string.h>
#include <stdio.h>

enum input_line_status
{
    INPUT_LINE_EOF = 0,
    INPUT_LINE_OK,
    INPUT_LINE_MALFORMED
};

static enum input_line_status p101_report_read_line(const struct p101_env *env, struct p101_error *err, FILE *stream, char *line, size_t line_size);

static enum input_line_status p101_report_read_line(const struct p101_env *env, struct p101_error *err, FILE *stream, char *line, size_t line_size)
{
    bool   saw_byte;
    bool   malformed;
    size_t length;

    saw_byte  = false;
    malformed = false;
    length    = 0U;

    while(p101_error_has_no_error(err))
    {
        int ch;

        ch = p101_fgetc(env, err, stream);

        if(ch == EOF)
        {
            break;
        }

        saw_byte = true;

        if(ch == '\0')
        {
            malformed = true;
        }

        if(length + 1U < line_size)
        {
            line[length] = (char)ch;
            length++;
        }
        else
        {
            malformed = true;
        }

        if(ch == '\n')
        {
            break;
        }
    }

    if(!saw_byte)
    {
        return INPUT_LINE_EOF;
    }

    line[(length < line_size) ? length : (line_size - 1U)] = '\0';

    if(malformed)
    {
        return INPUT_LINE_MALFORMED;
    }

    return INPUT_LINE_OK;
}

void p101_report_read_resources(const struct p101_env *env, struct p101_error *err, const char *path, struct report_model *model)
{
    FILE *stream;
    bool  owned;
    char  line[LINE_MAX_BYTES];

    P101_TRACE(env);
    stream = p101_report_open_input(env, err, path, &owned);

    while(p101_error_has_no_error(err))
    {
        struct resource_event  event;
        enum line_status       status;
        enum input_line_status line_status;

        line_status = p101_report_read_line(env, err, stream, line, sizeof(line));

        if(line_status == INPUT_LINE_EOF)
        {
            break;
        }

        if(line_status == INPUT_LINE_MALFORMED)
        {
            model->resource_malformed++;
            continue;
        }

        p101_memset(env, &event, 0, sizeof(event));
        status = p101_report_parse_resource_line(env, err, line, &event, model, model->resource_records + 1U);

#ifdef __clang__
    #pragma clang diagnostic push
    #pragma clang diagnostic ignored "-Wcovered-switch-default"
#endif
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
                p101_report_ingest_resource(env, err, model, &event);
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
            default:
            {
                model->resource_malformed++;
                break;
            }
        }
#ifdef __clang__
    #pragma clang diagnostic pop
#endif

        p101_report_free_resource_event(env, &event);
    }

    p101_report_close_if_owned(env, err, stream, owned);
}

void p101_report_read_calls(const struct p101_env *env, struct p101_error *err, const char *path, struct report_model *model)
{
    FILE *stream;
    bool  owned;
    char  line[LINE_MAX_BYTES];

    P101_TRACE(env);
    stream = p101_report_open_input(env, err, path, &owned);

    while(p101_error_has_no_error(err))
    {
        struct call_event      event;
        enum line_status       status;
        enum input_line_status line_status;

        line_status = p101_report_read_line(env, err, stream, line, sizeof(line));

        if(line_status == INPUT_LINE_EOF)
        {
            break;
        }

        if(line_status == INPUT_LINE_MALFORMED)
        {
            model->call_malformed++;
            continue;
        }

        p101_memset(env, &event, 0, sizeof(event));
        status = p101_report_parse_call_line(env, err, line, &event, model->call_records + 1U);

#ifdef __clang__
    #pragma clang diagnostic push
    #pragma clang diagnostic ignored "-Wcovered-switch-default"
#endif
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
                p101_report_add_call(env, err, model, &event);
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
            default:
            {
                model->call_malformed++;
                break;
            }
        }
#ifdef __clang__
    #pragma clang diagnostic pop
#endif

        p101_report_free_call_event(env, &event);
    }

    p101_report_close_if_owned(env, err, stream, owned);
}
