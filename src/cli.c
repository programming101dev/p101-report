#include "cli.h"
#include "constants.h"
#include "errors.h"
#include "io.h"
#include <p101_c/p101_ctype.h>
#include <p101_c/p101_stdio.h>
#include <p101_c/p101_stdlib.h>
#include <p101_c/p101_string.h>
#include <p101_posix/p101_unistd.h>
#include <stdlib.h>

void p101_report_arguments_init(const struct p101_env *env, struct arguments *args)
{
    P101_TRACE_SCOPE(env);
    p101_memset(env, args, 0, sizeof(*args));
    args->format = REPORT_FORMAT_TEXT;
}

void p101_report_parse_arguments(const struct p101_env *env, struct p101_error *err, int argc, char *argv[], struct arguments *args)
{
    int opt;

    P101_TRACE_SCOPE(env);
    opterr = 0;

    if(argc == 2 && p101_strcmp(env, argv[1], "--help") == 0)
    {
        p101_report_usage(env, err, argv[0], EXIT_SUCCESS, NULL);
    }

    while((opt = p101_getopt(env, argc, argv, ":hvjmd:r:c:")) != -1 && p101_error_has_no_error(err))
    {
        switch(opt)
        {
            case 'h':
            {
                p101_report_usage(env, err, argv[0], EXIT_CLEAN, NULL);
            }
            case 'v':
            {
                args->verbose = true;
                break;
            }
            case 'j':
            {
                args->format = REPORT_FORMAT_JSON;
                break;
            }
            case 'm':
            {
                args->format = REPORT_FORMAT_MERMAID;
                break;
            }
            case 'd':
            {
                args->report_dir = optarg;
                break;
            }
            case 'r':
            {
                args->resource_log = optarg;
                break;
            }
            case 'c':
            {
                args->call_log = optarg;
                break;
            }
            case ':':
            {
                char msg[MSG_LEN];

                p101_snprintf(env, err, msg, sizeof(msg), "Option '-%c' requires an argument.", optopt ? optopt : '?');
                P101_ERROR_RAISE_USER(err, msg, ERR_USAGE);
                break;
            }
            case '?':
            {
                char msg[MSG_LEN];

                if(p101_isprint(env, optopt))
                {
                    p101_snprintf(env, err, msg, sizeof(msg), "Unknown option '-%c'.", optopt);
                }
                else
                {
                    p101_snprintf(env, err, msg, sizeof(msg), "Unknown option character 0x%02X.", (unsigned)(unsigned char)optopt);
                }

                P101_ERROR_RAISE_USER(err, msg, ERR_USAGE);
                break;
            }
            default:
            {
                char msg[MSG_LEN];

                p101_snprintf(env, err, msg, sizeof(msg), "Internal error: unhandled option '-%c'.", p101_isprint(env, opt) ? opt : '?');
                P101_ERROR_RAISE_USER(err, msg, ERR_USAGE);
                break;
            }
        }
    }

    if(p101_error_has_error(err))
    {
        goto done;
    }

    if(optind < argc)
    {
        if(args->report_dir != NULL)
        {
            P101_ERROR_RAISE_USER(err, "Report directory was supplied twice.", ERR_USAGE);
            goto done;
        }

        args->report_dir = argv[optind];
        optind++;
    }

    if(optind < argc)
    {
        P101_ERROR_RAISE_USER(err, "Too many positional arguments.", ERR_USAGE);
    }

done:
    return;
}

void p101_report_check_arguments(const struct p101_env *env, struct p101_error *err, struct arguments *args, char *resource_buf, size_t resource_buf_size, char *call_buf, size_t call_buf_size)
{
    P101_TRACE_SCOPE(env);

    if(args->report_dir == NULL && (args->resource_log == NULL || args->call_log == NULL))
    {
        P101_ERROR_RAISE_USER(err, "Provide a report directory, or both -r resources.log and -c calls.log.", ERR_USAGE);
        goto done;
    }

    if(args->report_dir != NULL && args->report_dir[0] == '\0')
    {
        P101_ERROR_RAISE_USER(err, "The report directory must not be empty.", ERR_USAGE);
        goto done;
    }

    if(args->resource_log == NULL)
    {
        p101_report_join_path(env, err, resource_buf, resource_buf_size, args->report_dir, "resources.log");
        args->resource_log = resource_buf;
    }

    if(args->call_log == NULL)
    {
        p101_report_join_path(env, err, call_buf, call_buf_size, args->report_dir, "calls.log");
        args->call_log = call_buf;
    }

    if(args->resource_log == NULL || args->resource_log[0] == '\0')
    {
        P101_ERROR_RAISE_USER(err, "The resource log path must not be empty.", ERR_USAGE);
        goto done;
    }

    if(args->call_log == NULL || args->call_log[0] == '\0')
    {
        P101_ERROR_RAISE_USER(err, "The call log path must not be empty.", ERR_USAGE);
        goto done;
    }

done:
    return;
}

_Noreturn void p101_report_usage(const struct p101_env *env, struct p101_error *err, const char *program_name, int exit_code, const char *message)
{
#ifndef P101_SUPPRESS_USAGE_TEXT
    FILE *stream;

    stream = (exit_code == EXIT_CLEAN) ? stdout : stderr;

    if(message != NULL)
    {
        p101_fprintf(env, err, stream, "%s\n\n", message);
    }

    p101_fprintf(env, err, stream, "Usage: %s [-h] [-v] [-j|-m] [-d <report-dir>] [-r <resources.log> -c <calls.log>] [report-dir]\n", program_name);
    p101_fputs(env, err, "\n", stream);
    p101_fputs(env, err, "Correlate p101 resource and call logs into one report.\n", stream);
    p101_fputs(env, err, "\n", stream);
    p101_fputs(env, err, "Options:\n", stream);
    p101_fputs(env, err, "  -h                  show this help\n", stream);
    p101_fputs(env, err, "  -v                  trace p101-report itself\n", stream);
    p101_fputs(env, err, "  -j                  write JSON instead of the text report\n", stream);
    p101_fputs(env, err, "  -m                  write Mermaid resource lifetime graph instead of the text report\n", stream);
    p101_fputs(env, err, "  -d <report-dir>     read resources.log and calls.log from a p101-observe report directory\n", stream);
    p101_fputs(env, err, "  -r <resources.log>  resource log to replay\n", stream);
    p101_fputs(env, err, "  -c <calls.log>      call log to correlate\n", stream);
#else
    (void)err;
    (void)message;
    (void)program_name;
#endif

    p101_exit(env, exit_code);
}
