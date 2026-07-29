/*
 * libFuzzer harness for p101-report's argument parser and line parsers.
 */
#include "cli.h"
#include "constants.h"
#include "memory.h"
#include "model.h"
#include "parse.h"
#include <p101_c/p101_stdlib.h>
#include <p101_c/p101_string.h>
#include <p101_tool_event/event.h>
#include <setjmp.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static jmp_buf g_fuzz_exit_jmp;

_Noreturn void p101_fuzz_exit(const struct p101_env *env, int code)
{
    (void)env;
    (void)code;
    longjmp(g_fuzz_exit_jmp, 1);
}

#define FUZZ_MAX_ARGS 64

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    char               *buf;
    char               *argv[FUZZ_MAX_ARGS];
    int                 argc;
    char               *p;
    struct p101_error  *err;
    struct p101_env    *env;
    struct arguments    args;
    struct report_model model;
    char                resources[PATH_MAX_BYTES];
    char                calls[PATH_MAX_BYTES];

    err = p101_error_create(false);
    env = p101_env_create(err, NULL);
    p101_memset(env, &model, 0, sizeof(model));

    buf = (char *)p101_malloc(env, err, size + 1U);
    if(buf == NULL)
    {
        goto done;
    }
    p101_memcpy(env, buf, data, size);
    buf[size] = '\0';

    argv[0] = (char *)"prog";
    argc    = 1;
    p       = buf;
    while(argc < FUZZ_MAX_ARGS - 1)
    {
        while(*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r' || *p == '\v' || *p == '\f')
        {
            p++;
        }
        if(*p == '\0')
        {
            break;
        }
        argv[argc++] = p;
        while(*p != '\0' && *p != ' ' && *p != '\t' && *p != '\n' && *p != '\r' && *p != '\v' && *p != '\f')
        {
            p++;
        }
        if(*p != '\0')
        {
            *p++ = '\0';
        }
    }
    argv[argc] = NULL;

#ifdef __GLIBC__
    optind = 0;
#else
    {
        extern int optreset;
        optreset = 1;
        optind   = 1;
    }
#endif

    p101_memset(env, &args, 0, sizeof(args));

    if(setjmp(g_fuzz_exit_jmp) == 0)
    {
        p101_report_parse_arguments(env, err, argc, argv, &args);

        if(p101_error_has_no_error(err))
        {
            p101_report_check_arguments(env, err, &args, resources, sizeof(resources), calls, sizeof(calls));
        }

        if(size < P101_TOOL_EVENT_LINE_MAX_BYTES)
        {
            struct resource_event resource_event;
            struct call_event     call_event;

            p101_memset(env, &resource_event, 0, sizeof(resource_event));
            p101_memset(env, &call_event, 0, sizeof(call_event));
            p101_memcpy(env, buf, data, size);
            buf[size] = '\0';
            (void)p101_report_parse_resource_line(env, err, buf, &resource_event, &model, 1U);
            p101_report_free_resource_event(env, &resource_event);

            p101_memcpy(env, buf, data, size);
            buf[size] = '\0';
            (void)p101_report_parse_call_line(env, err, buf, &call_event, 1U);
            p101_report_free_call_event(env, &call_event);
        }
    }

done:
    p101_report_free_model(env, &model);
    p101_free(env, buf);
    p101_env_destroy(env);
    p101_error_destroy(err);
    return 0;
}
