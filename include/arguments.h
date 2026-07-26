#ifndef P101_REPORT_ARGUMENTS_H
#define P101_REPORT_ARGUMENTS_H

#include <stdbool.h>

struct arguments
{
    const char *report_dir;
    const char *resource_log;
    const char *call_log;
    bool        verbose;
};

#endif    // P101_REPORT_ARGUMENTS_H
