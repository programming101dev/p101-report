#include "runner.h"
#include "bundle.h"
#include "constants.h"
#include "memory.h"
#include "model.h"
#include "output.h"
#include "reader.h"
#include <p101_c/p101_string.h>
#include <p101_tool_event/model.h>
#include <stdlib.h>

int p101_report_run(const struct p101_env *env, struct p101_error *err, const struct arguments *args)
{
    struct report_model model;
    int                 ret_val;

    P101_TRACE_SCOPE(env);
    p101_memset(env, &model, 0, sizeof(model));
    ret_val         = EXIT_TROUBLE;
    model.run_model = p101_tool_model_create(err);
    if(model.run_model == NULL)
    {
        goto done;
    }

    p101_report_read_resources(env, err, args->resource_log, &model);
    p101_report_read_calls(env, err, args->call_log, &model);

    if(p101_error_has_error(err))
    {
        goto done;
    }
    if(p101_tool_model_finish(err, model.run_model) != 0)
    {
        goto done;
    }

    p101_report_finalize_leaks(env, err, &model);

    if(p101_error_has_error(err))
    {
        goto done;
    }

    if(args->bundle_output_dir == NULL)
    {
        p101_report_print_report(env, err, args, &model);
    }
    else
    {
        p101_report_write_bundle(env, err, args, &model);
    }

    if(p101_error_has_error(err))
    {
        goto done;
    }

    if(model.resource_malformed != 0U || model.call_malformed != 0U || model.resource_bad_version != 0U || model.call_bad_version != 0U || !p101_tool_event_stream_health_is_complete(&model.resource_stream_health) ||
       !p101_tool_event_stream_health_is_complete(&model.call_stream_health))
    {
        ret_val = EXIT_TROUBLE;
    }
    else
    {
        ret_val = model.finding_count == 0U ? EXIT_CLEAN : EXIT_FINDINGS;
    }

done:
    p101_report_free_model(env, &model);
    return ret_val;
}
