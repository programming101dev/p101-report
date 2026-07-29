#ifndef P101_REPORT_LIFETIME_MERMAID_H
#define P101_REPORT_LIFETIME_MERMAID_H

#include "types.h"
#include <p101_env/env.h>
#include <p101_error/error.h>

void p101_report_print_mermaid_lifetimes(const struct p101_env *env, struct p101_error *err, const struct report_model *model);

#endif    // P101_REPORT_LIFETIME_MERMAID_H
