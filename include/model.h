#ifndef P101_REPORT_MODEL_H
#define P101_REPORT_MODEL_H

#include "types.h"
#include <p101_env/env.h>
#include <p101_error/error.h>
#include <stdbool.h>
#include <stddef.h>

void p101_report_ingest_resource(const struct p101_env *env, struct p101_error *err, struct report_model *model, const struct resource_event *event);
void p101_report_finalize_leaks(const struct p101_env *env, struct p101_error *err, struct report_model *model);

#endif    // P101_REPORT_MODEL_H
