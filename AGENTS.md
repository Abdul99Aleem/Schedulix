# AGENTS.md

Instructions for OpenCode in this repository.

## QNX documentation is the source of truth

This project targets **QNX SDP 8.0** (`qcc` 12.2.0, `aarch64le`, QNX OS 8.0.0,
Raspberry Pi 4).

Before answering or changing anything about QNX OS behavior, kernel or driver
APIs, build flags, or target configuration, consult the official documentation:

<https://www.qnx.com/developers/docs/qnxeverywhere/index.html>

- **Do not answer QNX questions from memory.** QNX differs from Linux in ways that
  silently break builds and runtime behavior. Look it up, then answer.
- Load the **`qnx-docs`** skill for the guide map, the offline page index, and
  lookup recipes. Use it for every QNX API, driver, or target-image question.
- Fetch only the specific page you need. `qnx.com` is a large, slow site; do not
  re-download its index to re-confirm something already established this session.
- `qnx.com` intermittently refuses connections, and the same URL can succeed and
  then time out. Retry a few times before treating a page as missing. If it still
  fails, say so instead of guessing.
- Cite the guide and page you used for non-obvious claims.

## Repo-local context

`CURRENT_STATE.md` holds the current validated state of the project; `docs/` holds
phase reports and target logs. Read them before changing build, deployment, or
validation procedures.