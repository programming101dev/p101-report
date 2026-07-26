#include "model.h"
#include "constants.h"
#include "finding.h"

static void p101_report_add_finding(const struct p101_env *env, struct p101_error *err, struct report_model *model, const struct finding *finding);
static void p101_report_add_resource_event(const struct p101_env *env, struct p101_error *err, struct report_model *model, const struct resource_event *event);
static bool p101_report_grow_array(const struct p101_env *env, struct p101_error *err, void **items, size_t *capacity, size_t count, size_t item_size);
#include <p101_c/p101_stdlib.h>
#include <p101_c/p101_string.h>
#include <p101_posix/p101_string.h>
#include <stdlib.h>

void p101_report_ingest_resource(const struct p101_env *env, struct p101_error *err, struct report_model *model, const struct resource_event *event)
{
    size_t index;

    P101_TRACE(env);
    p101_report_add_resource_event(env, err, model, event);

#ifdef __clang__
    #pragma clang diagnostic push
    #pragma clang diagnostic ignored "-Wcovered-switch-default"
#endif
    switch(event->kind)
    {
        case RESOURCE_FD_OPEN:
        {
            if(p101_report_grow_array(env, err, (void **)&model->fds, &model->fd_capacity, model->fd_count, sizeof(*model->fds)))
            {
                model->fds[model->fd_count].pid      = event->pid;
                model->fds[model->fd_count].fd       = event->fd;
                model->fds[model->fd_count].site     = event->site;
                model->fds[model->fd_count].sequence = event->sequence;
                model->fd_count++;
            }
            break;
        }
        case RESOURCE_FD_CLOSE:
        {
            bool found;

            found = false;
            for(index = 0; index < model->fd_count; index++)
            {
                if(model->fds[index].pid == event->pid && model->fds[index].fd == event->fd)
                {
                    if(p101_report_grow_array(env, err, (void **)&model->closed_fds, &model->closed_fd_capacity, model->closed_fd_count, sizeof(*model->closed_fds)))
                    {
                        model->closed_fds[model->closed_fd_count].pid      = event->pid;
                        model->closed_fds[model->closed_fd_count].fd       = event->fd;
                        model->closed_fds[model->closed_fd_count].site     = event->site;
                        model->closed_fds[model->closed_fd_count].sequence = event->sequence;
                        model->closed_fd_count++;
                    }
                    model->fds[index] = model->fds[model->fd_count - 1U];
                    model->fd_count--;
                    found = true;
                    break;
                }
            }

            if(!found)
            {
                struct finding finding;

                p101_memset(env, &finding, 0, sizeof(finding));
                finding.kind     = FINDING_STRAY_CLOSE;
                finding.pid      = event->pid;
                finding.fd       = event->fd;
                finding.site     = event->site;
                finding.sequence = event->sequence;

                for(index = 0; index < model->closed_fd_count; index++)
                {
                    if(model->closed_fds[index].pid == event->pid && model->closed_fds[index].fd == event->fd)
                    {
                        finding.kind              = FINDING_DOUBLE_CLOSE;
                        finding.previous_site     = model->closed_fds[index].site;
                        finding.previous_sequence = model->closed_fds[index].sequence;
                        break;
                    }
                }

                p101_report_add_finding(env, err, model, &finding);
            }
            break;
        }
        case RESOURCE_ALLOC:
        {
            if(event->ptr != NULL && p101_strcmp(env, event->ptr, "-") != 0 && p101_report_grow_array(env, err, (void **)&model->allocs, &model->alloc_capacity, model->alloc_count, sizeof(*model->allocs)))
            {
                model->allocs[model->alloc_count].pid      = event->pid;
                model->allocs[model->alloc_count].ptr      = p101_report_dup_text(env, err, event->ptr);
                model->allocs[model->alloc_count].size     = event->size;
                model->allocs[model->alloc_count].site     = event->site;
                model->allocs[model->alloc_count].sequence = event->sequence;
                model->alloc_count++;
            }
            break;
        }
        case RESOURCE_FREE:
        {
            bool found;

            found = false;
            for(index = 0; index < model->alloc_count; index++)
            {
                if(model->allocs[index].pid == event->pid && p101_strcmp(env, model->allocs[index].ptr, event->ptr) == 0)
                {
                    if(p101_report_grow_array(env, err, (void **)&model->freed_allocs, &model->freed_alloc_capacity, model->freed_alloc_count, sizeof(*model->freed_allocs)))
                    {
                        model->freed_allocs[model->freed_alloc_count].pid      = event->pid;
                        model->freed_allocs[model->freed_alloc_count].ptr      = p101_report_dup_text(env, err, event->ptr);
                        model->freed_allocs[model->freed_alloc_count].site     = event->site;
                        model->freed_allocs[model->freed_alloc_count].sequence = event->sequence;
                        model->freed_alloc_count++;
                    }
                    p101_free(env, model->allocs[index].ptr);
                    model->allocs[index] = model->allocs[model->alloc_count - 1U];
                    model->alloc_count--;
                    found = true;
                    break;
                }
            }

            if(!found)
            {
                struct finding finding;

                p101_memset(env, &finding, 0, sizeof(finding));
                finding.kind     = FINDING_STRAY_FREE;
                finding.pid      = event->pid;
                finding.ptr      = event->ptr;
                finding.site     = event->site;
                finding.sequence = event->sequence;

                for(index = 0; index < model->freed_alloc_count; index++)
                {
                    if(model->freed_allocs[index].pid == event->pid && p101_strcmp(env, model->freed_allocs[index].ptr, event->ptr) == 0)
                    {
                        finding.kind              = FINDING_DOUBLE_FREE;
                        finding.previous_site     = model->freed_allocs[index].site;
                        finding.previous_sequence = model->freed_allocs[index].sequence;
                        break;
                    }
                }

                p101_report_add_finding(env, err, model, &finding);
            }
            break;
        }
        case RESOURCE_REALLOC:
        {
            bool found;

            found = false;
            if(event->ptr == NULL || p101_strcmp(env, event->ptr, "-") == 0)
            {
                found = true;
            }

            for(index = 0; !found && index < model->alloc_count; index++)
            {
                if(model->allocs[index].pid == event->pid && p101_strcmp(env, model->allocs[index].ptr, event->ptr) == 0)
                {
                    p101_free(env, model->allocs[index].ptr);
                    model->allocs[index] = model->allocs[model->alloc_count - 1U];
                    model->alloc_count--;
                    found = true;
                }
            }

            if(!found)
            {
                struct finding finding;

                p101_memset(env, &finding, 0, sizeof(finding));
                finding.kind     = FINDING_BAD_REALLOC;
                finding.pid      = event->pid;
                finding.ptr      = event->ptr;
                finding.site     = event->site;
                finding.sequence = event->sequence;
                p101_report_add_finding(env, err, model, &finding);
            }

            if(event->new_ptr != NULL && p101_strcmp(env, event->new_ptr, "-") != 0 && p101_report_grow_array(env, err, (void **)&model->allocs, &model->alloc_capacity, model->alloc_count, sizeof(*model->allocs)))
            {
                model->allocs[model->alloc_count].pid      = event->pid;
                model->allocs[model->alloc_count].ptr      = p101_report_dup_text(env, err, event->new_ptr);
                model->allocs[model->alloc_count].size     = event->size;
                model->allocs[model->alloc_count].site     = event->site;
                model->allocs[model->alloc_count].sequence = event->sequence;
                model->alloc_count++;
            }
            break;
        }
        case RESOURCE_FORK:
        {
            size_t original_fd_count;

            original_fd_count = model->fd_count;
            for(index = 0; index < original_fd_count && p101_error_has_no_error(err); index++)
            {
                bool   child_has_live_fd;
                bool   child_closed_fd;
                size_t probe;

                if(model->fds[index].pid != event->pid)
                {
                    continue;
                }

                child_has_live_fd = false;
                for(probe = 0; probe < model->fd_count; probe++)
                {
                    if(model->fds[probe].pid == event->child_pid && model->fds[probe].fd == model->fds[index].fd)
                    {
                        child_has_live_fd = true;
                        break;
                    }
                }

                child_closed_fd = false;
                for(probe = 0; probe < model->closed_fd_count; probe++)
                {
                    if(model->closed_fds[probe].pid == event->child_pid && model->closed_fds[probe].fd == model->fds[index].fd)
                    {
                        child_closed_fd = true;
                        break;
                    }
                }

                if(!child_has_live_fd && !child_closed_fd && p101_report_grow_array(env, err, (void **)&model->fds, &model->fd_capacity, model->fd_count, sizeof(*model->fds)))
                {
                    model->fds[model->fd_count].pid      = event->child_pid;
                    model->fds[model->fd_count].fd       = model->fds[index].fd;
                    model->fds[model->fd_count].site     = model->fds[index].site;
                    model->fds[model->fd_count].sequence = event->sequence;
                    model->fd_count++;
                }
            }
            break;
        }
        case RESOURCE_EXEC:
        {
            if(event->cloexec == 0)
            {
                for(index = 0; index < model->fd_count; index++)
                {
                    if(model->fds[index].pid == event->pid && model->fds[index].fd == event->fd)
                    {
                        struct finding finding;

                        p101_memset(env, &finding, 0, sizeof(finding));
                        finding.kind              = FINDING_EXEC_INHERIT;
                        finding.pid               = event->pid;
                        finding.fd                = event->fd;
                        finding.ptr               = event->target;
                        finding.site              = event->site;
                        finding.sequence          = event->sequence;
                        finding.previous_site     = model->fds[index].site;
                        finding.previous_sequence = model->fds[index].sequence;
                        p101_report_add_finding(env, err, model, &finding);
                        break;
                    }
                }
            }
            break;
        }
        default:
        {
            break;
        }
    }
#ifdef __clang__
    #pragma clang diagnostic pop
#endif
}

