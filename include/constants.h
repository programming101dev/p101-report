#ifndef P101_REPORT_CONSTANTS_H
#define P101_REPORT_CONSTANTS_H

enum
{
    LINE_MAX_BYTES       = 4096,
    PATH_MAX_BYTES       = 4096,
    MSG_LEN              = 256,
    DECIMAL_BASE         = 10,
    JSON_CONTROL_LIMIT   = 0x20U,
    JSON_NON_ASCII_LIMIT = 0x80U,
    FIRST_CAPACITY       = 16,
    TRACE_CONTEXT_LIMIT  = 5,
    LIFETIME_TEXT_LIMIT  = 40,
    EXIT_CLEAN           = 0,
    EXIT_FINDINGS        = 1,
    EXIT_TROUBLE         = 2
};

#endif    // P101_REPORT_CONSTANTS_H
