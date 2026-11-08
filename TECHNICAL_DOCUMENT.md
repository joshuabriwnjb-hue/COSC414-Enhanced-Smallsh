# Phase 1: Enhanced UNIX Shell — Technical Document

**Course:** COSC 414 Operating Systems  
**Student:** Joshua Brown  
**Team members:** [List every actual team member]  
**Submission date:** November 8, 2026

## Team responsibilities

| Member | Assigned tasks | Work actually completed |
| --- | --- | --- |
| Joshua Brown | [Confirm responsibilities] | [Describe actual contribution] |
| [Member name, or remove row] | [Assigned tasks] | [Completed work] |

Complete this table from the team's actual division of work. Do not claim contributions that have not occurred.

## Development environment

The intended local testing environment is the Linux terminal on a Chromebook. The assignment specifies Ubuntu in VirtualBox; the environment below must accurately state whether an instructor-approved alternative or the specified VM was used.

- Chromebook model: [Enter model]
- Linux distribution/version from /etc/os-release: [Enter actual output]
- Kernel/architecture from uname -a: [Enter actual output]
- GCC version: [Enter actual output]
- Available processors and memory: [Enter actual values]
- VirtualBox version, Ubuntu guest version and assigned VM resources: [Enter actual details if used; otherwise state not used]
- Instructor authorization for an alternate environment: [Record actual authorization, if applicable]

## Collaboration

Actual collaboration platform(s): [GitHub / Teams / Zoom / GroupMe / other actually used].
Repository or shared-work link: [Enter link if available].
Describe how the team shared source files, coordinated tasks, and reviewed tests: [Complete from actual activity]. Do not claim VM sharing or repository use unless performed.

## Design and implementation

The supplied Chapter 5 smallsh discussion establishes the userin, gettok, procline, and runcommand stages. The enhanced program retains these stages, with definitions consolidated into smallsh.c. userin reads a bounded line; gettok identifies arguments and separators; procline assembles argument vectors; runcommand dispatches built-ins or launches external programs with fork and execvp. Semicolons run successive commands, and ampersands launch background jobs. Chapter 5 exercise questions are outside this submission.

The shell installs a SIGINT handler using sigaction. At the prompt, the handler records interruption and writes a newline using an async-signal-safe operation. Interrupted input is cleared and the prompt is restored. The shell ignores SIGTSTP and terminal-control stop signals after initialization. Children restore default signal dispositions so foreground programs respond normally to Ctrl+C and Ctrl+Z.

Each external command is placed in a separate process group using setpgid in both parent and child. The parent hands the terminal to a foreground job using tcsetpgrp and waits with waitpid and WUNTRACED. A pipe acts as a launch gate so a newly forked child cannot attempt terminal input before the handoff. On termination or stop, the shell regains the terminal and restores its terminal settings. Stopped jobs retain their own saved terminal settings for later fg use.

A fixed job table stores each active job's ID, PGID, command, state and saved terminal settings. jobs displays active entries. bg sends SIGCONT to a process group. fg transfers terminal ownership and resumes the job. kill accepts a job ID or a positive PID; job targets use a negative PGID with kill(2) to signal the whole group. waitpid with WNOHANG, WUNTRACED and WCONTINUED updates background states before prompts and commands. State changes do not occur in signal handlers.

## Compilation

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic -O2 smallsh.c -o smallsh
./smallsh
```

Actual local compilation result: [Record warnings/errors or successful build; attach screenshot].

## Base testing on the student's environment

| Test | Expected behavior | Actual observation / evidence |
| --- | --- | --- |
| echo and semicolon-separated commands | Correct ordered output | Pending local test |
| sleep 300, then Ctrl+C | Child ends; shell survives | Pending local test |
| Ctrl+C at an empty prompt | Fresh prompt; shell survives | Pending local test |
| sleep 300, then Ctrl+Z and jobs | Stopped job listed | Pending local test |
| bg %N, then jobs | Same job runs in background | Pending local test |
| fg %N, then Ctrl+C | Job owns terminal, then terminates | Pending local test |
| sleep 300 & and jobs | Immediate prompt, Running job | Pending local test |
| kill %N, sleep 1, jobs | Job terminated and removed | Pending local test |
| Process-group inspection using ps | Distinct shell and child PGIDs; correct TPGID | Pending local test |
| cat &, then fg %N | Background reader stops; foreground reader works | Pending local test |
| Invalid command/job | Error; shell remains usable | Pending local test |
| exit / Ctrl+D | Shell exits | Pending local test |

Attach actual screenshots marked in START_HERE.md and record IDs observed on your system. Do not substitute sample IDs or development-machine results for your own evidence.

## Preparation-time validation

The delivered source compiled with GCC using -std=c11 -Wall -Wextra -Wpedantic -O2 without warnings in the preparation environment. A separate pseudo-terminal harness checked prompt Ctrl+C, foreground stop/resume/interrupt, background tracking, STOP/TERM, background terminal reads and fg, semicolon parsing, failed exec, invalid job lookup, and exit. These checks passed. They do not constitute execution on the student's Chromebook or the required Ubuntu VirtualBox VM.

## Limitations

The shell tracks one direct child per job and has a fixed 128-job limit. It does not implement pipelines, redirection, expansion, comments, escapes, or cd. Background completion notifications and reaping happen at command/prompt boundaries; a child finishing during idle input may remain a zombie until the next command. Program descendants share group signals but are not separately tracked after their direct parent exits. These limits should be explained during demonstration.

## Conclusion

[After local testing, summarize the observed foreground/background behavior, signal behavior, process-group evidence, and any unresolved issues.]
