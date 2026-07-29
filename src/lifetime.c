#include "lifetime.h"
#include "constants.h"
#include "json.h"
#include <p101_c/p101_stdio.h>
#include <p101_c/p101_string.h>
#include <stdbool.h>
#include <stdio.h>

static const char                  *p101_report_resource_kind_name(enum resource_kind kind);
static const struct resource_event *p101_report_find_fd_lifetime_end(const struct p101_env *env, const struct report_model *model, const struct resource_event *birth);
static const struct resource_event *p101_report_find_alloc_lifetime_end(const struct p101_env *env, const struct report_model *model, const struct resource_event *birth);
static bool                         p101_report_lifetime_duration_ns(const struct resource_event *birth, const struct resource_event *death, size_t *duration_ns);
static void                         p101_report_print_json_optional_time(const struct p101_env *env, struct p101_error *err, const struct resource_event *event);
static void                         p101_report_print_text_timeline(const struct p101_env *env, struct p101_error *err, const char *label, const struct resource_event *birth, const struct resource_event *death);
static void                         p101_report_print_json_lifetime(const struct p101_env *env, struct p101_error *err, const struct report_model *model, const struct resource_event *birth);

static const char *p101_report_resource_kind_name(enum resource_kind kind)
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

static const struct resource_event *p101_report_find_fd_lifetime_end(const struct p101_env *env, const struct report_model *model, const struct resource_event *birth)
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

static const struct resource_event *p101_report_find_alloc_lifetime_end(const struct p101_env *env, const struct report_model *model, const struct resource_event *birth)
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

static void p101_report_print_text_timeline(const struct p101_env *env, struct p101_error *err, const char *label, const struct resource_event *birth, const struct resource_event *death)
{
    p101_printf(env, err, "     timeline: event %zu %s", birth->event_sequence, label);

    if(death == NULL)
    {
        p101_printf(env, err, " --------- still live\n");
    }
    else
    {
        size_t duration_ns;

        p101_printf(env, err, " --------- event %zu %s", death->event_sequence, p101_report_resource_kind_name(death->kind));
        if(p101_report_lifetime_duration_ns(birth, death, &duration_ns))
        {
            p101_printf(env, err, " (%zu ns)", duration_ns);
        }
        p101_printf(env, err, "\n");
    }
}

static bool p101_report_lifetime_duration_ns(const struct resource_event *birth, const struct resource_event *death, size_t *duration_ns)
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

static void p101_report_print_json_optional_time(const struct p101_env *env, struct p101_error *err, const struct resource_event *event)
{
    p101_printf(env, err, ", \"event_sequence\": %zu", event->event_sequence);

    if(event->monotonic_ns_available != 0)
    {
        p101_printf(env, err, ", \"monotonic_ns\": %zu", event->monotonic_ns);
    }
    else
    {
        p101_fputs(env, err, ", \"monotonic_ns\": null", stdout);
    }

    if(event->wall_unix_ns_available != 0)
    {
        p101_printf(env, err, ", \"wall_unix_ns\": %zu", event->wall_unix_ns);
    }
    else
    {
        p101_fputs(env, err, ", \"wall_unix_ns\": null", stdout);
    }
}

void p101_report_print_text_lifetimes(const struct p101_env *env, struct p101_error *err, const struct report_model *model)
{
    size_t printed;
    size_t total_lifetimes;

    printed         = 0U;
    total_lifetimes = 0U;
    p101_printf(env, err, "Resource lifetimes\n");

    for(size_t i = 0; i < model->resource_count && p101_error_has_no_error(err); i++)
    {
        const struct resource_event *birth;
        const struct resource_event *death;
        const struct source_site    *site;

        birth = &model->resources[i];
        site  = &model->sites[birth->site];

        if(birth->kind == RESOURCE_FD_OPEN)
        {
            total_lifetimes++;
            if(printed >= LIFETIME_TEXT_LIMIT)
            {
                continue;
            }

            death = p101_report_find_fd_lifetime_end(env, model, birth);
            p101_printf(env, err, "  fd %d pid %ld: event %zu opened at %s:%d in %s", birth->fd, birth->pid, birth->event_sequence, site->file_name, site->line_number, site->function_name);
            if(death == NULL)
            {
                p101_printf(env, err, " -> still live\n");
            }
            else
            {
                const struct source_site *death_site;

                death_site = &model->sites[death->site];
                p101_printf(env, err, " -> event %zu closed at %s:%d in %s\n", death->event_sequence, death_site->file_name, death_site->line_number, death_site->function_name);
            }
            p101_report_print_text_timeline(env, err, "fd-open", birth, death);
            printed++;
        }
        else if(birth->kind == RESOURCE_ALLOC || (birth->kind == RESOURCE_REALLOC && birth->new_ptr != NULL && p101_strcmp(env, birth->new_ptr, "-") != 0))
        {
            const char *ptr;

            total_lifetimes++;
            if(printed >= LIFETIME_TEXT_LIMIT)
            {
                continue;
            }

            ptr   = (birth->kind == RESOURCE_ALLOC) ? birth->ptr : birth->new_ptr;
            death = p101_report_find_alloc_lifetime_end(env, model, birth);
            p101_printf(env,
                        err,
                        "  ptr %s pid %ld: event %zu %s %zu byte%s at %s:%d in %s",
                        ptr == NULL ? "-" : ptr,
                        birth->pid,
                        birth->event_sequence,
                        p101_report_resource_kind_name(birth->kind),
                        birth->size,
                        birth->size == 1U ? "" : "s",
                        site->file_name,
                        site->line_number,
                        site->function_name);
            if(death == NULL)
            {
                p101_printf(env, err, " -> still live\n");
            }
            else
            {
                const struct source_site *death_site;

                death_site = &model->sites[death->site];
                p101_printf(env, err, " -> event %zu %s at %s:%d in %s\n", death->event_sequence, p101_report_resource_kind_name(death->kind), death_site->file_name, death_site->line_number, death_site->function_name);
            }
            p101_report_print_text_timeline(env, err, p101_report_resource_kind_name(birth->kind), birth, death);
            printed++;
        }
    }

    if(total_lifetimes == 0U)
    {
        p101_printf(env, err, "  no open/alloc lifetimes recorded\n");
    }
    else if(total_lifetimes > printed)
    {
        p101_printf(env, err, "  ... showing first %zu of %zu lifetimes\n", printed, total_lifetimes);
    }
    p101_printf(env, err, "\n");
}

