# Project Specification and Reconstruction Rules

## 1. Purpose

This repository contains an existing software project and a corresponding reconstruction specification.

The documentation under `docs/` is intended to describe the application sufficiently precisely that the application can later be recreated from an empty repository without access to the original source code.

The primary goals are:

1. Preserve the functional behavior of the existing application.
2. Preserve important architectural decisions.
3. Preserve the domain model and business rules.
4. Preserve externally observable behavior.
5. Make the project reproducible by another developer or AI coding agent.
6. Avoid undocumented assumptions.

---

# 2. Source of Truth

The project has two distinct states:

### Existing implementation

The existing source code represents the current implementation.

Use it to discover:

* actual behavior
* actual dependencies
* actual data structures
* actual API behavior
* actual UI behavior
* actual configuration
* actual tests
* actual technical constraints

### Reconstruction specification

The documentation under `docs/` represents the intended knowledge required to recreate the application.

Relevant documents include:

```text
docs/
├── 00-overview.md
├── 01-requirements.md
├── 02-architecture.md
├── 03-domain-model.md
├── 04-data-model.md
├── 05-api.md
├── 06-business-rules.md
├── 07-ui.md
├── 08-security.md
├── 09-configuration.md
├── 10-testing.md
├── 11-deployment.md
├── 12-dependencies.md
├── 13-project-structure.md
├── 14-reconstruction-plan.md
└── 15-consistency-audit.md
```

During reconstruction, the documentation is the primary specification.

Do not replace documented requirements with personal design preferences.

---

# 3. General AI Coding Rules

Before implementing a feature:

1. Read the relevant documentation.
2. Identify the requirements involved.
3. Identify related domain objects.
4. Identify related business rules.
5. Identify architectural constraints.
6. Identify relevant APIs and persistence requirements.
7. Identify relevant tests.
8. Implement only after understanding these dependencies.

Do not immediately start writing code when the requirements are unclear.

First determine what information is available.

---

# 4. Never Invent Requirements

Do not invent:

* business rules
* validation rules
* API behavior
* database fields
* user permissions
* workflows
* UI behavior
* external integrations
* configuration values
* security requirements
* error handling behavior

If required information is missing, explicitly report:

```text
[BLOCKED BY MISSING INFORMATION]
```

Explain exactly what information is missing and why it is required.

Do not silently choose a behavior merely because it appears reasonable.

---

# 5. Evidence and Confidence

When analyzing the existing implementation, distinguish between:

### FACT

Directly supported by source code, configuration, tests, schema, API definitions, or other repository artifacts.

### INFERRED

Behavior strongly implied by the implementation but not explicitly documented.

### UNKNOWN

The repository does not provide enough information to determine the intended behavior.

### ASSUMPTION

A temporary assumption required to continue development.

Assumptions must never silently become requirements.

When appropriate, document the evidence and confidence level.

Use:

```text
Confidence: HIGH
Confidence: MEDIUM
Confidence: LOW
```

---

# 6. Documentation Rules

Documentation must describe behavior precisely enough to support reconstruction.

Prefer:

```text
The endpoint POST /api/customers creates a customer.

Required fields:
- firstName
- lastName
- email

The email address must be unique.

If the email already exists, the API returns HTTP 409.
```

over:

```text
The customer API allows users to create customers.
```

Avoid vague language such as:

* usually
* probably
* should
* might
* handles
* manages
* etc.

unless the uncertainty is intentional and explicitly documented.

---

# 7. Documentation Updates

When implementing a change that affects documented behavior:

1. Identify affected documentation.
2. Update the documentation.
3. Update the implementation.
4. Update or create tests.
5. Check the consistency of related documents.

Documentation and implementation must not silently diverge.

Examples of changes that normally require documentation updates:

* new API endpoint
* changed API response
* new entity
* changed database field
* changed business rule
* new permission
* changed workflow
* new external service
* new environment variable
* changed deployment requirement
* changed user-visible behavior

---

# 8. Architecture Rules

Follow the architecture documented in:

```text
docs/02-architecture.md
```

Do not introduce a new architectural pattern merely because it is personally preferred.

Before introducing a new architectural component, determine whether the documentation already defines an appropriate location.

If the existing architecture is insufficient, report the architectural gap before making a major architectural change.

---

# 9. Domain Model Rules

The domain model documented in:

```text
docs/03-domain-model.md
```

and the business rules documented in:

```text
docs/06-business-rules.md
```

must be treated as authoritative during reconstruction.

Do not duplicate business rules unnecessarily across unrelated layers.

Keep domain terminology consistent.

If the same concept has different names in different parts of the implementation, identify the inconsistency instead of silently renaming everything.

---

# 10. Database Rules

Follow:

```text
docs/04-data-model.md
```

Database changes must consider:

* primary keys
* foreign keys
* unique constraints
* indexes
* nullability
* defaults
* cascading behavior
* migrations
* transaction boundaries

Do not remove constraints merely to make implementation easier.

Do not introduce undocumented database structures without explaining the reason.

---

# 11. API Rules

Follow:

```text
docs/05-api.md
```

For externally visible APIs, preserve documented:

* HTTP methods
* paths
* parameters
* request structures
* response structures
* status codes
* validation behavior
* authentication requirements
* authorization requirements
* error behavior

Do not change an API contract merely to simplify the implementation.

---

# 12. Business Rules

