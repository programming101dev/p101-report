#ifndef P101_REPORT_ARGUMENTS_H
#define P101_REPORT_ARGUMENTS_H

#include <stdbool.h>

enum report_format
{
    REPORT_FORMAT_TEXT = 0,
    REPORT_FORMAT_JSON,
    REPORT_FORMAT_MERMAID
};

struct arguments
{
    const char        *report_dir;
    const char        *resource_log;
    const char        *call_log;
    const char        *bundle_output_dir;
    enum report_format format;
    bool               verbose;
};

#endif    // P101_REPORT_ARGUMENTS_H
