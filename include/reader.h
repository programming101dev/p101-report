#ifndef P101_REPORT_READER_H
#define P101_REPORT_READER_H

#include "types.h"
#include <p101_env/env.h>
#include <p101_error/error.h>

void p101_report_read_resources(const struct p101_env *env, struct p101_error *err, const char *path, struct report_model *model);
void p101_report_read_calls(const struct p101_env *env, struct p101_error *err, const char *path, struct report_model *model);

#endif    // P101_REPORT_READER_H
