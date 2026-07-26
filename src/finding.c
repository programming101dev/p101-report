#include "finding.h"
#include <p101_c/p101_string.h>

bool p101_report_site_matches_call(const struct p101_env *env, const struct source_site *site, const struct call_event *call, long pid)
{
    bool result;

    result = false;

    if(call->pid != pid)
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
