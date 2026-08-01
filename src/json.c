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
    while(*cursor != '\0')
    {
        unsigned char current;

        current = *cursor;
        if(current == '\"' || current == '\\')
        {
            p101_fputc(env, err, '\\', stream);
            p101_fputc(env, err, (int)current, stream);
        }
        else if(current == '\n')
        {
            p101_fputs(env, err, "\\n", stream);
        }
        else if(current == '\r')
        {
            p101_fputs(env, err, "\\r", stream);
        }
        else if(current == '\t')
        {
            p101_fputs(env, err, "\\t", stream);
        }
        else if(current < JSON_CONTROL_LIMIT || current >= JSON_NON_ASCII_LIMIT)
        {
            p101_fprintf(env, err, stream, "\\u%04x", (unsigned)current);
        }
        else
        {
            p101_fputc(env, err, (int)current, stream);
        }

        if(p101_error_has_error(err))
        {
            break;
        }
        cursor++;
    }

done:
    p101_fputc(env, err, '\"', stream);
}
