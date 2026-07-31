#include "bundle.h"
#include "constants.h"
#include "io.h"
#include "output.h"
#include <fcntl.h>
#include <p101_c/p101_stdio.h>
#include <p101_posix/p101_fcntl.h>
#include <p101_posix/p101_unistd.h>
#include <p101_tool_event/model.h>
#include <stdio.h>
#include <unistd.h>

static void write_one(const struct p101_env *env, struct p101_error *err, const struct arguments *args, const struct report_model *model, const char *name, enum report_format format);
static void write_run_model(const struct p101_env *env, struct p101_error *err, const struct arguments *args, const struct report_model *model);

void p101_report_write_bundle(const struct p101_env *env, struct p101_error *err, const struct arguments *args, const struct report_model *model)
{
    P101_TRACE_SCOPE(env);
    write_one(env, err, args, model, "correlated-report.txt", REPORT_FORMAT_TEXT);
    write_one(env, err, args, model, "correlated-report.json", REPORT_FORMAT_JSON);
    write_one(env, err, args, model, "resource-lifetimes.md", REPORT_FORMAT_MERMAID);
    write_run_model(env, err, args, model);
}

static void write_run_model(const struct p101_env *env, struct p101_error *err, const struct arguments *args, const struct report_model *model)
{
    char  path[PATH_MAX_BYTES];
    FILE *stream;

    if(p101_error_has_error(err))
    {
        return;
    }
    p101_report_join_path(env, err, path, sizeof(path), args->bundle_output_dir, "run-model.json");
    stream = p101_fopen(env, err, path, "w");
    if(stream != NULL)
    {
        (void)p101_tool_model_write_json(err, stream, model->run_model);
        p101_fclose(env, err, stream);
    }
}

static void write_one(const struct p101_env *env, struct p101_error *err, const struct arguments *args, const struct report_model *model, const char *name, enum report_format format)
{
    struct arguments output_args;
    char             path[PATH_MAX_BYTES];
    int              output_fd;
    int              saved_stdout;

    if(p101_error_has_error(err))
    {
        return;
    }

    output_fd    = -1;
    saved_stdout = -1;
    p101_report_join_path(env, err, path, sizeof(path), args->bundle_output_dir, name);
    if(p101_error_has_error(err))
    {
        goto done;
    }

    p101_fflush(env, err, stdout);
    output_fd = p101_open(env, err, path, O_WRONLY | O_CREAT | O_TRUNC, REPORT_FILE_MODE);
    if(p101_error_has_error(err))
    {
        goto done;
    }
    saved_stdout = p101_dup(env, err, STDOUT_FILENO);
    if(p101_error_has_error(err))
    {
        goto done;
    }
    p101_dup2(env, err, output_fd, STDOUT_FILENO);
    p101_close(env, err, output_fd);
    output_fd = -1;
    if(p101_error_has_error(err))
    {
        goto done;
    }

    output_args        = *args;
    output_args.format = format;
    p101_report_print_report(env, err, &output_args, model);
    p101_fflush(env, err, stdout);

done:
    if(saved_stdout >= 0)
    {
        p101_dup2(env, err, saved_stdout, STDOUT_FILENO);
        p101_close(env, err, saved_stdout);
    }
    if(output_fd >= 0)
    {
        p101_close(env, err, output_fd);
    }
}
