#include "output.h"
#include "constants.h"
#include "finding.h"
#include "json.h"
#include "lifetime.h"
#include <p101_c/p101_stdio.h>
#include <stdio.h>

void p101_report_print_report(const struct p101_env *env, struct p101_error *err, const struct arguments *args, const struct report_model *model)
{
#ifdef __clang__
    #pragma clang diagnostic push
    #pragma clang diagnostic ignored "-Wcovered-switch-default"
#endif
    switch(args->format)
    {
        case REPORT_FORMAT_TEXT:
        {
            p101_report_print_text_report(env, err, args, model);
            break;
        }
        case REPORT_FORMAT_JSON:
        {
            p101_report_print_json_report(env, err, args, model);
            break;
        }
        case REPORT_FORMAT_MERMAID:
        {
            p101_report_print_mermaid_report(env, err, args, model);
            break;
        }
        default:
        {
            p101_report_print_text_report(env, err, args, model);
            break;
        }
    }
#ifdef __clang__
    #pragma clang diagnostic pop
#endif
}

void p101_report_print_mermaid_report(const struct p101_env *env, struct p101_error *err, const struct arguments *args, const struct report_model *model)
{
    P101_TRACE(env);
    p101_fputs(env, err, "```mermaid\n", stdout);
    p101_fputs(env, err, "flowchart LR\n", stdout);
    p101_printf(env, err, "    summary[\"resources: %zu records; calls: %zu records; findings: %zu\"]\n", model->resource_records, model->call_records, model->finding_count);
    p101_report_print_mermaid_lifetimes(env, err, model);
    p101_fputs(env, err, "```\n", stdout);
    (void)args;
}

void p101_report_print_text_report(const struct p101_env *env, struct p101_error *err, const struct arguments *args, const struct report_model *model)
{
    size_t index;

    P101_TRACE(env);
    p101_printf(env, err, "p101-report\n");
    p101_printf(env, err, "event schema: p101-event-format-v1\n");
    p101_printf(env, err, "resource log: %s\n", args->resource_log);
    p101_printf(env, err, "call log:     %s\n\n", args->call_log);

    p101_printf(env, err, "Summary\n");
    p101_printf(env, err, "  resource records: %zu parsed, %zu skipped, %zu malformed, %zu unsupported version\n", model->resource_records, model->resource_skipped, model->resource_malformed, model->resource_bad_version);
    p101_printf(env, err, "  call records:     %zu parsed, %zu skipped, %zu malformed, %zu unsupported version\n", model->call_records, model->call_skipped, model->call_malformed, model->call_bad_version);
    p101_printf(env, err, "  findings:         %zu\n\n", model->finding_count);

    p101_report_print_text_lifetimes(env, err, model);

    if(model->finding_count == 0)
    {
        p101_printf(env, err, "No resource findings. Nice and boring.\n");
        goto done;
    }

    p101_printf(env, err, "Findings with trace context\n");
    for(index = 0; index < model->finding_count && p101_error_has_no_error(err); index++)
    {
        p101_report_print_finding(env, err, model, &model->findings[index], index + 1U);
    }

done:
    return;
}

void p101_report_print_json_report(const struct p101_env *env, struct p101_error *err, const struct arguments *args, const struct report_model *model)
{
    p101_fputs(env, err, "{\n", stdout);
    p101_fputs(env, err, "  \"event_schema\": \"p101-event-format-v1\",\n", stdout);
    p101_fputs(env, err, "  \"event_id_policy\": \"derived-1-based-input-sequence\",\n", stdout);
    p101_fputs(env, err, "  \"resource_log\": ", stdout);
    p101_report_json_string(env, err, stdout, args->resource_log);
    p101_fputs(env, err, ",\n  \"call_log\": ", stdout);
    p101_report_json_string(env, err, stdout, args->call_log);
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
    p101_fputs(env, err, "  },\n  \"lifetimes\": [", stdout);
    p101_report_print_json_lifetimes(env, err, model);
    p101_fputs(env, err, "\n  ],\n  \"findings\": [", stdout);

    for(size_t i = 0; i < model->finding_count && p101_error_has_no_error(err); i++)
    {
        if(i > 0U)
        {
            p101_fputs(env, err, ",", stdout);
        }

        p101_fputs(env, err, "\n", stdout);
        p101_report_print_json_finding(env, err, model, &model->findings[i]);
    }

    if(model->finding_count > 0U)
    {
        p101_fputs(env, err, "\n", stdout);
    }

    p101_fputs(env, err, "  ]\n}\n", stdout);
}

