# External Agent CLI Arguments

Use these exact command shapes when launching external read-only investigation
agents from this repo.

## Grok

For real code investigation with repository reads, do not use `-p`. Run Grok
as an agent from a real terminal/TTY:

```bash
grok "$PROMPT" \
  --cwd /home/kapablanka/repos/machgate \
  --permission-mode plan \
  --always-approve \
  --max-turns 12 \
  --no-alt-screen \
  --no-subagents \
  --no-memory
```

If the session needs to be captured cleanly, export it afterward:

```bash
grok sessions list
grok export <session-id> /tmp/grok-session.md
```

Use `-p` only when the prompt already contains all evidence and Grok should not
inspect files:

```bash
grok -p "$PROMPT" \
  --cwd /home/kapablanka/repos/machgate \
  --permission-mode plan \
  --max-turns 4 \
  --no-memory \
  --no-subagents \
  --output-format plain
```

Do not pass `--effort` or `--reasoning-effort` to the default `grok-build`
model. It rejects that parameter with:

```text
invalid-argument: Model grok-build does not support parameter reasoningEffort
```

Do not use `--max-turns 1` for code investigation. It can exit before doing
work with:

```text
Max turns reached
Error: max turns reached
```

Do not rely on `-p` for repository investigation. It can exit successfully while
only printing a status sentence such as:

```text
I'll inspect the guard-acquire implementation and related libc++abi code.
```

That is no useful result. Either run the TTY command above or provide all
evidence in the `-p` prompt.

## Agy

Use `--print` for one-shot headless output.

```bash
agy --print "$PROMPT" --print-timeout 10m
```

For read-only investigation, do not add permission bypass flags.

## Claude

Use `-p` for one-shot headless output.

```bash
claude -p "$PROMPT" --output-format text
```

If this returns:

```text
Failed to authenticate. API Error: 401 Invalid authentication credentials
```

then Claude is not available in headless mode for this shell. Report that exact
error and continue with Grok, Agy, and local subagents.