Business rules are more important than implementation details.

When implementing functionality, identify the applicable rules in:

```text
docs/06-business-rules.md
```

A business rule must not be accidentally lost because the implementation was moved to another class, service, controller, component, or module.

When adding a new business rule, document it explicitly.

---

# 13. UI Rules

For frontend changes, follow:

```text
docs/07-ui.md
```

Preserve documented:

* routes
* navigation
* workflows
* forms
* validation
* permissions
* loading states
* error states
* empty states
* API interactions

Do not invent UI behavior that is not specified unless the user explicitly requests a new design.

---

# 14. Security Rules

Follow:

```text
docs/08-security.md
```

Never expose:

* passwords
* API keys
* access tokens
* private keys
* secrets
* credentials

Never place real credentials into documentation or source code.

Use placeholders when documenting configuration.

---

# 15. Configuration Rules

Follow:

```text
docs/09-configuration.md
```

Every required environment variable or external dependency should be documented.

Never hard-code secrets.

If a configuration value is unknown, use:

```text
[UNKNOWN]
```

rather than inventing a value.

---

# 16. Testing Rules

Testing is part of the specification.

Follow:

```text
docs/10-testing.md
```

and existing tests.

When implementing functionality:

1. Identify existing relevant tests.
2. Preserve existing behavior.
3. Add tests for new behavior.
4. Test important error cases.
5. Test business rules.
6. Run the relevant test suite.

Do not delete or weaken tests merely to make the implementation pass.

If a test contradicts the specification, report the contradiction.

Do not silently choose one interpretation.

---

# 17. Reconstruction Workflow

When recreating the application from an empty repository, follow:

```text
docs/14-reconstruction-plan.md
```

Implement incrementally.

Recommended order:

```text
1. Project initialization
2. Dependencies
3. Configuration
4. Domain model
5. Database
6. Business logic
7. Backend/API
8. Authentication/authorization
9. Frontend/UI
10. External integrations
11. Tests
12. Build
13. Deployment
```

Do not attempt to implement the entire application in one step.

After each phase:

1. Build the project.
2. Run relevant tests.
3. Check for errors.
4. Verify the implementation against the specification.
5. Only then continue to the next phase.

---

# 18. Change Management

Before making a significant change, answer:

```text
What requirement does this change implement?

Which documentation files describe this requirement?

Which components are affected?

Which tests verify the behavior?

Does the change modify an external contract?

Does the change require documentation updates?
```

For significant changes, provide this information before implementation.

---

# 19. Handling Contradictions

If two sources disagree, do not silently resolve the contradiction.

Use this priority order as an investigation guide:

1. Explicit user requirement
2. Explicit acceptance criteria
3. Documented business rule
4. Existing externally observable behavior
5. Tests
6. Implementation details
7. Assumptions

However, when the conflict is significant, stop and report it rather than making a silent decision.

Document contradictions in:

```text
docs/15-consistency-audit.md
```

---

# 20. Code Quality

Code should be:

* readable
* maintainable
* testable
* consistent with the existing project
* consistent with documented architecture
* appropriately modular
* free of unnecessary complexity

Do not introduce abstractions merely for the sake of abstraction.

Do not refactor unrelated code while implementing a specific requirement unless explicitly requested.

---

# 21. Dependencies

Before adding a new dependency:

1. Check whether an existing dependency already provides the required functionality.
2. Check whether the architecture allows the dependency.
3. Check whether the dependency is documented.
4. Consider its impact on deployment and maintenance.

When a new dependency is intentionally introduced, update:

```text
docs/12-dependencies.md
```

---

# 22. AI Reconstruction Mode

When explicitly instructed to reconstruct the project from the specification, behave as an implementation agent.

Before coding:

1. Read all relevant `docs/` files.
2. Read `docs/14-reconstruction-plan.md`.
3. Identify the current reconstruction phase.
4. Identify dependencies of that phase.
5. Identify missing information.

Then provide a short implementation plan.

Implement only the current phase unless explicitly instructed otherwise.

After implementation, report:

```text
Implemented:
- ...

Files changed:
- ...

Tests:
- ...

Specification references:
- ...

Known gaps:
- ...

Potential ambiguities:
- ...
```

---

# 23. Reconstruction Stop Conditions

Stop implementation and request clarification when:

* a required business rule is missing
* two requirements contradict each other
* a required API contract is undefined
* a required database relationship is unclear
* authentication behavior is undefined
* a security-sensitive behavior is ambiguous
* an external service contract is missing
* multiple materially different implementations are possible

Do not guess in these situations.

---

# 24. Documentation Completeness

The documentation is considered reconstruction-ready only when another developer or AI agent can answer:

* What does the system do?
* Who uses it?
* What are the domain entities?
* What are the business rules?
* How is data stored?
* What APIs exist?
* What does each API accept and return?
* How does authentication work?
* What does the UI contain?
* What external systems are required?
* What configuration is required?
* How is the application tested?
* How is it built?
* How is it deployed?
* In what order should it be reconstructed?
* Which behaviors remain unknown?

If any of these cannot be answered, identify the missing information explicitly.

---

# 25. Final Principle

The goal is not to produce documentation that merely looks complete.

The goal is to produce documentation that is:

* precise
* traceable
* testable
* internally consistent
* implementation-oriented
* reproducible

When in doubt:

**Do not invent. Inspect, document, test, and ask for clarification.**
