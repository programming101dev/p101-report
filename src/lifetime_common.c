#include "lifetime_common.h"
#include <p101_c/p101_string.h>

const char *p101_report_resource_kind_name(enum resource_kind kind)
{
    const char *name;

#ifdef __clang__
    #pragma clang diagnostic push
    #pragma clang diagnostic ignored "-Wcovered-switch-default"
#endif
    switch(kind)
    {
        case RESOURCE_FD_OPEN:
        {
            name = "fd-open";
            break;
        }
        case RESOURCE_FD_CLOSE:
        {
            name = "fd-close";
            break;
        }
        case RESOURCE_ALLOC:
        {
            name = "alloc";
            break;
        }
        case RESOURCE_FREE:
        {
            name = "free";
            break;
        }
        case RESOURCE_REALLOC:
        {
            name = "realloc";
            break;
        }
        case RESOURCE_FORK:
        {
            name = "fork";
            break;
        }
        case RESOURCE_SPAWN:
        {
            name = "spawn";
            break;
        }
        case RESOURCE_EXEC:
        {
            name = "exec";
            break;
        }
        case RESOURCE_EXEC_FAIL:
        {
            name = "exec-fail";
            break;
        }
        case RESOURCE_GENERIC:
        {
            name = "resource";
            break;
        }
        default:
        {
            name = "unknown";
            break;
        }
    }
#ifdef __clang__
    #pragma clang diagnostic pop
#endif

    return name;
}

const struct resource_event *p101_report_find_fd_lifetime_end(const struct p101_env *env, const struct report_model *model, const struct resource_event *birth)
{
    const struct resource_event *match;

    match = NULL;
    for(size_t i = 0; i < model->resource_count; i++)
    {
        const struct resource_event *event;

        event = &model->resources[i];
        if(event->sequence > birth->sequence && event->kind == RESOURCE_FD_CLOSE && event->pid == birth->pid && event->fd == birth->fd)
        {
            match = event;
            break;
        }
    }

    (void)env;
    return match;
}

const struct resource_event *p101_report_find_alloc_lifetime_end(const struct p101_env *env, const struct report_model *model, const struct resource_event *birth)
{
    const struct resource_event *match;
    const char                  *ptr;

    match = NULL;
    ptr   = (birth->kind == RESOURCE_ALLOC) ? birth->ptr : birth->new_ptr;
    for(size_t i = 0; i < model->resource_count; i++)
    {
        const struct resource_event *event;

        event = &model->resources[i];
        if(event->sequence <= birth->sequence || event->pid != birth->pid || ptr == NULL)
        {
            continue;
        }

        if((event->kind == RESOURCE_FREE || event->kind == RESOURCE_REALLOC) && event->ptr != NULL && p101_strcmp(env, event->ptr, ptr) == 0)
        {
            match = event;
            break;
        }
    }

    return match;
}

const struct resource_event *p101_report_find_generic_lifetime_end(const struct p101_env *env, const struct report_model *model, const struct resource_event *birth)
{
    const char *resource_id;

    resource_id = birth->generic_kind == P101_TOOL_EVENT_RESOURCE_ACQUIRE ? birth->resource_id : birth->related_id;
    if(resource_id == NULL)
    {
        return NULL;
    }

    for(size_t index = 0U; index < model->resource_count; index++)
    {
        const struct resource_event *event;

        event = &model->resources[index];
        if(event->sequence <= birth->sequence || event->kind != RESOURCE_GENERIC || event->pid != birth->pid || event->resource_class == NULL || event->resource_id == NULL)
        {
            continue;
        }
        if(event->generic_kind != P101_TOOL_EVENT_RESOURCE_RELEASE && !((event->generic_kind == P101_TOOL_EVENT_RESOURCE_REPLACE || event->generic_kind == P101_TOOL_EVENT_RESOURCE_TRANSFER) && event->related_id != NULL))
        {
            continue;
        }
        if(p101_strcmp(env, event->resource_class, birth->resource_class) == 0 && p101_strcmp(env, event->resource_id, resource_id) == 0)
        {
            return event;
        }
    }

    return NULL;
}

bool p101_report_lifetime_duration_ns(const struct resource_event *birth, const struct resource_event *death, size_t *duration_ns)
{
    bool result;

    result = false;

    if(birth == NULL || death == NULL || duration_ns == NULL)
    {
        goto done;
    }

    if(birth->monotonic_ns_available == 0 || death->monotonic_ns_available == 0 || death->monotonic_ns < birth->monotonic_ns)
    {
        goto done;
    }

    *duration_ns = death->monotonic_ns - birth->monotonic_ns;
    result       = true;

done:
    return result;
}
