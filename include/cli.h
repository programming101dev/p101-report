#ifndef P101_REPORT_CLI_H
#define P101_REPORT_CLI_H

#include "arguments.h"
#include <p101_env/env.h>
#include <p101_error/error.h>
#include <stddef.h>

void           p101_report_arguments_init(const struct p101_env *env, struct arguments *args);
void           p101_report_parse_arguments(const struct p101_env *env, struct p101_error *err, int argc, char *argv[], struct arguments *args);
void           p101_report_check_arguments(const struct p101_env *env, struct p101_error *err, struct arguments *args, char *resource_buf, size_t resource_buf_size, char *call_buf, size_t call_buf_size);
_Noreturn void p101_report_usage(const struct p101_env *env, struct p101_error *err, const char *program_name, int exit_code, const char *message);

#endif    // P101_REPORT_CLI_H
