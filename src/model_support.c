#include "model_support.h"
#include "constants.h"
#include "memory.h"
#include <errno.h>
#include <p101_c/p101_stdlib.h>
#include <p101_c/p101_string.h>
#include <stdint.h>

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

    if(p101_report_grow_array_internal(env, err, (void **)&model->sites, &model->site_capacity, model->site_count, sizeof(*model->sites)))
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

void p101_report_add_finding_internal(const struct p101_env *env, struct p101_error *err, struct report_model *model, const struct finding *finding)
{
    if(p101_report_grow_array_internal(env, err, (void **)&model->findings, &model->finding_capacity, model->finding_count, sizeof(*model->findings)))
    {
        struct finding *copy;

        copy                 = &model->findings[model->finding_count];
        *copy                = *finding;
        copy->ptr            = NULL;
        copy->resource_class = NULL;
        copy->resource_id    = NULL;
        if(finding->ptr != NULL)
        {
            copy->ptr = p101_report_dup_text(env, err, finding->ptr);
        }
        if(finding->resource_class != NULL)
        {
            copy->resource_class = p101_report_dup_text(env, err, finding->resource_class);
        }
        if(finding->resource_id != NULL)
        {
            copy->resource_id = p101_report_dup_text(env, err, finding->resource_id);
        }
        if(p101_error_has_error(err))
        {
            p101_free(env, copy->ptr);
            p101_free(env, copy->resource_class);
            p101_free(env, copy->resource_id);
            p101_memset(env, copy, 0, sizeof(*copy));
        }
        else
        {
            model->finding_count++;
        }
    }
}

void p101_report_add_call(const struct p101_env *env, struct p101_error *err, struct report_model *model, const struct call_event *event)
{
    if(p101_report_grow_array_internal(env, err, (void **)&model->calls, &model->call_capacity, model->call_count, sizeof(*model->calls)))
    {
        model->calls[model->call_count] = *event;
        model->call_count++;
    }
}

bool p101_report_grow_array_internal(const struct p101_env *env, struct p101_error *err, void **items, size_t *capacity, size_t count, size_t item_size)
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
