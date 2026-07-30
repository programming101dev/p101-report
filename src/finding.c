#include "finding.h"
#include <p101_c/p101_string.h>

bool p101_report_site_matches_call(const struct p101_env *env, const struct source_site *site, const struct call_event *call, long pid, size_t context_id)
{
    bool result;

    result = false;

    if(call->pid != pid || call->context_id != context_id)
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

const char *p101_report_finding_name(enum finding_kind kind)
{
    const char *name;

#ifdef __clang__
    #pragma clang diagnostic push
    #pragma clang diagnostic ignored "-Wcovered-switch-default"
#endif
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
        case FINDING_EXEC_INHERIT:
        {
            name = "descriptor inherited across exec";
            break;
        }
        case FINDING_RESOURCE_LEAK:
        {
            name = "leaked resource";
            break;
        }
        case FINDING_RESOURCE_DOUBLE_RELEASE:
        {
            name = "double release";
            break;
        }
        case FINDING_RESOURCE_STRAY_RELEASE:
        {
            name = "release of unknown resource";
            break;
        }
        case FINDING_RESOURCE_BAD_REPLACE:
        {
            name = "replacement without a new resource identifier";
            break;
        }
        case FINDING_RESOURCE_DUPLICATE_ACQUIRE:
        {
            name = "duplicate acquisition of a live resource";
            break;
        }
        default:
        {
            name = "unknown finding";
            break;
        }
    }
#ifdef __clang__
    #pragma clang diagnostic pop
#endif

    return name;
}

const char *p101_report_finding_id(enum finding_kind kind)
{
    const char *id;

#ifdef __clang__
    #pragma clang diagnostic push
    #pragma clang diagnostic ignored "-Wcovered-switch-default"
#endif
    switch(kind)
    {
        case FINDING_FD_LEAK:
        {
            id = "P101-FD-001";
            break;
        }
        case FINDING_DOUBLE_CLOSE:
        {
            id = "P101-FD-002";
            break;
        }
        case FINDING_STRAY_CLOSE:
        {
            id = "P101-FD-003";
            break;
        }
        case FINDING_ALLOC_LEAK:
        {
            id = "P101-ALLOC-001";
            break;
        }
        case FINDING_DOUBLE_FREE:
        {
            id = "P101-ALLOC-002";
            break;
        }
        case FINDING_STRAY_FREE:
        {
            id = "P101-ALLOC-003";
            break;
        }
        case FINDING_BAD_REALLOC:
        {
            id = "P101-ALLOC-004";
            break;
        }
        case FINDING_EXEC_INHERIT:
        {
            id = "P101-FD-004";
            break;
        }
        case FINDING_RESOURCE_LEAK:
        {
            id = "P101-RESOURCE-001";
            break;
        }
        case FINDING_RESOURCE_DOUBLE_RELEASE:
        {
            id = "P101-RESOURCE-002";
            break;
        }
        case FINDING_RESOURCE_STRAY_RELEASE:
        {
            id = "P101-RESOURCE-003";
            break;
        }
        case FINDING_RESOURCE_BAD_REPLACE:
        {
            id = "P101-RESOURCE-004";
            break;
        }
        case FINDING_RESOURCE_DUPLICATE_ACQUIRE:
        {
            id = "P101-RESOURCE-005";
            break;
        }
        default:
        {
            id = "P101-UNKNOWN-000";
            break;
        }
    }
#ifdef __clang__
    #pragma clang diagnostic pop
#endif

    return id;
}
