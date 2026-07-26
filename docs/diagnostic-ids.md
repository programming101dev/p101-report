# p101 diagnostic IDs

`p101-report` emits stable IDs for resource findings. The wording around a
finding can improve over time, but the ID should remain stable enough to cite in
assignments, tests, and cohort summaries.

| ID | Finding | Meaning |
| --- | --- | --- |
| `P101-FD-001` | leaked descriptor | A descriptor was opened and remained live at process end. |
| `P101-FD-002` | double close | A descriptor was closed more than once. |
| `P101-FD-003` | close of unknown descriptor | A close was observed for a descriptor not known to be live. |
| `P101-FD-004` | descriptor inherited across exec | A tracked descriptor was live at an exec boundary without `FD_CLOEXEC`. |
| `P101-ALLOC-001` | leaked allocation | A heap allocation remained live at process end. |
| `P101-ALLOC-002` | double free | A pointer was freed more than once. |
| `P101-ALLOC-003` | free of unknown pointer | A free was observed for a pointer not known to be live. |
| `P101-ALLOC-004` | realloc of unknown pointer | A realloc was observed for a pointer not known to be live. |

These IDs describe p101-observed behavior. If a program bypasses the wrappers,
the p101 tools may not see the operation. Use `p101-wrapper-audit` as the gate
that keeps the resource tools honest.
