#include "model.h"
#include "constants.h"
#include "finding.h"
#include "memory.h"
#include "model_generic.h"
#include "model_support.h"
#include <p101_tool_event/ownership.h>
#include <stdint.h>

static void p101_report_add_resource_event(const struct p101_env *env, struct p101_error *err, struct report_model *model, const struct resource_event *event);
static void p101_report_finish_exec(const struct p101_env *env, struct p101_error *err, struct report_model *model, long pid, bool end_of_input);
static void p101_report_rollback_failed_exec(const struct p101_env *env, struct report_model *model, const struct resource_event *event);
#include <p101_c/p101_stdlib.h>
#include <p101_c/p101_string.h>
#include <p101_posix/p101_string.h>
#include <stdlib.h>

void p101_report_ingest_resource(const struct p101_env *env, struct p101_error *err, struct report_model *model, const struct resource_event *event)
{
    size_t index;

    P101_TRACE(env);
    if(event->kind != RESOURCE_EXEC && event->kind != RESOURCE_EXEC_FAIL)
    {
        p101_report_finish_exec(env, err, model, event->pid, false);
    }
    p101_report_add_resource_event(env, err, model, event);

#ifdef __clang__
    #pragma clang diagnostic push
    #pragma clang diagnostic ignored "-Wcovered-switch-default"
#endif
    switch(event->kind)
    {
        case RESOURCE_FD_OPEN:
        {
            if(p101_report_grow_array_internal(env, err, (void **)&model->fds, &model->fd_capacity, model->fd_count, sizeof(*model->fds)))
            {
                p101_memset(env, &model->fds[model->fd_count], 0, sizeof(*model->fds));
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
            bool                                     found;
            p101_tool_event_ownership_release_result release_result;
            p101_tool_event_ownership_state          ownership_state;

            found = false;
            for(index = 0; index < model->fd_count; index++)
            {
                if(model->fds[index].pid == event->pid && model->fds[index].fd == event->fd)
                {
                    if(p101_report_grow_array_internal(env, err, (void **)&model->closed_fds, &model->closed_fd_capacity, model->closed_fd_count, sizeof(*model->closed_fds)))
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

            ownership_state = P101_TOOL_EVENT_OWNERSHIP_NEVER;
            if(found)
            {
                ownership_state = P101_TOOL_EVENT_OWNERSHIP_LIVE;
            }
            release_result = p101_tool_event_ownership_classify_release(ownership_state);
            if(!found)
            {
                struct finding finding;

                p101_memset(env, &finding, 0, sizeof(finding));
                finding.pid      = event->pid;
                finding.fd       = event->fd;
                finding.site     = event->site;
                finding.sequence = event->sequence;

                for(index = 0; index < model->closed_fd_count; index++)
                {
                    if(model->closed_fds[index].pid == event->pid && model->closed_fds[index].fd == event->fd)
                    {
                        finding.previous_site     = model->closed_fds[index].site;
                        finding.previous_sequence = model->closed_fds[index].sequence;
                        release_result            = p101_tool_event_ownership_classify_release(P101_TOOL_EVENT_OWNERSHIP_RELEASED);
                        break;
                    }
                }

                finding.kind = release_result == P101_TOOL_EVENT_OWNERSHIP_RELEASE_DUPLICATE ? FINDING_DOUBLE_CLOSE : FINDING_STRAY_CLOSE;
                p101_report_add_finding_internal(env, err, model, &finding);
            }
            break;
        }
        case RESOURCE_ALLOC:
        {
            if(event->ptr != NULL && p101_strcmp(env, event->ptr, "-") != 0 && p101_report_grow_array_internal(env, err, (void **)&model->allocs, &model->alloc_capacity, model->alloc_count, sizeof(*model->allocs)))
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
            bool                                     found;
            p101_tool_event_ownership_release_result release_result;
            p101_tool_event_ownership_state          ownership_state;

            found = false;
            for(index = 0; index < model->alloc_count; index++)
            {
                if(model->allocs[index].pid == event->pid && p101_strcmp(env, model->allocs[index].ptr, event->ptr) == 0)
                {
                    if(p101_report_grow_array_internal(env, err, (void **)&model->freed_allocs, &model->freed_alloc_capacity, model->freed_alloc_count, sizeof(*model->freed_allocs)))
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

            ownership_state = P101_TOOL_EVENT_OWNERSHIP_NEVER;
            if(found)
            {
                ownership_state = P101_TOOL_EVENT_OWNERSHIP_LIVE;
            }
            release_result = p101_tool_event_ownership_classify_release(ownership_state);
            if(!found)
            {
                struct finding finding;

                p101_memset(env, &finding, 0, sizeof(finding));
                finding.pid      = event->pid;
                finding.ptr      = event->ptr;
                finding.site     = event->site;
                finding.sequence = event->sequence;

                for(index = 0; index < model->freed_alloc_count; index++)
                {
                    if(model->freed_allocs[index].pid == event->pid && p101_strcmp(env, model->freed_allocs[index].ptr, event->ptr) == 0)
                    {
                        finding.previous_site     = model->freed_allocs[index].site;
                        finding.previous_sequence = model->freed_allocs[index].sequence;
                        release_result            = p101_tool_event_ownership_classify_release(P101_TOOL_EVENT_OWNERSHIP_RELEASED);
                        break;
                    }
                }

                finding.kind = release_result == P101_TOOL_EVENT_OWNERSHIP_RELEASE_DUPLICATE ? FINDING_DOUBLE_FREE : FINDING_STRAY_FREE;
                p101_report_add_finding_internal(env, err, model, &finding);
            }
            break;
        }
        case RESOURCE_REALLOC:
        {
            bool                                     found;
            bool                                     source_is_null;
            p101_tool_event_ownership_replace_result replace_result;
            p101_tool_event_ownership_state          ownership_state;

            source_is_null = false;
            if(event->ptr == NULL || p101_strcmp(env, event->ptr, "-") == 0)
            {
                source_is_null = true;
            }
            found = false;
            if(source_is_null)
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

            ownership_state = P101_TOOL_EVENT_OWNERSHIP_NEVER;
            if(found)
            {
                ownership_state = P101_TOOL_EVENT_OWNERSHIP_LIVE;
            }
            replace_result = p101_tool_event_ownership_classify_replace(source_is_null, ownership_state);
            if(replace_result == P101_TOOL_EVENT_OWNERSHIP_REPLACE_BAD)
            {
                struct finding finding;

                p101_memset(env, &finding, 0, sizeof(finding));
                finding.kind     = FINDING_BAD_REALLOC;
                finding.pid      = event->pid;
                finding.ptr      = event->ptr;
                finding.site     = event->site;
                finding.sequence = event->sequence;
                p101_report_add_finding_internal(env, err, model, &finding);
            }

            if(event->new_ptr != NULL && p101_strcmp(env, event->new_ptr, "-") != 0 && p101_report_grow_array_internal(env, err, (void **)&model->allocs, &model->alloc_capacity, model->alloc_count, sizeof(*model->allocs)))
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

                if(!child_has_live_fd && !child_closed_fd && p101_report_grow_array_internal(env, err, (void **)&model->fds, &model->fd_capacity, model->fd_count, sizeof(*model->fds)))
                {
                    p101_memset(env, &model->fds[model->fd_count], 0, sizeof(*model->fds));
                    model->fds[model->fd_count].pid      = event->child_pid;
                    model->fds[model->fd_count].fd       = model->fds[index].fd;
                    model->fds[model->fd_count].site     = model->fds[index].site;
                    model->fds[model->fd_count].sequence = event->sequence;
                    model->fd_count++;
                }
            }
            break;
        }
        case RESOURCE_SPAWN:
        {
            /*
             * The event is retained for the timeline, but opaque file actions
             * prevent a portable reconstruction of the child's descriptor
             * table. Do not invent fork-style inheritance.
             */
            break;
        }
        case RESOURCE_EXEC:
        {
            for(index = 0; index < model->fd_count; index++)
            {
                if(model->fds[index].pid == event->pid && model->fds[index].fd == event->fd)
                {
                    struct finding finding;

                    model->fds[index].exec_pending  = 1;
                    model->fds[index].exec_cloexec  = event->cloexec;
                    model->fds[index].exec_site     = event->site;
                    model->fds[index].exec_sequence = event->sequence;
                    if(!p101_tool_event_ownership_exec_inherits(P101_TOOL_EVENT_OWNERSHIP_LIVE, event->cloexec != 0))
                    {
                        break;
                    }

                    p101_memset(env, &finding, 0, sizeof(finding));
                    finding.kind              = FINDING_EXEC_INHERIT;
                    finding.pid               = event->pid;
                    finding.fd                = event->fd;
                    finding.ptr               = event->target;
                    finding.site              = event->site;
                    finding.sequence          = event->sequence;
                    finding.previous_site     = model->fds[index].site;
                    finding.previous_sequence = model->fds[index].sequence;
                    p101_report_add_finding_internal(env, err, model, &finding);
                    break;
                }
            }
            break;
        }
        case RESOURCE_EXEC_FAIL:
        {
            p101_report_rollback_failed_exec(env, model, event);
            break;
        }
        case RESOURCE_GENERIC:
        {
            p101_report_ingest_generic(env, err, model, event);
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
    index = 0U;
    while(index < model->fd_count && p101_error_has_no_error(err))
    {
        if(model->fds[index].exec_pending != 0)
        {
            p101_report_finish_exec(env, err, model, model->fds[index].pid, true);
            index = 0U;
        }
        else
        {
            index++;
        }
    }

    for(index = 0; index < model->fd_count && p101_error_has_no_error(err); index++)
    {
        struct finding finding;

        p101_memset(env, &finding, 0, sizeof(finding));
        finding.kind     = FINDING_FD_LEAK;
        finding.pid      = model->fds[index].pid;
        finding.fd       = model->fds[index].fd;
        finding.site     = model->fds[index].site;
        finding.sequence = model->fds[index].sequence;
        p101_report_add_finding_internal(env, err, model, &finding);
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
        p101_report_add_finding_internal(env, err, model, &finding);
    }

    p101_report_finalize_generic(env, err, model);
}

static void p101_report_finish_exec(const struct p101_env *env, struct p101_error *err, struct report_model *model, long pid, bool end_of_input)
{
    size_t index;

    index = 0U;
    while(index < model->fd_count && p101_error_has_no_error(err))
    {
        struct live_fd *fd;

        fd = &model->fds[index];
        if(fd->pid != pid || fd->exec_pending == 0)
        {
            index++;
            continue;
        }

        if(fd->exec_cloexec != 0)
        {
            if(p101_report_grow_array_internal(env, err, (void **)&model->closed_fds, &model->closed_fd_capacity, model->closed_fd_count, sizeof(*model->closed_fds)))
            {
                model->closed_fds[model->closed_fd_count].pid      = fd->pid;
                model->closed_fds[model->closed_fd_count].fd       = fd->fd;
                model->closed_fds[model->closed_fd_count].site     = fd->exec_site;
                model->closed_fds[model->closed_fd_count].sequence = fd->exec_sequence;
                model->closed_fd_count++;
            }
            model->fds[index] = model->fds[model->fd_count - 1U];
            model->fd_count--;
            continue;
        }

        if(end_of_input)
        {
            model->fds[index] = model->fds[model->fd_count - 1U];
            model->fd_count--;
            continue;
        }

        fd->exec_pending  = 0;
        fd->exec_cloexec  = 0;
        fd->exec_site     = 0U;
        fd->exec_sequence = 0U;
        index++;
    }
}

static void p101_report_rollback_failed_exec(const struct p101_env *env, struct report_model *model, const struct resource_event *event)
{
    size_t first_sequence;
    size_t read_index;
    size_t write_index;

    first_sequence = event->sequence;
    if(model->resource_count == 0U)
    {
        return;
    }

    for(read_index = 0U; read_index < model->fd_count; read_index++)
    {
        if(model->fds[read_index].pid == event->pid && model->fds[read_index].exec_pending != 0)
        {
            model->fds[read_index].exec_pending  = 0;
            model->fds[read_index].exec_cloexec  = 0;
            model->fds[read_index].exec_site     = 0U;
            model->fds[read_index].exec_sequence = 0U;
        }
    }

    /*
     * The exec wrapper writes one snapshot record per open descriptor followed
     * immediately by EXECFAIL when the native exec call returns. Walk only
     * that contiguous attempt, so an earlier successful exec of the same
     * target remains a finding.
     */
    read_index = model->resource_count - 1U;
    while(read_index > 0U)
    {
        const struct resource_event *candidate;

        candidate = &model->resources[read_index - 1U];
        if(candidate->kind != RESOURCE_EXEC || candidate->pid != event->pid || candidate->site != event->site || candidate->target == NULL || event->target == NULL || p101_strcmp(env, candidate->target, event->target) != 0)
        {
            break;
        }
        first_sequence = candidate->sequence;
        read_index--;
    }

    write_index = 0;
    for(read_index = 0; read_index < model->finding_count; read_index++)
    {
        struct finding *finding;

        finding = &model->findings[read_index];
        if(finding->kind == FINDING_EXEC_INHERIT && finding->pid == event->pid && finding->sequence >= first_sequence && finding->ptr != NULL && event->target != NULL && p101_strcmp(env, finding->ptr, event->target) == 0)
        {
            p101_free(env, finding->ptr);
            continue;
        }

        if(write_index != read_index)
        {
            model->findings[write_index] = *finding;
        }
        write_index++;
    }
    model->finding_count = write_index;
}

static void p101_report_add_resource_event(const struct p101_env *env, struct p101_error *err, struct report_model *model, const struct resource_event *event)
{
    struct resource_event *copy;

    if(!p101_report_grow_array_internal(env, err, (void **)&model->resources, &model->resource_capacity, model->resource_count, sizeof(*model->resources)))
    {
        goto done;
    }

    copy                 = &model->resources[model->resource_count];
    *copy                = *event;
    copy->ptr            = NULL;
    copy->new_ptr        = NULL;
    copy->target         = NULL;
    copy->resource_class = NULL;
    copy->resource_id    = NULL;
    copy->related_id     = NULL;
    copy->metadata       = NULL;
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
    if(event->resource_class != NULL)
    {
        copy->resource_class = p101_report_dup_text(env, err, event->resource_class);
    }
    if(event->resource_id != NULL)
    {
        copy->resource_id = p101_report_dup_text(env, err, event->resource_id);
    }
    if(event->related_id != NULL)
    {
        copy->related_id = p101_report_dup_text(env, err, event->related_id);
    }
    if(event->metadata != NULL)
    {
        copy->metadata = p101_report_dup_text(env, err, event->metadata);
    }
    if(p101_error_has_error(err))
    {
        p101_report_free_resource_event(env, copy);
        p101_memset(env, copy, 0, sizeof(*copy));
    }
    else
    {
        model->resource_count++;
    }

done:
    return;
}
