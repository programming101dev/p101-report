# p101-report

`p101-report` reads the two logs produced by the p101 observer workflow and
turns them into one correlated story:

- `resources.log` from `P101_RESOURCE_LOG`
- `calls.log` from `P101_CALL_LOG`

It replays descriptor and allocation events, finds leaks and bad releases, then
prints matching call records for the same pid/source site. It is the “what
happened here?” layer above `p101-resource-tracker` and `p101-trace`.

## Usage

```sh
p101-report [-h] [-v] [-j|-m] [-d <report-dir>] [-r <resources.log> -c <calls.log>] [report-dir]
```

Examples:

```sh
p101-observe -o run-report -- ./my-program config.txt
p101-report run-report
p101-report -j run-report
p101-report -m run-report > resource-lifetimes.md

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

Use `-j` for the same report as JSON when another tool or grading script needs
to consume the result. Use `-m` to render resource lifetimes as a Mermaid graph
for Markdown viewers that support Mermaid.

Findings include stable diagnostic IDs such as `P101-FD-001` and
`P101-ALLOC-002`. The current ID list is documented in
[`docs/diagnostic-ids.md`](docs/diagnostic-ids.md).

JSON output includes `event_schema` and `event_id_policy`. The event IDs shown
in reports are derived from the 1-based parsed-record sequence in each input
stream. The canonical v1 log contract is documented by `p101-observe` in
`../p101-observe/docs/event-format.md`.

The goal is not to replace the lower-level tools. `p101-resource-tracker` remains
the strict analyzer, and `p101-trace` remains the dedicated call-log renderer.
`p101-report` correlates their raw inputs into one teaching-friendly narrative.

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
