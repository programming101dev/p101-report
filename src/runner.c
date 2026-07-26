#include "runner.h"
#include "constants.h"
#include "model.h"
#include "output.h"
#include "reader.h"
#include <p101_c/p101_string.h>
#include <stdlib.h>

int p101_report_run(const struct p101_env *env, struct p101_error *err, const struct arguments *args)
{
    struct report_model model;
    int                 ret_val;

    P101_TRACE(env);
    p101_memset(env, &model, 0, sizeof(model));
    ret_val = EXIT_TROUBLE;

    p101_report_read_resources(env, err, args->resource_log, &model);
    p101_report_read_calls(env, err, args->call_log, &model);

    if(p101_error_has_error(err))
    {
        goto done;
    }

    p101_report_finalize_leaks(env, err, &model);

    if(p101_error_has_error(err))
    {
        goto done;
    }

    p101_report_print_report(env, err, args, &model);

    if(p101_error_has_error(err))
    {
        goto done;
    }

    ret_val = model.finding_count == 0 ? EXIT_CLEAN : EXIT_FINDINGS;

done:
    p101_report_free_model(env, &model);
    return ret_val;
}
