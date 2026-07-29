#ifndef P101_REPORT_MODEL_GENERIC_H
#define P101_REPORT_MODEL_GENERIC_H

#include "types.h"
#include <p101_env/env.h>
#include <p101_error/error.h>

void p101_report_ingest_generic(const struct p101_env *env, struct p101_error *err, struct report_model *model, const struct resource_event *event);
void p101_report_finalize_generic(const struct p101_env *env, struct p101_error *err, struct report_model *model);

#endif    // P101_REPORT_MODEL_GENERIC_H
