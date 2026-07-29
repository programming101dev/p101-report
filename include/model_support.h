#ifndef P101_REPORT_MODEL_SUPPORT_H
#define P101_REPORT_MODEL_SUPPORT_H

#include "types.h"
#include <p101_env/env.h>
#include <p101_error/error.h>
#include <stdbool.h>
#include <stddef.h>

size_t p101_report_intern_site(const struct p101_env *env, struct p101_error *err, struct report_model *model, const char *file_name, const char *function_name, int line_number);
void p101_report_add_call(const struct p101_env *env, struct p101_error *err, struct report_model *model, const struct call_event *event);
void p101_report_add_finding_internal(const struct p101_env *env, struct p101_error *err, struct report_model *model, const struct finding *finding);
bool p101_report_grow_array_internal(const struct p101_env *env, struct p101_error *err, void **items, size_t *capacity, size_t count, size_t item_size);

#endif    // P101_REPORT_MODEL_SUPPORT_H
