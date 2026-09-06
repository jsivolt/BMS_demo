# Working agreement for AI coding agents

## Scope discipline

Implement only what the plan document (e.g. `src/battery/SOC_DESIGN.md`) or the
chat request specifies. Do not add functions, helpers, or features that were not
asked for.

- If a spec lists the functions to add, add exactly those.
- If you spot a gap, a missing piece, or a worthwhile improvement, **describe it
  and ask before implementing it.** Do not fold unrequested code into the change.
- Resolving an ambiguity or contradiction *inside* the agreed scope is fine
  (choosing a function signature, keeping existing clamping/validation behavior).
  Adding new capability is not.
- Fixing a genuine blocker that stops the requested work from compiling or
  linking is in scope — call it out when you report back.
- This does not restrict investigation, code review, or asking questions. It
  restricts writing unrequested implementation code.

The goal is a small, predictable diff that maps directly to what was discussed,
so review stays fast.
