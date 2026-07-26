#ifndef P101_REPORT_RUNNER_H
#define P101_REPORT_RUNNER_H

#include "arguments.h"
#include <p101_env/env.h>
#include <p101_error/error.h>

int p101_report_run(const struct p101_env *env, struct p101_error *err, const struct arguments *args);

#endif    // P101_REPORT_RUNNER_H