void p101_report_finalize_leaks(const struct p101_env *env, struct p101_error *err, struct report_model *model)
{
    size_t index;

    P101_TRACE(env);

    for(index = 0; index < model->fd_count && p101_error_has_no_error(err); index++)
    {
        struct finding finding;

        p101_memset(env, &finding, 0, sizeof(finding));
        finding.kind     = FINDING_FD_LEAK;
        finding.pid      = model->fds[index].pid;
        finding.fd       = model->fds[index].fd;
        finding.site     = model->fds[index].site;
        finding.sequence = model->fds[index].sequence;
        p101_report_add_finding(env, err, model, &finding);
    }

    for(index = 0; index < model->alloc_count && p101_error_has_no_error(err); index++)
    {
        struct finding finding;

        p101_memset(env, &finding, 0, sizeof(finding));
        finding.kind     = FINDING_ALLOC_LEAK;
        finding.pid      = model->allocs[index].pid;
        finding.ptr      = model->allocs[index].ptr;
        finding.size     = model->allocs[index].size;
        finding.site     = model->allocs[index].site;
        finding.sequence = model->allocs[index].sequence;
        p101_report_add_finding(env, err, model, &finding);
    }
}

size_t p101_report_intern_site(const struct p101_env *env, struct p101_error *err, struct report_model *model, const char *file_name, const char *function_name, int line_number)
{
    size_t index;

    for(index = 0; index < model->site_count; index++)
    {
        if(model->sites[index].line_number == line_number && p101_strcmp(env, model->sites[index].file_name, file_name) == 0 && p101_strcmp(env, model->sites[index].function_name, function_name) == 0)
        {
            goto done;
        }
    }

    if(p101_report_grow_array(env, err, (void **)&model->sites, &model->site_capacity, model->site_count, sizeof(*model->sites)))
    {
        index                             = model->site_count;
        model->sites[index].file_name     = p101_report_dup_text(env, err, file_name);
        model->sites[index].function_name = p101_report_dup_text(env, err, function_name);
        model->sites[index].line_number   = line_number;
        model->site_count++;
    }
    else
    {
        index = 0;
    }

done:
    return index;
}

