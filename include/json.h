#ifndef P101_REPORT_JSON_H
#define P101_REPORT_JSON_H

#include <p101_env/env.h>
#include <p101_error/error.h>
#include <stdio.h>

void p101_report_json_string(const struct p101_env *env, struct p101_error *err, FILE *stream, const char *text);

#endif    // P101_REPORT_JSON_H