void p101_report_print_json_lifetimes(const struct p101_env *env, struct p101_error *err, const struct report_model *model)
{
    size_t printed;

    printed = 0U;
    for(size_t i = 0; i < model->resource_count && p101_error_has_no_error(err); i++)
    {
        const struct resource_event *event;

        event = &model->resources[i];
        if(event->kind != RESOURCE_FD_OPEN && event->kind != RESOURCE_ALLOC && !(event->kind == RESOURCE_REALLOC && event->new_ptr != NULL && p101_strcmp(env, event->new_ptr, "-") != 0))
        {
            continue;
        }

        if(printed > 0U)
        {
            p101_fputs(env, err, ",", stdout);
        }

        p101_fputs(env, err, "\n", stdout);
        p101_report_print_json_lifetime(env, err, model, event);
        printed++;
    }
}

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
        p101_fputs(env, err, "    summary --> empty[\"no open/alloc lifetimes recorded\"]\n", stdout);
    }

    p101_fputs(env, err, "    classDef leak fill:#ffd6d6,stroke:#a33,stroke-width:2px\n", stdout);
}

static void p101_report_print_json_lifetime(const struct p101_env *env, struct p101_error *err, const struct report_model *model, const struct resource_event *birth)
{
    const struct source_site    *site;
    const struct resource_event *death;

    site = &model->sites[birth->site];
    p101_fputs(env, err, "    {\"resource\": ", stdout);

    if(birth->kind == RESOURCE_FD_OPEN)
    {
        death = p101_report_find_fd_lifetime_end(env, model, birth);
        p101_report_json_string(env, err, stdout, "fd");
        p101_printf(env, err, ", \"fd\": %d", birth->fd);
    }
    else
    {
        const char *ptr;

        death = p101_report_find_alloc_lifetime_end(env, model, birth);
        ptr   = (birth->kind == RESOURCE_ALLOC) ? birth->ptr : birth->new_ptr;
        p101_report_json_string(env, err, stdout, "allocation");
        p101_fputs(env, err, ", \"ptr\": ", stdout);
        p101_report_json_string(env, err, stdout, ptr == NULL ? "-" : ptr);
        p101_printf(env, err, ", \"bytes\": %zu", birth->size);
    }

    p101_printf(env, err, ", \"pid\": %ld, \"birth\": {\"event\": %zu, \"kind\": ", birth->pid, birth->sequence);
    p101_report_json_string(env, err, stdout, p101_report_resource_kind_name(birth->kind));
    p101_report_print_json_optional_time(env, err, birth);
    p101_fputs(env, err, ", \"file\": ", stdout);
    p101_report_json_string(env, err, stdout, site->file_name);
    p101_printf(env, err, ", \"line\": %d, \"function\": ", site->line_number);
    p101_report_json_string(env, err, stdout, site->function_name);
    p101_fputs(env, err, "}", stdout);

    if(death == NULL)
    {
        p101_fputs(env, err, ", \"death\": null, \"duration_ns\": null", stdout);
    }
    else
    {
        const struct source_site *death_site;
        size_t                    duration_ns;

        death_site = &model->sites[death->site];
        p101_printf(env, err, ", \"death\": {\"event\": %zu, \"kind\": ", death->sequence);
        p101_report_json_string(env, err, stdout, p101_report_resource_kind_name(death->kind));
        p101_report_print_json_optional_time(env, err, death);
        p101_fputs(env, err, ", \"file\": ", stdout);
        p101_report_json_string(env, err, stdout, death_site->file_name);
        p101_printf(env, err, ", \"line\": %d, \"function\": ", death_site->line_number);
        p101_report_json_string(env, err, stdout, death_site->function_name);
        p101_fputs(env, err, "}", stdout);

        if(p101_report_lifetime_duration_ns(birth, death, &duration_ns))
        {
            p101_printf(env, err, ", \"duration_ns\": %zu", duration_ns);
        }
        else
        {
            p101_fputs(env, err, ", \"duration_ns\": null", stdout);
        }
    }

    p101_fputs(env, err, "}", stdout);
}