static void p101_report_add_resource_event(const struct p101_env *env, struct p101_error *err, struct report_model *model, const struct resource_event *event)
{
    struct resource_event *copy;

    if(!p101_report_grow_array(env, err, (void **)&model->resources, &model->resource_capacity, model->resource_count, sizeof(*model->resources)))
    {
        goto done;
    }

    copy  = &model->resources[model->resource_count];
    *copy = *event;
    if(event->ptr != NULL)
    {
        copy->ptr = p101_report_dup_text(env, err, event->ptr);
    }
    if(event->new_ptr != NULL)
    {
        copy->new_ptr = p101_report_dup_text(env, err, event->new_ptr);
    }
    if(event->target != NULL)
    {
        copy->target = p101_report_dup_text(env, err, event->target);
    }
    model->resource_count++;

done:
    return;
}

static void p101_report_add_finding(const struct p101_env *env, struct p101_error *err, struct report_model *model, const struct finding *finding)
{
    if(p101_report_grow_array(env, err, (void **)&model->findings, &model->finding_capacity, model->finding_count, sizeof(*model->findings)))
    {
        model->findings[model->finding_count] = *finding;
        if(finding->ptr != NULL)
        {
            model->findings[model->finding_count].ptr = p101_report_dup_text(env, err, finding->ptr);
        }
        model->finding_count++;
    }
}

void p101_report_add_call(const struct p101_env *env, struct p101_error *err, struct report_model *model, const struct call_event *event)
{
    if(p101_report_grow_array(env, err, (void **)&model->calls, &model->call_capacity, model->call_count, sizeof(*model->calls)))
    {
        model->calls[model->call_count] = *event;
        model->call_count++;
    }
}

static bool p101_report_grow_array(const struct p101_env *env, struct p101_error *err, void **items, size_t *capacity, size_t count, size_t item_size)
{
    bool   ok;
    size_t new_capacity;
    void  *new_items;

    ok = true;

    if(count < *capacity)
    {
        goto done;
    }

    new_capacity = (*capacity == 0U) ? FIRST_CAPACITY : (*capacity * 2U);
    if(new_capacity < *capacity || (item_size != 0U && new_capacity > (SIZE_MAX / item_size)))
    {
        P101_ERROR_RAISE_ERRNO(err, ENOMEM);
        ok = false;
        goto done;
    }

    new_items = p101_realloc(env, err, *items, new_capacity * item_size);
    if(new_items == NULL)
    {
        ok = false;
        goto done;
    }

    *items    = new_items;
    *capacity = new_capacity;

done:
    return ok;
}

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
    }
    p101_free(env, model->findings);
}

void p101_report_free_resource_event(const struct p101_env *env, struct resource_event *event)
{
    p101_free(env, event->ptr);
    p101_free(env, event->new_ptr);
    p101_free(env, event->target);
    event->ptr     = NULL;
    event->new_ptr = NULL;
    event->target  = NULL;
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
