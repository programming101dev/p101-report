#ifndef P101_REPORT_TEXT_H
#define P101_REPORT_TEXT_H

#include <p101_env/env.h>
#include <stdbool.h>
#include <stddef.h>

char *p101_report_split_tab(char **cursor);
bool  p101_report_strip_line(const struct p101_env *env, char *line);
bool  p101_report_parse_long_field(const char *text, long min, long max, long *out);
bool  p101_report_parse_size_field(const char *text, size_t *out);
bool  p101_report_line_has_prefix(const struct p101_env *env, const char *line, const char *prefix);

#endif    // P101_REPORT_TEXT_H
