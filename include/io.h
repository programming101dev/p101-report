#ifndef P101_REPORT_IO_H
#define P101_REPORT_IO_H

#include <p101_env/env.h>
#include <p101_error/error.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>

int   p101_report_close_if_owned(const struct p101_env *env, struct p101_error *err, FILE *stream, bool owned);
FILE *p101_report_open_input(const struct p101_env *env, struct p101_error *err, const char *path, bool *owned);
void  p101_report_join_path(const struct p101_env *env, struct p101_error *err, char *out, size_t out_size, const char *dir, const char *leaf);

#endif    // P101_REPORT_IO_H
