#include "reader.h"
#include "constants.h"
#include "io.h"
#include "memory.h"
#include "model.h"
#include "model_support.h"
#include "parse.h"
#include <p101_c/p101_string.h>
#include <p101_env/env.h>
#include <p101_tool_event/event.h>
#include <stdio.h>

void p101_report_read_resources(const struct p101_env *env, struct p101_error *err, const char *path, struct report_model *model)
{
    FILE *stream;
    bool  owned;
    char  line[P101_TOOL_EVENT_LINE_MAX_BYTES];

    P101_TRACE(env);
    stream = p101_report_open_input(env, err, path, &owned);

    while(p101_error_has_no_error(err))
    {
        struct resource_event       event;
        enum line_status            status;
        p101_tool_event_line_status line_status;

        line_status = p101_tool_event_read_line(err, stream, line, sizeof(line));

        if(line_status == P101_TOOL_EVENT_LINE_EOF)
        {
            break;
        }

        if(line_status == P101_TOOL_EVENT_LINE_ERROR)
        {
            break;
        }

        if(line_status == P101_TOOL_EVENT_LINE_MALFORMED)
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
    char  line[P101_TOOL_EVENT_LINE_MAX_BYTES];

    P101_TRACE(env);
    stream = p101_report_open_input(env, err, path, &owned);

    while(p101_error_has_no_error(err))
    {
        struct call_event           event;
        enum line_status            status;
        p101_tool_event_line_status line_status;

        line_status = p101_tool_event_read_line(err, stream, line, sizeof(line));

        if(line_status == P101_TOOL_EVENT_LINE_EOF)
        {
            break;
        }

        if(line_status == P101_TOOL_EVENT_LINE_ERROR)
        {
            break;
        }

        if(line_status == P101_TOOL_EVENT_LINE_MALFORMED)
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
