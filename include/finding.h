#ifndef P101_REPORT_FINDING_H
#define P101_REPORT_FINDING_H

#include "types.h"
#include <p101_env/env.h>
#include <stdbool.h>

bool        p101_report_site_matches_call(const struct p101_env *env, const struct source_site *site, const struct call_event *call, long pid, size_t context_id);
const char *p101_report_finding_id(enum finding_kind kind);
const char *p101_report_finding_name(enum finding_kind kind);

#endif    // P101_REPORT_FINDING_H
