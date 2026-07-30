#!/usr/bin/env bash
set -euo pipefail

tool=$1
work=$(mktemp -d "${TMPDIR:-/tmp}/p101-report-test.XXXXXX")
trap 'rm -rf "$work"' EXIT

expect() {
  wanted=$1
  shift
  set +e
  "$tool" "$@" >"$work/stdout" 2>"$work/stderr"
  got=$?
  set -e
  if [ "$got" -ne "$wanted" ]; then
    cat "$work/stderr" >&2
  fi
  [ "$got" -eq "$wanted" ]
}

cat >"$work/resources.log" <<'LOG'
P101FD	4	123	7	1	100	200	OPEN	3	10	main	fixture.c
P101ALLOC	4	123	7	2	110	210	ALLOC	0x1000	-	64	11	main	fixture.c
P101RESOURCE	4	123	7	3	120	220	ACQUIRE	mapping	map-1	-	4096	private	12	main	fixture.c
P101COMPLETE	4	123	7	4	130	230	3	0	0
LOG
cat >"$work/calls.log" <<'LOG'
P101CALL	4	123	7	1	100	200	ENTER	10	main	p101_open	path=x	-	fixture.c
P101CALL	4	123	7	2	101	201	EXIT	10	main	p101_open	-	3	fixture.c
P101COMPLETE	4	123	7	3	130	230	2	0	0
LOG

expect 0 --help
expect 0 -h
expect 2
expect 2 -Z
expect 2 -r
expect 2 -c
expect 2 -d
expect 2 -b
expect 2 "-"$'\001'
export P101_REPORT_TEST_OPTION=@
expect 2
export P101_REPORT_TEST_OPTION=$'\001'
expect 2
export P101_REPORT_TEST_OPTION=:
expect 2
unset P101_REPORT_TEST_OPTION
expect 2 -r "$work/resources.log"
expect 2 -c "$work/calls.log"
expect 2 -d "$work" "$work"
expect 2 "$work" extra
expect 2 -d ''
expect 2 -b '' "$work"
expect 2 -j -b "$work" "$work"
expect 2 -r '' -c "$work/calls.log"
expect 2 -r "$work/resources.log" -c ''
expect 1 -v "$work"
expect 1 -j "$work"
expect 1 -m "$work"
expect 1 -b "$work" "$work"
test -s "$work/correlated-report.txt"
test -s "$work/correlated-report.json"
test -s "$work/resource-lifetimes.md"
expect 1 -r "$work/resources.log" -c "$work/calls.log"

cat >"$work/clean-resources.log" <<'LOG'
P101FD	4	123	7	1	100	200	OPEN	3	10	main	fixture.c
P101FD	4	123	7	2	110	210	CLOSE	3	11	main	fixture.c
P101COMPLETE	4	123	7	3	130	230	2	0	0
LOG
expect 0 -r "$work/clean-resources.log" -c "$work/calls.log"
expect 0 -j -r "$work/clean-resources.log" -c "$work/calls.log"

expect 2 -r "$work/missing.log" -c "$work/calls.log"
expect 2 -r /tmp -c "$work/calls.log"
expect 2 -r "$work/resources.log" -c /tmp
printf 'P101FD\t3\t123\t7\t1\t100\t200\tOPEN\t3\t10\tmain\tfixture.c\n' >"$work/bad.log"
expect 2 -r "$work/bad.log" -c "$work/calls.log"

all_resources="$work/all-resources.log"
sequence=0
emit_resource() {
  magic=$1
  shift
  sequence=$((sequence + 1))
  printf '%s\t4\t200\t9\t%s\t%s\t%s\t%s\n' "$magic" "$sequence" "$((1000 + sequence))" "$((2000 + sequence))" "$*" >>"$all_resources"
}
: >"$all_resources"
emit_resource P101FD $'OPEN\t3\t10\topen_fd\tall.c'
emit_resource P101FD $'CLOSE\t3\t11\tclose_fd\tall.c'
emit_resource P101FD $'CLOSE\t3\t12\tclose_again\tall.c'
emit_resource P101FD $'CLOSE\t99\t13\tclose_unknown\tall.c'
emit_resource P101ALLOC $'ALLOC\t0xa\t-\t1\t20\talloc_one\tall.c'
emit_resource P101ALLOC $'FREE\t0xa\t-\t0\t21\tfree_one\tall.c'
emit_resource P101ALLOC $'FREE\t0xa\t-\t0\t22\tfree_again\tall.c'
emit_resource P101ALLOC $'FREE\t0xb\t-\t0\t23\tfree_unknown\tall.c'
emit_resource P101ALLOC $'REALLOC\t0xb\t0xc\t64\t24\trealloc_unknown\tall.c'
emit_resource P101ALLOC $'REALLOC\t-\t0xd\t32\t25\trealloc_null\tall.c'
emit_resource P101ALLOC $'REALLOC\t0xd\t-\t0\t26\trealloc_free\tall.c'
emit_resource P101ALLOC $'ALLOC\t-\t-\t0\t27\talloc_null\tall.c'
emit_resource P101FD $'OPEN\t4\t30\topen_parent\tall.c'
emit_resource P101FORK $'201\t31\tfork_child\tall.c'
emit_resource P101FD $'CLOSE\t4\t32\tchild_close\tall.c'
emit_resource P101FD $'OPEN\t5\t33\topen_exec\tall.c'
emit_resource P101EXEC $'5\t0\t34\texec_live\tall.c\t/bin/echo'
emit_resource P101SPAWN $'202\t35\tspawn\tall.c\t/bin/true'
emit_resource P101FD $'OPEN\t6\t36\topen_cloexec\tall.c'
emit_resource P101EXEC $'6\t1\t37\texec_cloexec\tall.c\t/bin/echo'
emit_resource P101SPAWN $'203\t38\tspawn_after_exec\tall.c\t/bin/true'
emit_resource P101FD $'OPEN\t7\t39\topen_failed_exec\tall.c'
emit_resource P101EXEC $'7\t0\t40\texec_fail\tall.c\t/missing'
emit_resource P101EXECFAIL $'40\texec_fail\tall.c\t/missing'
emit_resource P101RESOURCE $'ACQUIRE\tmapping\tmap-1\t-\t4096\tprivate\t50\tacquire\tall.c'
emit_resource P101RESOURCE $'ACQUIRE\tmapping\tmap-1\t-\t4096\tprivate\t51\tduplicate\tall.c'
emit_resource P101RESOURCE $'RELEASE\tmapping\tmap-1\t-\t0\t-\t52\trelease\tall.c'
emit_resource P101RESOURCE $'RELEASE\tmapping\tmap-1\t-\t0\t-\t53\tdouble_release\tall.c'
emit_resource P101RESOURCE $'RELEASE\tmapping\tmap-x\t-\t0\t-\t54\tstray_release\tall.c'
emit_resource P101RESOURCE $'REPLACE\tmapping\tmap-x\t-\t8192\tprivate\t55\tbad_replace\tall.c'
emit_resource P101RESOURCE $'ACQUIRE\tmapping\tmap-2\t-\t1024\tprivate\t56\tacquire_transfer\tall.c'
emit_resource P101RESOURCE $'TRANSFER\tmapping\tmap-2\tmap-3\t1024\tprivate\t57\ttransfer\tall.c'
emit_resource P101RESOURCE $'RELEASE\tmapping\tmap-3\t-\t0\t-\t58\trelease_transfer\tall.c'
printf 'P101COMPLETE\t4\t200\t9\t%s\t9999\t9999\t%s\t0\t0\n' "$((sequence + 1))" "$sequence" >>"$all_resources"

