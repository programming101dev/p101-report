#ifndef P101_REPORT_OUTPUT_H
#define P101_REPORT_OUTPUT_H

#include "arguments.h"
#include "types.h"
#include <p101_env/env.h>
#include <p101_error/error.h>

void p101_report_print_report(const struct p101_env *env, struct p101_error *err, const struct arguments *args, const struct report_model *model);
void p101_report_print_text_report(const struct p101_env *env, struct p101_error *err, const struct arguments *args, const struct report_model *model);
void p101_report_print_json_report(const struct p101_env *env, struct p101_error *err, const struct arguments *args, const struct report_model *model);
void p101_report_print_mermaid_report(const struct p101_env *env, struct p101_error *err, const struct arguments *args, const struct report_model *model);
void p101_report_print_finding(const struct p101_env *env, struct p101_error *err, const struct report_model *model, const struct finding *finding, size_t ordinal);
void p101_report_print_trace_context(const struct p101_env *env, struct p101_error *err, const struct report_model *model, const struct finding *finding);
void p101_report_print_json_finding(const struct p101_env *env, struct p101_error *err, const struct report_model *model, const struct finding *finding);
void p101_report_print_json_trace_context(const struct p101_env *env, struct p101_error *err, const struct report_model *model, const struct finding *finding);

#endif    // P101_REPORT_OUTPUT_H
