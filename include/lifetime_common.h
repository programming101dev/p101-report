#ifndef P101_REPORT_LIFETIME_COMMON_H
#define P101_REPORT_LIFETIME_COMMON_H

#include "types.h"
#include <p101_env/env.h>
#include <stdbool.h>
#include <stddef.h>

const char *p101_report_resource_kind_name(enum resource_kind kind);
const struct resource_event *p101_report_find_fd_lifetime_end(const struct p101_env *env, const struct report_model *model, const struct resource_event *birth);
const struct resource_event *p101_report_find_alloc_lifetime_end(const struct p101_env *env, const struct report_model *model, const struct resource_event *birth);
const struct resource_event *p101_report_find_generic_lifetime_end(const struct p101_env *env, const struct report_model *model, const struct resource_event *birth);
bool p101_report_lifetime_duration_ns(const struct resource_event *birth, const struct resource_event *death, size_t *duration_ns);

#endif    // P101_REPORT_LIFETIME_COMMON_H
