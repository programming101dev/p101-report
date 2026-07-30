#include "memory.h"
#include <p101_c/p101_stdlib.h>
#include <p101_c/p101_string.h>
#include <p101_posix/p101_string.h>

char *p101_report_dup_text(const struct p101_env *env, struct p101_error *err, const char *text)
{
    return p101_strdup(env, err, text == NULL ? "" : text);
}

void p101_report_free_model(const struct p101_env *env, struct report_model *model)
{
    size_t index;

    for(index = 0; index < model->site_count; index++)
    {
        p101_free(env, model->sites[index].file_name);
        p101_free(env, model->sites[index].function_name);
    }
    p101_free(env, model->sites);

    for(index = 0; index < model->resource_count; index++)
    {
        p101_report_free_resource_event(env, &model->resources[index]);
    }
    p101_free(env, model->resources);

    for(index = 0; index < model->call_count; index++)
    {
        p101_report_free_call_event(env, &model->calls[index]);
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
        p101_free(env, model->findings[index].resource_class);
        p101_free(env, model->findings[index].resource_id);
    }
    p101_free(env, model->findings);
    p101_tool_event_lifecycle_destroy(&model->generic_lifecycle);
    p101_tool_event_stream_health_destroy(&model->resource_stream_health);
    p101_tool_event_stream_health_destroy(&model->call_stream_health);
}

void p101_report_free_resource_event(const struct p101_env *env, struct resource_event *event)
{
    p101_free(env, event->ptr);
    p101_free(env, event->new_ptr);
    p101_free(env, event->target);
    p101_free(env, event->resource_class);
    p101_free(env, event->resource_id);
    p101_free(env, event->related_id);
    p101_free(env, event->metadata);
    event->ptr            = NULL;
    event->new_ptr        = NULL;
    event->target         = NULL;
    event->resource_class = NULL;
    event->resource_id    = NULL;
    event->related_id     = NULL;
    event->metadata       = NULL;
}

void p101_report_free_call_event(const struct p101_env *env, struct call_event *event)
{
    p101_free(env, event->function_name);
    p101_free(env, event->call_name);
    p101_free(env, event->arguments);
    p101_free(env, event->result);
    p101_free(env, event->file_name);
    p101_memset(env, event, 0, sizeof(*event));
}
