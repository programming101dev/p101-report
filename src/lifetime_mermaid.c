#include "lifetime_mermaid.h"
#include "constants.h"
#include "lifetime_common.h"
#include <p101_c/p101_stdio.h>
#include <p101_c/p101_string.h>
#include <stdio.h>

void p101_report_print_mermaid_lifetimes(const struct p101_env *env, struct p101_error *err, const struct report_model *model)
{
    size_t printed;

    printed = 0U;
    for(size_t i = 0; i < model->resource_count && p101_error_has_no_error(err); i++)
    {
        const struct resource_event *birth;
        const struct resource_event *death;
        const char                  *resource_name;

        birth = &model->resources[i];
        death = NULL;

        if(birth->kind == RESOURCE_FD_OPEN)
        {
            resource_name = "fd";
            death         = p101_report_find_fd_lifetime_end(env, model, birth);
        }
        else if(birth->kind == RESOURCE_ALLOC || (birth->kind == RESOURCE_REALLOC && birth->new_ptr != NULL && p101_strcmp(env, birth->new_ptr, "-") != 0))
        {
            resource_name = "alloc";
            death         = p101_report_find_alloc_lifetime_end(env, model, birth);
        }
        else if(birth->kind == RESOURCE_GENERIC &&
                (birth->generic_kind == P101_TOOL_EVENT_RESOURCE_ACQUIRE || ((birth->generic_kind == P101_TOOL_EVENT_RESOURCE_REPLACE || birth->generic_kind == P101_TOOL_EVENT_RESOURCE_TRANSFER) && birth->related_id != NULL)))
        {
            resource_name = birth->resource_class == NULL ? "resource" : birth->resource_class;
            death         = p101_report_find_generic_lifetime_end(env, model, birth);
        }
        else
        {
            continue;
        }

        if(printed >= LIFETIME_TEXT_LIMIT)
        {
            continue;
        }

        p101_printf(env, err, "    summary --> b%zu[\"event %zu %s %s\"]\n", birth->sequence, birth->event_sequence, resource_name, p101_report_resource_kind_name(birth->kind));

        if(death == NULL)
        {
            p101_printf(env, err, "    b%zu --> leak%zu[\"still live at exit\"]\n", birth->sequence, birth->sequence);
            p101_printf(env, err, "    leak%zu:::leak\n", birth->sequence);
        }
        else
        {
            size_t duration_ns;

            if(p101_report_lifetime_duration_ns(birth, death, &duration_ns))
            {
                p101_printf(env, err, "    b%zu --> d%zu[\"event %zu %s; %zu ns\"]\n", birth->sequence, death->sequence, death->event_sequence, p101_report_resource_kind_name(death->kind), duration_ns);
            }
            else
            {
                p101_printf(env, err, "    b%zu --> d%zu[\"event %zu %s\"]\n", birth->sequence, death->sequence, death->event_sequence, p101_report_resource_kind_name(death->kind));
            }
        }

        printed++;
    }

    if(printed == 0U)
    {
        p101_fputs(env, err, "    summary --> empty[\"no resource lifetimes recorded\"]\n", stdout);
    }

    p101_fputs(env, err, "    classDef leak fill:#ffd6d6,stroke:#a33,stroke-width:2px\n", stdout);
}
