#ifndef P101_REPORT_MODEL_H
#define P101_REPORT_MODEL_H

#include "types.h"
#include <p101_env/env.h>
#include <p101_error/error.h>
#include <stdbool.h>
#include <stddef.h>

void   p101_report_ingest_resource(const struct p101_env *env, struct p101_error *err, struct report_model *model, const struct resource_event *event);
void   p101_report_finalize_leaks(const struct p101_env *env, struct p101_error *err, struct report_model *model);
size_t p101_report_intern_site(const struct p101_env *env, struct p101_error *err, struct report_model *model, const char *file_name, const char *function_name, int line_number);
void   p101_report_add_call(const struct p101_env *env, struct p101_error *err, struct report_model *model, const struct call_event *event);
char  *p101_report_dup_text(const struct p101_env *env, struct p101_error *err, const char *text);
void   p101_report_free_model(const struct p101_env *env, struct report_model *model);
void   p101_report_free_resource_event(const struct p101_env *env, struct resource_event *event);
void   p101_report_free_call_event(const struct p101_env *env, struct call_event *event);

#endif    // P101_REPORT_MODEL_H
