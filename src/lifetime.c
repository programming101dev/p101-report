#include "lifetime.h"
#include "constants.h"
#include "json.h"
#include "lifetime_common.h"
#include <p101_c/p101_stdio.h>
#include <p101_c/p101_string.h>
#include <stdbool.h>
#include <stdio.h>

static const char *p101_report_generic_operation_name(p101_tool_event_resource_kind kind);
static const char *p101_report_generic_birth_id(const struct resource_event *birth);
static void        p101_report_print_json_optional_time(const struct p101_env *env, struct p101_error *err, const struct resource_event *event);
static void        p101_report_print_text_timeline(const struct p101_env *env, struct p101_error *err, const char *label, const struct resource_event *birth, const struct resource_event *death);
static void        p101_report_print_json_lifetime(const struct p101_env *env, struct p101_error *err, const struct report_model *model, const struct resource_event *birth);

static const char *p101_report_generic_operation_name(p101_tool_event_resource_kind kind)
{
    const char *name;

#ifdef __clang__
    #pragma clang diagnostic push
    #pragma clang diagnostic ignored "-Wcovered-switch-default"
#endif
    switch(kind)
    {
        case P101_TOOL_EVENT_RESOURCE_ACQUIRE:
        {
            name = "acquire";
            break;
        }
        case P101_TOOL_EVENT_RESOURCE_RELEASE:
        {
            name = "release";
            break;
        }
        case P101_TOOL_EVENT_RESOURCE_REPLACE:
        {
            name = "replace";
            break;
        }
        case P101_TOOL_EVENT_RESOURCE_TRANSFER:
        {
            name = "transfer";
            break;
        }
        default:
        {
            name = "resource";
            break;
        }
    }
#ifdef __clang__
    #pragma clang diagnostic pop
#endif

    return name;
}

static const char *p101_report_generic_birth_id(const struct resource_event *birth)
{
    if(birth->generic_kind == P101_TOOL_EVENT_RESOURCE_ACQUIRE)
    {
        return birth->resource_id;
    }
    return birth->related_id;
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
        else if(birth->kind == RESOURCE_GENERIC &&
                (birth->generic_kind == P101_TOOL_EVENT_RESOURCE_ACQUIRE || ((birth->generic_kind == P101_TOOL_EVENT_RESOURCE_REPLACE || birth->generic_kind == P101_TOOL_EVENT_RESOURCE_TRANSFER) && birth->related_id != NULL)))
        {
            const char *resource_id;

            total_lifetimes++;
            if(printed >= LIFETIME_TEXT_LIMIT)
            {
                continue;
            }

            resource_id = p101_report_generic_birth_id(birth);
            death       = p101_report_find_generic_lifetime_end(env, model, birth);
            p101_printf(env,
                        err,
                        "  %s %s pid %ld context %zu: event %zu %s at %s:%d in %s",
                        birth->resource_class == NULL ? "resource" : birth->resource_class,
                        resource_id == NULL ? "?" : resource_id,
                        birth->pid,
                        birth->context_id,
                        birth->event_sequence,
                        p101_report_generic_operation_name(birth->generic_kind),
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
                p101_printf(env, err, " -> event %zu %s at %s:%d in %s\n", death->event_sequence, p101_report_generic_operation_name(death->generic_kind), death_site->file_name, death_site->line_number, death_site->function_name);
            }
            p101_report_print_text_timeline(env, err, p101_report_generic_operation_name(birth->generic_kind), birth, death);
            printed++;
        }
    }

    if(total_lifetimes == 0U)
    {
        p101_printf(env, err, "  no resource lifetimes recorded\n");
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
        if(event->kind != RESOURCE_FD_OPEN && event->kind != RESOURCE_ALLOC && !(event->kind == RESOURCE_REALLOC && event->new_ptr != NULL && p101_strcmp(env, event->new_ptr, "-") != 0) &&
           !(event->kind == RESOURCE_GENERIC && (event->generic_kind == P101_TOOL_EVENT_RESOURCE_ACQUIRE || ((event->generic_kind == P101_TOOL_EVENT_RESOURCE_REPLACE || event->generic_kind == P101_TOOL_EVENT_RESOURCE_TRANSFER) && event->related_id != NULL))))
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
    else if(birth->kind == RESOURCE_ALLOC || birth->kind == RESOURCE_REALLOC)
    {
        const char *ptr;

        death = p101_report_find_alloc_lifetime_end(env, model, birth);
        ptr   = (birth->kind == RESOURCE_ALLOC) ? birth->ptr : birth->new_ptr;
        p101_report_json_string(env, err, stdout, "allocation");
        p101_fputs(env, err, ", \"ptr\": ", stdout);
        p101_report_json_string(env, err, stdout, ptr == NULL ? "-" : ptr);
        p101_printf(env, err, ", \"bytes\": %zu", birth->size);
    }
    else
    {
        const char *resource_id;

        death       = p101_report_find_generic_lifetime_end(env, model, birth);
        resource_id = p101_report_generic_birth_id(birth);
        p101_report_json_string(env, err, stdout, "generic");
        p101_fputs(env, err, ", \"resource_class\": ", stdout);
        p101_report_json_string(env, err, stdout, birth->resource_class == NULL ? "resource" : birth->resource_class);
        p101_fputs(env, err, ", \"resource_id\": ", stdout);
        p101_report_json_string(env, err, stdout, resource_id == NULL ? "?" : resource_id);
        p101_printf(env, err, ", \"context_id\": %zu", birth->context_id);
    }

    p101_printf(env, err, ", \"pid\": %ld, \"birth\": {\"event\": %zu, \"kind\": ", birth->pid, birth->sequence);
    p101_report_json_string(env, err, stdout, birth->kind == RESOURCE_GENERIC ? p101_report_generic_operation_name(birth->generic_kind) : p101_report_resource_kind_name(birth->kind));
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
        p101_report_json_string(env, err, stdout, death->kind == RESOURCE_GENERIC ? p101_report_generic_operation_name(death->generic_kind) : p101_report_resource_kind_name(death->kind));
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
