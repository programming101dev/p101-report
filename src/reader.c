#include "reader.h"
#include "constants.h"
#include "io.h"
#include "model.h"
#include "parse.h"
#include <p101_c/p101_stdio.h>
#include <p101_c/p101_string.h>
#include <stdio.h>

static bool p101_report_line_is_complete(const struct p101_env *env, struct p101_error *err, FILE *stream, char *line);

static bool p101_report_line_is_complete(const struct p101_env *env, struct p101_error *err, FILE *stream, char *line)
{
    bool   complete;
    size_t length;

    complete = true;
    length   = p101_strlen(env, line);

    if(length == LINE_MAX_BYTES - 1U && p101_strchr(env, line, '\n') == NULL)
    {
        char discard[LINE_MAX_BYTES];

        complete = false;
        while(p101_error_has_no_error(err) && p101_fgets(env, err, discard, sizeof(discard), stream) != NULL)
        {
            if(p101_strchr(env, discard, '\n') != NULL)
            {
                break;
            }
        }
    }

    return complete;
}

void p101_report_read_resources(const struct p101_env *env, struct p101_error *err, const char *path, struct report_model *model)
{
    FILE *stream;
    bool  owned;
    char  line[LINE_MAX_BYTES];

    P101_TRACE(env);
    stream = p101_report_open_input(env, err, path, &owned);

    while(p101_error_has_no_error(err) && p101_fgets(env, err, line, sizeof(line), stream) != NULL)
    {
        struct resource_event event;
        enum line_status      status;

        if(!p101_report_line_is_complete(env, err, stream, line))
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

    while(p101_error_has_no_error(err) && p101_fgets(env, err, line, sizeof(line), stream) != NULL)
    {
        struct call_event event;
        enum line_status  status;

        if(!p101_report_line_is_complete(env, err, stream, line))
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
