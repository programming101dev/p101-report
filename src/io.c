#include "io.h"
#include "constants.h"
#include <p101_c/p101_stdio.h>
#include <p101_c/p101_string.h>
#include <stdio.h>

int p101_report_close_if_owned(const struct p101_env *env, struct p101_error *err, FILE *stream, bool owned)
{
    int ret_val;

    ret_val = 0;
    if(owned && stream != NULL)
    {
        ret_val = p101_fclose(env, err, stream);
    }

    return ret_val;
}

FILE *p101_report_open_input(const struct p101_env *env, struct p101_error *err, const char *path, bool *owned)
{
    FILE *stream;

    stream = NULL;
    *owned = false;

    if(p101_strcmp(env, path, "-") == 0)
    {
        stream = stdin;
    }
    else
    {
        stream = p101_fopen(env, err, path, "r");
        *owned = stream != NULL;
    }

    return stream;
}

void p101_report_join_path(const struct p101_env *env, struct p101_error *err, char *out, size_t out_size, const char *dir, const char *leaf)
{
    size_t dir_len;

    dir_len = p101_strlen(env, dir);
    if(dir_len > 0U && dir[dir_len - 1U] == '/')
    {
        p101_snprintf(env, err, out, out_size, "%s%s", dir, leaf);
    }
    else
    {
        p101_snprintf(env, err, out, out_size, "%s/%s", dir, leaf);
    }
}
