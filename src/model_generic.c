#include "model_generic.h"
#include "model_support.h"
#include <p101_c/p101_string.h>

static enum finding_kind p101_report_generic_finding_kind(p101_tool_event_lifecycle_finding_kind kind);
static enum finding_kind p101_report_unknown_generic_finding_kind(void);
static size_t            p101_report_find_generic_sequence(const struct p101_env *env, const struct report_model *model, const struct p101_tool_event_lifecycle_finding *finding, bool previous);

void p101_report_ingest_generic(const struct p101_env *env, struct p101_error *err, struct report_model *model, const struct resource_event *event)
{
    struct p101_tool_event_record record;
    const struct source_site     *site;

    if(model->generic_lifecycle == NULL)
    {
        model->generic_lifecycle = p101_tool_event_lifecycle_create(err);
        if(model->generic_lifecycle == NULL)
        {
            goto done;
        }
    }

    site = &model->sites[event->site];
    p101_memset(env, &record, 0, sizeof(record));
    record.record_kind            = P101_TOOL_EVENT_RECORD_RESOURCE;
    record.resource_kind          = event->generic_kind;
    record.pid                    = event->pid;
    record.context_id             = event->context_id;
    record.sequence               = event->event_sequence;
    record.monotonic_ns           = event->monotonic_ns;
    record.monotonic_ns_available = event->monotonic_ns_available;
    record.wall_unix_ns           = event->wall_unix_ns;
    record.wall_unix_ns_available = event->wall_unix_ns_available;
    record.resource_class         = event->resource_class;
    record.resource_id            = event->resource_id;
    record.related_id             = event->related_id;
    record.metadata               = event->metadata;
    record.size                   = event->size;
    record.line_number            = site->line_number;
    record.function_name          = site->function_name;
    record.file_name              = site->file_name;
    (void)p101_tool_event_lifecycle_ingest(err, model->generic_lifecycle, &record);

done:
    return;
}

void p101_report_finalize_generic(const struct p101_env *env, struct p101_error *err, struct report_model *model)
{
    size_t count;

    if(model->generic_lifecycle == NULL || model->generic_finalized != 0)
    {
        goto done;
    }

    if(p101_tool_event_lifecycle_finish(err, model->generic_lifecycle) != 0)
    {
        goto done;
    }

    count = p101_tool_event_lifecycle_finding_count(model->generic_lifecycle);
    for(size_t index = 0U; index < count && p101_error_has_no_error(err); index++)
    {
        const struct p101_tool_event_lifecycle_finding *source;
        struct finding                                  finding;

        source = p101_tool_event_lifecycle_finding_at(model->generic_lifecycle, index);
        if(source == NULL)
        {
            P101_ERROR_RAISE_CHECK(err);
            break;
        }

        p101_memset(env, &finding, 0, sizeof(finding));
        finding.kind           = p101_report_generic_finding_kind(source->kind);
        finding.pid            = source->pid;
        finding.resource_class = source->resource_class;
        finding.resource_id    = source->resource_id;
        finding.context_id     = source->context_id;
        finding.site           = p101_report_intern_site(env, err, model, source->file_name, source->function_name, source->line_number);
        finding.sequence       = p101_report_find_generic_sequence(env, model, source, false);
        if(source->previous_sequence != 0U)
        {
            finding.previous_site     = p101_report_intern_site(env, err, model, source->previous_file_name, source->previous_function_name, source->previous_line_number);
            finding.previous_sequence = p101_report_find_generic_sequence(env, model, source, true);
        }
        p101_report_add_finding_internal(env, err, model, &finding);
    }

    if(p101_error_has_no_error(err))
    {
        model->generic_finalized = 1;
    }

done:
    return;
}

static enum finding_kind p101_report_generic_finding_kind(p101_tool_event_lifecycle_finding_kind kind)
{
    enum finding_kind mapped;

#ifdef __clang__
    #pragma clang diagnostic push
    #pragma clang diagnostic ignored "-Wcovered-switch-default"
#endif
    switch(kind)
    {
        case P101_TOOL_EVENT_LIFECYCLE_FINDING_LEAK:
        {
            mapped = FINDING_RESOURCE_LEAK;
            break;
        }
        case P101_TOOL_EVENT_LIFECYCLE_FINDING_DOUBLE_RELEASE:
        {
            mapped = FINDING_RESOURCE_DOUBLE_RELEASE;
            break;
        }
        case P101_TOOL_EVENT_LIFECYCLE_FINDING_STRAY_RELEASE:
        {
            mapped = FINDING_RESOURCE_STRAY_RELEASE;
            break;
        }
        case P101_TOOL_EVENT_LIFECYCLE_FINDING_BAD_REPLACE:
        {
            mapped = FINDING_RESOURCE_BAD_REPLACE;
            break;
        }
        case P101_TOOL_EVENT_LIFECYCLE_FINDING_DUPLICATE_ACQUIRE:
        {
            mapped = FINDING_RESOURCE_DUPLICATE_ACQUIRE;
            break;
        }
        default:
        {
            mapped = p101_report_unknown_generic_finding_kind();
            break;
        }
    }
#ifdef __clang__
    #pragma clang diagnostic pop
#endif

    return mapped;
}

static enum finding_kind p101_report_unknown_generic_finding_kind(void)
{
    return FINDING_RESOURCE_BAD_REPLACE;
}

static size_t p101_report_find_generic_sequence(const struct p101_env *env, const struct report_model *model, const struct p101_tool_event_lifecycle_finding *finding, bool previous)
{
    size_t context_id;
    size_t event_sequence;

    if(previous)
    {
        context_id     = finding->previous_context_id;
        event_sequence = finding->previous_sequence;
    }
    else
    {
        context_id     = finding->context_id;
        event_sequence = finding->sequence;
    }
    for(size_t index = 0U; index < model->resource_count; index++)
    {
        const struct resource_event *event;

        event = &model->resources[index];
        if(event->kind != RESOURCE_GENERIC || event->pid != finding->pid || event->event_sequence != event_sequence || event->context_id != context_id)
        {
            continue;
        }
        if(event->resource_class != NULL && p101_strcmp(env, event->resource_class, finding->resource_class) == 0)
        {
            return event->sequence;
        }
    }
    return event_sequence;
}
