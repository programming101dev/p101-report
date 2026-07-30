# p101-report

`p101-report` reads the two logs produced by the p101 observer workflow and
turns them into one correlated story:

- `resources.log` from `P101_RESOURCE_LOG`
- `calls.log` from `P101_CALL_LOG`

It replays descriptor, allocation, and generic resource lifecycle events, finds
leaks and bad releases, then prints matching call records for the same
pid/source site. It is the “what happened here?” layer above
`p101-resource-tracker` and `p101-trace`.
Failed exec attempts are rolled back when their `P101EXECFAIL` record arrives,
so only descriptors crossing a successful image boundary are reported.
Successful `posix_spawn` calls appear as `P101SPAWN` lifetime boundaries. The
report retains their parent, child, target, timestamp, and source site, but does
not invent a child descriptor table because portable spawn file actions are
opaque.

## Usage

```sh
p101-report [-h] [-v] [-j|-m] [-b <output-dir>] [-d <report-dir>] [-r <resources.log> -c <calls.log>] [report-dir]
```

Examples:

```sh
p101-observe -o run-report -- ./my-program config.txt
p101-report run-report
p101-report -j run-report
p101-report -m run-report > resource-lifetimes.md
p101-report -b run-report run-report

p101-report -r run-report/resources.log -c run-report/calls.log
```

When given a report directory, `p101-report` reads:

```text
<report-dir>/resources.log
<report-dir>/calls.log
```

## Output

The report starts with parse health for both streams, then lists resource
findings with trace context:

```text
1. leaked descriptor
   resource: fd 3, pid 123, resource event #4
   site:     server.c:41 in open_config
   trace:
     #9 ENTER p101_open(path=config.txt) -> - at server.c:41 in open_config
     #10 EXIT  p101_open(-) -> 3 at server.c:41 in open_config
```

Both logs must contain a successful `P101COMPLETE` receipt.
Missing receipts and producer write failures are reported as incomplete
evidence and prevent a clean exit status. Older event formats are rejected.

Use `-j` for the same report as JSON when another tool or grading script needs
to consume the result. Use `-m` to render resource lifetimes as a Mermaid graph
for Markdown viewers that support Mermaid.

Use `-b <output-dir>` to generate `correlated-report.txt`,
`correlated-report.json`, and `resource-lifetimes.md` from one parse and one
canonical runtime model. This is the mode used by `p101-observe`.

Findings include stable diagnostic IDs such as `P101-FD-001` and
`P101-ALLOC-002`. The current ID list is documented in
[`docs/diagnostic-ids.md`](docs/diagnostic-ids.md).

JSON output includes `event_schema` and `event_id_policy`. The event IDs shown
in reports are derived from the 1-based parsed-record sequence in each input
stream, while lifetime durations use the v3 monotonic timestamps emitted by
`lib_env`. The canonical log contract is documented by `p101-observe` in
the
[`lib_tool_event` protocol specification](https://github.com/programming101dev/lib_tool_event/blob/main/docs/event-format.md).
Every JSON finding also uses the common `id`, `severity`, `location`, `message`,
and `evidence` envelope; trace and resource-specific fields remain alongside
that envelope.

The reader uses `P101_TOOL_EVENT_LINE_MAX_BYTES` from `lib_tool_event` and the same
exec/CLOEXEC semantics as `p101-resource-tracker`. The stack regression corpus
compares their findings case by case so the two models cannot silently drift.
That parity includes `P101-RESOURCE-001` through `P101-RESOURCE-005` for
generic leaks, double releases, stray releases, and malformed replacements.

The goal is not to replace the lower-level tools. `p101-resource-tracker` remains
the strict analyzer, and `p101-trace` remains the dedicated call-log renderer.
`p101-report` correlates their raw inputs into one teaching-friendly narrative.

## Boundaries

`p101-report` only knows what is present in `resources.log` and `calls.log`.
Missing wrapper events, direct non-p101 calls, third-party behavior, and
unsupported event schemas are outside its model. The correlated story is
replayable evidence from those logs, not proof about unobserved executions.

## Exit status

| Status | Meaning |
| --- | --- |
| `0` | Both logs parsed and no resource findings were found |
| `1` | The logs parsed and at least one resource finding was found |
| `2` | Bad usage or a log could not be read |

## Build and check

```sh
./change-compiler.sh -c clang
./check.sh
```
