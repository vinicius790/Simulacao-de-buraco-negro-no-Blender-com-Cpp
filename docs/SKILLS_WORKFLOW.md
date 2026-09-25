# Skills / Plugin Workflow Applied to This Upgrade

The user explicitly requested that the referenced development skills/plugins be evaluated and used where they add real value. They were **not** treated as a reason to add unrelated agent or API dependencies to the simulation.

## OpenAI Developers

Useful scope from the available plugin metadata: repository-first inspection, current official guidance for OpenAI API / Agents SDK / ChatGPT Apps, and validation of OpenAI integrations.

Decision for this project: **no OpenAI runtime added**. The black-hole project is a C++/OpenGL numerical-rendering application and currently has no OpenAI API integration. Adding one would increase complexity without improving the simulation, numerical model, rendering or build reliability.

What was retained from the workflow: inspect the existing repository first and choose the smallest architecture that fits the real requirement.

## agent-sdk-dev

The referenced plugin is designed for creating and verifying Claude Agent SDK applications in Python/TypeScript.

Decision for this project: **not applicable to the runtime**. No Agent SDK scaffolding, environment files or agent loop were added.

What was retained from the workflow: verification before claiming completion, explicit setup boundaries and avoiding unverified assumptions.

## code-review

This is the most directly useful referenced workflow for this project.

Applied principles:

- review correctness before style;
- separate independent issue classes (OpenGL synchronization, std140 layout, CPU work, numerical behavior, build tooling);
- avoid speculative findings and keep only issues supported by code;
- preserve repository-specific behavior rather than blindly enforcing generic style;
- add regression guards for behavior-sensitive changes.

This directly motivated the conservative treatment of blending, fullscreen topology and the legacy Euler integrator.

## engineering

The exact referenced `engineering@claude-cowork` plugin was not exposed as an accessible skill/tool in this session. It is therefore **not falsely claimed as executed**.

The project still applies normal software-engineering fundamentals: explicit invariants, modular build configuration, static analysis configuration, CI, reproducibility notes and staged scientific modernization.

## Ponytail

The exact referenced `Ponytail@claude-cowork` plugin was not exposed as an accessible skill/tool in this session. It is therefore **not falsely claimed as executed** and no behavior is attributed to it.

## everything-claude-code

The useful ideas found in its public workflow/documentation are strongly relevant:

- research/inspect before implementation;
- regression tests before risky refactors;
- focused, reviewable changes;
- static/security/quality checks as separate layers;
- explicit anti-patterns and documented limits;
- avoid context/tooling bloat when a smaller solution is sufficient.

Applied concretely through:

- `tests/validate_source_invariants.py`;
- `docs/BASELINE.md`;
- `docs/VALIDATION_STATUS.md`;
- compiler warning configuration;
- `.clang-tidy` and `.clang-format`;
- GitHub Actions CI;
- separation of the legacy baseline from the future scientific-mode specification.

## Resulting rule for this repository

Use development skills as **review and validation multipliers**, not as dependencies of the simulation itself. The project should only gain a new framework, API or runtime when it solves a measured technical requirement and its cost is justified.
