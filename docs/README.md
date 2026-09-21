


# Project Reconstruction Specification

This directory contains the authoritative specification for reconstructing the application.

## Documentation Map

| File                        | Purpose                                    |
| --------------------------- | ------------------------------------------ |
| `00-overview.md`            | Purpose, scope and system overview         |
| `01-requirements.md`        | Functional and non-functional requirements |
| `02-architecture.md`        | System architecture and components         |
| `03-domain-model.md`        | Domain entities and relationships          |
| `04-data-model.md`          | Database and persistence model             |
| `05-api.md`                 | External APIs and interfaces               |
| `06-business-rules.md`      | Business rules and workflows               |
| `07-ui.md`                  | User interface and user workflows          |
| `08-security.md`            | Authentication, authorization and security |
| `09-configuration.md`       | Configuration and environment              |
| `10-testing.md`             | Test strategy and test cases               |
| `11-deployment.md`          | Build and deployment                       |
| `12-dependencies.md`        | Dependencies and external systems          |
| `13-project-structure.md`   | Repository and source structure            |
| `14-reconstruction-plan.md` | Step-by-step reconstruction plan           |
| `15-consistency-audit.md`   | Documentation consistency and known gaps   |

## Reading Order

When learning the system, read:

1. `00-overview.md`
2. `01-requirements.md`
3. `02-architecture.md`
4. `03-domain-model.md`
5. `04-data-model.md`
6. `06-business-rules.md`
7. `05-api.md`
8. `07-ui.md`
9. `08-security.md`
10. `09-configuration.md`
11. `10-testing.md`
12. `11-deployment.md`
13. `12-dependencies.md`
14. `13-project-structure.md`
15. `14-reconstruction-plan.md`
16. `15-consistency-audit.md`

## Reconstruction

When rebuilding the application from scratch, do not implement everything at once.

Follow:

```text
docs/14-reconstruction-plan.md
```

and implement one phase at a time.

The specification deliberately distinguishes between:

* `FACT` — directly supported by repository evidence
* `INFERRED` — derived from implementation behavior
* `UNKNOWN` — cannot currently be determined
* `ASSUMPTION` — temporary assumption

Unknown information must not be silently converted into implementation behavior.

## Specification Principle

The objective is reproducibility.

A new implementation should be able to reproduce the externally observable behavior of the existing application without requiring access to the original source code.
