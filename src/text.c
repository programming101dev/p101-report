#include "text.h"
#include "constants.h"
#include <limits.h>
#include <p101_c/p101_stdlib.h>
#include <p101_c/p101_string.h>
#include <stdlib.h>

char *p101_report_split_tab(char **cursor)
{
    char *start;
    char *tab;

    start = *cursor;
    if(start == NULL)
    {
        goto done;
    }

    tab = start;
    while(*tab != '\0' && *tab != '\t')
    {
        tab++;
    }

    if(*tab == '\0')
    {
        *cursor = NULL;
    }
    else
    {
        *tab    = '\0';
        *cursor = tab + 1;
    }

done:
    return start;
}

bool p101_report_strip_line(const struct p101_env *env, char *line)
{
    size_t length;

    if(line == NULL)
    {
        return false;
    }

    length = p101_strlen(env, line);
    while(length > 0U && (line[length - 1U] == '\n' || line[length - 1U] == '\r'))
    {
        length--;
        line[length] = '\0';
    }

    return true;
}

bool p101_report_parse_long_field(const char *text, long min, long max, long *out)
{
    bool        ok;
    bool        negative;
    long        value;
    const char *cursor;

    ok     = false;
    value  = 0;
    cursor = text;

    if(cursor == NULL || *cursor == '\0')
    {
        goto done;
    }

    negative = (*cursor == '-');
    if(negative)
    {
        cursor++;
    }

    if(*cursor == '\0')
    {
        goto done;
    }

    while(*cursor != '\0')
    {
        int digit;

        if(*cursor < '0' || *cursor > '9')
        {
            goto done;
        }

        digit = *cursor - '0';
        if(value > (LONG_MAX - (long)digit) / DECIMAL_BASE)
        {
            goto done;
        }

        value = (value * DECIMAL_BASE) + digit;
        cursor++;
    }

    if(negative)
    {
        value = -value;
    }

    if(value < min || value > max)
    {
        goto done;
    }

    *out = value;
    ok   = true;

done:
    return ok;
}

bool p101_report_parse_size_field(const char *text, size_t *out)
{
    bool        ok;
    size_t      value;
    const char *cursor;

    ok     = false;
    value  = 0;
    cursor = text;

    if(cursor == NULL || *cursor == '\0')
    {
        goto done;
    }

    while(*cursor != '\0')
    {
        size_t digit;

        if(*cursor < '0' || *cursor > '9')
        {
            goto done;
        }

        digit = (size_t)(*cursor - '0');
        if(value > (SIZE_MAX - digit) / (size_t)DECIMAL_BASE)
        {
            goto done;
        }

        value = (value * (size_t)DECIMAL_BASE) + digit;
        cursor++;
    }

    *out = value;
    ok   = true;

done:
    return ok;
}

bool p101_report_line_has_prefix(const struct p101_env *env, const char *line, const char *prefix)
{
    return p101_strncmp(env, line, prefix, p101_strlen(env, prefix)) == 0;
}