all_calls="$work/all-calls.log"
cat >"$all_calls" <<'LOG'
P101CALL	4	200	9	1	1001	2001	ENTER	10	open_fd	p101_open	path=x	-	all.c
P101CALL	4	200	9	2	1002	2002	ENTER	10	open_fd	nested	x	-	all.c
P101CALL	4	200	9	3	1003	2003	EXIT	10	open_fd	nested	-	ok	all.c
P101CALL	4	200	9	4	1004	2004	EXIT	10	open_fd	p101_open	-	3	all.c
P101CALL	4	999	1	1	1005	2005	ENTER	1	other	other	-	-	other.c
P101COMPLETE	4	200	9	5	9999	9999	4	0	0
P101COMPLETE	4	999	1	2	9999	9999	1	0	0
LOG

expect 1 -r "$all_resources" -c "$all_calls"
expect 1 -j -r "$all_resources" -c "$all_calls"
expect 1 -m -r "$all_resources" -c "$all_calls"

mixed_resources="$work/mixed-resources.log"
{
  printf 'ordinary text\n'
  printf 'P101CALL\t4\t300\t1\t1\t10\t20\tENTER\t1\tf\tcall\t-\t-\tx.c\n'
  printf 'P101FD\t3\t300\t1\t2\t11\t21\tOPEN\t3\t1\tf\tx.c\n'
  printf 'P101FD\t4\tbad\n'
  printf 'P101COMPLETE\t4\t300\t1\t3\t12\t22\t2\t0\t0\n'
} >"$mixed_resources"
mixed_calls="$work/mixed-calls.log"
{
  printf 'ordinary text\n'
  printf 'P101FD\t4\t300\t1\t1\t10\t20\tOPEN\t3\t1\tf\tx.c\n'
  printf 'P101CALL\t3\t300\t1\t2\t11\t21\tENTER\t1\tf\tcall\t-\t-\tx.c\n'
  printf 'P101CALL\t4\tbad\n'
  printf 'P101COMPLETE\t4\t300\t1\t3\t12\t22\t2\t0\t0\n'
} >"$mixed_calls"
expect 2 -r "$mixed_resources" -c "$mixed_calls"

malformed_calls="$work/malformed-calls.log"
{
  printf 'P101CALL\t4\tbad\n'
  printf 'P101COMPLETE\t4\t123\t7\t2\t130\t230\t1\t0\t0\n'
} >"$malformed_calls"
expect 2 -r "$work/clean-resources.log" -c "$malformed_calls"

old_calls="$work/old-calls.log"
{
  printf 'P101CALL\t3\t123\t7\t1\t100\t200\tENTER\t10\tmain\tp101_open\t-\t-\tfixture.c\n'
  printf 'P101COMPLETE\t4\t123\t7\t2\t130\t230\t1\t0\t0\n'
} >"$old_calls"
expect 2 -r "$work/clean-resources.log" -c "$old_calls"

incomplete_resources="$work/incomplete-resources.log"
printf 'P101FD\t4\t123\t7\t1\t100\t200\tOPEN\t3\t10\tmain\tfixture.c\n' >"$incomplete_resources"
expect 2 -r "$incomplete_resources" -c "$work/calls.log"

incomplete_calls="$work/incomplete-calls.log"
printf 'P101CALL\t4\t123\t7\t1\t100\t200\tENTER\t10\tmain\tp101_open\t-\t-\tfixture.c\n' >"$incomplete_calls"
expect 2 -r "$work/clean-resources.log" -c "$incomplete_calls"

for index in $(seq 1 45); do
  P101_FAULT_CALL=$index P101_FAULT_ERRNO=5 \
    "$tool" -r "$work/resources.log" -c "$work/calls.log" >/dev/null 2>&1 || :
done
