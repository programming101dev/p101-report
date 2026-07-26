# p101-report commands

Quick reference for `p101-report`. Every script also supports `--help`.

## Program

| Command | What it does |
| --- | --- |
| `p101-report run-report` | Correlate `run-report/resources.log` and `run-report/calls.log` |
| `p101-report -j run-report` | Write the correlated report as JSON |
| `p101-report -m run-report > lifetimes.md` | Write a Mermaid resource lifetime graph |
| `p101-report -d run-report` | Same, explicit directory option |
| `p101-report -r resources.log -c calls.log` | Correlate explicit log paths |
| `p101-report -v run-report` | Enable p101 tracing inside the reporter |

## Project workflow

| Command | What it does |
| --- | --- |
| `./change-compiler.sh -c clang` | Configure with clang |
| `./build.sh` | Build through the strict analysis pipeline |
| `./test.sh` | Run unit tests |
| `./check.sh` | Run the local quality gate |
| `./fuzz.sh` | Run the parser fuzzer smoke |
