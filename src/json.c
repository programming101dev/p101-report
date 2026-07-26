#include "json.h"
#include "constants.h"
#include <p101_c/p101_stdio.h>

void p101_report_json_string(const struct p101_env *env, struct p101_error *err, FILE *stream, const char *text)
{
    const unsigned char *cursor;

    p101_fputc(env, err, '\"', stream);

    if(text == NULL)
    {
        goto done;
    }

    cursor = (const unsigned char *)text;
    while(*cursor != '\0' && p101_error_has_no_error(err))
    {
        if(*cursor == '\"' || *cursor == '\\')
        {
            p101_fputc(env, err, '\\', stream);
            p101_fputc(env, err, (int)*cursor, stream);
        }
        else if(*cursor == '\n')
        {
            p101_fputs(env, err, "\\n", stream);
        }
        else if(*cursor == '\r')
        {
            p101_fputs(env, err, "\\r", stream);
        }
        else if(*cursor == '\t')
        {
            p101_fputs(env, err, "\\t", stream);
        }
        else if(*cursor < JSON_CONTROL_LIMIT)
        {
            p101_fprintf(env, err, stream, "\\u%04x", (unsigned)*cursor);
        }
        else
        {
            p101_fputc(env, err, (int)*cursor, stream);
        }

        cursor++;
    }

done:
    p101_fputc(env, err, '\"', stream);
}
