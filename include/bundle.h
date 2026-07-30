#ifndef P101_REPORT_BUNDLE_H
#define P101_REPORT_BUNDLE_H

#include "arguments.h"
#include "types.h"
#include <p101_env/env.h>
#include <p101_error/error.h>

void p101_report_write_bundle(const struct p101_env *env, struct p101_error *err, const struct arguments *args, const struct report_model *model);

#endif    // P101_REPORT_BUNDLE_H
