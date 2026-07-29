#ifndef P101_REPORT_MEMORY_H
#define P101_REPORT_MEMORY_H

#include "types.h"
#include <p101_env/env.h>
#include <p101_error/error.h>

char *p101_report_dup_text(const struct p101_env *env, struct p101_error *err, const char *text);
void  p101_report_free_model(const struct p101_env *env, struct report_model *model);
void  p101_report_free_resource_event(const struct p101_env *env, struct resource_event *event);
void  p101_report_free_call_event(const struct p101_env *env, struct call_event *event);

#endif    // P101_REPORT_MEMORY_H
