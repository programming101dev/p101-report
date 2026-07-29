#ifndef P101_REPORT_TYPES_H
#define P101_REPORT_TYPES_H

#include <stddef.h>

enum resource_kind
{
    RESOURCE_FD_OPEN = 0,
    RESOURCE_FD_CLOSE,
    RESOURCE_ALLOC,
    RESOURCE_FREE,
    RESOURCE_REALLOC,
    RESOURCE_FORK,
    RESOURCE_SPAWN,
    RESOURCE_EXEC,
    RESOURCE_EXEC_FAIL
};

enum call_kind
{
    CALL_ENTER = 0,
    CALL_EXIT
};

enum line_status
{
    LINE_OTHER = 0,
    LINE_OK,
    LINE_MALFORMED,
    LINE_BAD_VERSION
};

enum finding_kind
{
    FINDING_FD_LEAK = 0,
    FINDING_ALLOC_LEAK,
    FINDING_DOUBLE_CLOSE,
    FINDING_STRAY_CLOSE,
    FINDING_DOUBLE_FREE,
    FINDING_STRAY_FREE,
    FINDING_BAD_REALLOC,
    FINDING_EXEC_INHERIT
};

struct source_site
{
    char *file_name;
    char *function_name;
    int   line_number;
};

struct resource_event
{
    enum resource_kind kind;
    long               pid;
    long               child_pid;
    int                fd;
    int                cloexec;
    char              *ptr;
    char              *new_ptr;
    char              *target;
    size_t             size;
    size_t             site;
    size_t             sequence;
    size_t             event_sequence;
    size_t             monotonic_ns;
    size_t             wall_unix_ns;
    int                monotonic_ns_available;
    int                wall_unix_ns_available;
};

struct call_event
{
    enum call_kind kind;
    long           pid;
    int            line_number;
    char          *function_name;
    char          *call_name;
    char          *arguments;
    char          *result;
    char          *file_name;
    size_t         sequence;
    size_t         event_sequence;
    size_t         monotonic_ns;
    size_t         wall_unix_ns;
    int            monotonic_ns_available;
    int            wall_unix_ns_available;
};

struct live_fd
{
    long   pid;
    int    fd;
    size_t site;
    size_t sequence;
};

struct closed_fd
{
    long   pid;
    int    fd;
    size_t site;
    size_t sequence;
};

struct live_alloc
{
    long   pid;
    char  *ptr;
    size_t size;
    size_t site;
    size_t sequence;
};

struct freed_alloc
{
    long   pid;
    char  *ptr;
    size_t site;
    size_t sequence;
};

struct finding
{
    enum finding_kind kind;
    long              pid;
    int               fd;
    char             *ptr;
    size_t            size;
    size_t            site;
    size_t            sequence;
    size_t            previous_site;
    size_t            previous_sequence;
};

struct report_model
{
    struct source_site *sites;
    size_t              site_count;
    size_t              site_capacity;

    struct resource_event *resources;
    size_t                 resource_count;
    size_t                 resource_capacity;

    struct call_event *calls;
    size_t             call_count;
    size_t             call_capacity;

    struct live_fd   *fds;
    size_t            fd_count;
    size_t            fd_capacity;
    struct closed_fd *closed_fds;
    size_t            closed_fd_count;
    size_t            closed_fd_capacity;

    struct live_alloc  *allocs;
    size_t              alloc_count;
    size_t              alloc_capacity;
    struct freed_alloc *freed_allocs;
    size_t              freed_alloc_count;
    size_t              freed_alloc_capacity;

    struct finding *findings;
    size_t          finding_count;
    size_t          finding_capacity;

    size_t resource_records;
    size_t call_records;
    size_t resource_skipped;
    size_t call_skipped;
    size_t resource_malformed;
    size_t call_malformed;
    size_t resource_bad_version;
    size_t call_bad_version;
};

#endif    // P101_REPORT_TYPES_H