void p101_report_print_finding(const struct p101_env *env, struct p101_error *err, const struct report_model *model, const struct finding *finding, size_t ordinal)
{
    const struct source_site *site;

    site = &model->sites[finding->site];
    p101_printf(env, err, "%zu. %s\n", ordinal, p101_report_finding_name(finding->kind));

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

    p101_report_print_trace_context(env, err, model, finding);
    p101_printf(env, err, "\n");
}

void p101_report_print_trace_context(const struct p101_env *env, struct p101_error *err, const struct report_model *model, const struct finding *finding)
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
        if(p101_report_site_matches_call(env, site, call, finding->pid))
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

void p101_report_print_json_finding(const struct p101_env *env, struct p101_error *err, const struct report_model *model, const struct finding *finding)
{
    const struct source_site *site;

    site = &model->sites[finding->site];

    p101_fputs(env, err, "    {\"kind\": ", stdout);
    p101_report_json_string(env, err, stdout, p101_report_finding_name(finding->kind));
    p101_printf(env, err, ", \"pid\": %ld", finding->pid);

    if(finding->kind == FINDING_FD_LEAK || finding->kind == FINDING_DOUBLE_CLOSE || finding->kind == FINDING_STRAY_CLOSE)
    {
        p101_printf(env, err, ", \"fd\": %d", finding->fd);
    }
    else
    {
        p101_fputs(env, err, ", \"ptr\": ", stdout);
        p101_report_json_string(env, err, stdout, finding->ptr == NULL ? "-" : finding->ptr);
        p101_printf(env, err, ", \"bytes\": %zu", finding->size);
    }

    p101_printf(env, err, ", \"resource_event\": %zu", finding->sequence);
    p101_fputs(env, err, ", \"site\": {\"file\": ", stdout);
    p101_report_json_string(env, err, stdout, site->file_name);
    p101_printf(env, err, ", \"line\": %d, \"function\": ", site->line_number);
    p101_report_json_string(env, err, stdout, site->function_name);
    p101_fputs(env, err, "}", stdout);

    if(finding->previous_sequence != 0U && finding->previous_site < model->site_count)
    {
        const struct source_site *previous;

        previous = &model->sites[finding->previous_site];
        p101_fprintf(env, err, stdout, ", \"previous\": {\"resource_event\": %zu, \"file\": ", finding->previous_sequence);
        p101_report_json_string(env, err, stdout, previous->file_name);
        p101_printf(env, err, ", \"line\": %d, \"function\": ", previous->line_number);
        p101_report_json_string(env, err, stdout, previous->function_name);
        p101_fputs(env, err, "}", stdout);
    }

    p101_fputs(env, err, ", \"trace\": [", stdout);
    p101_report_print_json_trace_context(env, err, model, finding);
    p101_fputs(env, err, "]}", stdout);
}

void p101_report_print_json_trace_context(const struct p101_env *env, struct p101_error *err, const struct report_model *model, const struct finding *finding)
{
    const struct source_site *site;
    size_t                    printed;

    site    = &model->sites[finding->site];
    printed = 0;

    for(size_t i = 0; i < model->call_count && printed < TRACE_CONTEXT_LIMIT && p101_error_has_no_error(err); i++)
    {
        const struct call_event *call;

        call = &model->calls[i];

        if(!p101_report_site_matches_call(env, site, call, finding->pid))
        {
            continue;
        }

        if(printed > 0U)
        {
            p101_fputs(env, err, ", ", stdout);
        }

        p101_printf(env, err, "{\"sequence\": %zu, \"event\": ", call->sequence);
        p101_report_json_string(env, err, stdout, call->kind == CALL_ENTER ? "ENTER" : "EXIT");
        p101_fputs(env, err, ", \"call\": ", stdout);
        p101_report_json_string(env, err, stdout, call->call_name);
        p101_fputs(env, err, ", \"arguments\": ", stdout);
        p101_report_json_string(env, err, stdout, call->arguments);
        p101_fputs(env, err, ", \"result\": ", stdout);
        p101_report_json_string(env, err, stdout, call->result);
        p101_fputs(env, err, ", \"file\": ", stdout);
        p101_report_json_string(env, err, stdout, call->file_name);
        p101_printf(env, err, ", \"line\": %d, \"function\": ", call->line_number);
        p101_report_json_string(env, err, stdout, call->function_name);
        p101_fputs(env, err, "}", stdout);
        printed++;
    }
}
