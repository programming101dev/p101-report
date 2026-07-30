#ifndef P101_REPORT_PARSE_H
#define P101_REPORT_PARSE_H

#include "types.h"
#include <p101_env/env.h>
#include <p101_error/error.h>
#include <stddef.h>

enum line_status p101_report_parse_resource_line(const struct p101_env *env, struct p101_error *err, char *line, struct resource_event *event, struct report_model *model, size_t sequence);
enum line_status p101_report_parse_call_line(const struct p101_env *env, struct p101_error *err, char *line, struct call_event *event, struct report_model *model, size_t sequence);

#endif    // P101_REPORT_PARSE_H
