# Phase 1: Enhanced UNIX Shell

**Student:** Joshua Brown  
**Course:** COSC 414 — Operating Systems  
**Date:** November 8, 2026  
**Repository:** https://github.com/joshuabriwnjb-hue/COSC414-Enhanced-Smallsh

## 1. Project Goal

The goal of Phase 1 was to develop an enhanced Unix shell based on the smallsh command-processing structure presented in Chapter 5. The shell supports foreground and background execution, signal handling, process groups, job tracking, and the built-in commands jobs, bg, fg, and kill.

Chapter 5 was used as a reference for the implementation. Its exercise questions were not part of this submission.

## 2. Individual Responsibilities

This project was completed individually by Joshua Brown. The individual responsibilities covered preparing the development environment, managing the source files, compiling the program, conducting terminal tests, recording results, documenting the implementation, and uploading the project to GitHub.

No team communication platform was used because there were no additional team members. GitHub was used to store and share the project source.

## 3. Development Environment

The program was compiled and tested using the Linux terminal on a Chromebook.

| Component | Observed configuration |
| --- | --- |
| Linux distribution | Debian GNU/Linux 12 (bookworm) |
| Hostname | penguin |
| Kernel | 6.6.147-09648-g97ef00d8a343 |
| Architecture | x86_64 |
| Compiler | GCC 12.2.0, Debian package 12.2.0-14+deb12u1 |
| Available logical processors | 2 |
| Total memory reported by Linux | 2.7 GiB |
| Swap | 0 B |

The assignment specifies an Ubuntu VirtualBox environment. The tests documented here were performed in Chromebook Linux running Debian 12, rather than an Ubuntu VirtualBox guest. Acceptance of this environment as an alternative has not been confirmed.

## 4. Implementation

### Command Processing

The implementation retains the Chapter 5 stages: userin, gettok, procline, and runcommand. Definitions are consolidated into smallsh.c.

The shell reads a command line, separates it into arguments, checks for built-in commands, and launches external programs using fork and execvp. A semicolon separates successive commands, while an ampersand starts a command in the background.

### Signals and Interrupt Handling

The shell uses sigaction to establish signal behavior. Ctrl+C at the shell prompt does not terminate the shell. Child processes restore default signal behavior so foreground programs respond to Ctrl+C and Ctrl+Z.

During testing, Ctrl+C terminated a foreground sleep command, and the shell remained available to execute another command. Ctrl+Z stopped a foreground sleep command and returned control to the shell.

### Process Groups and Terminal Control

Each external command is assigned its own process group using setpgid. For a foreground job, tcsetpgrp transfers terminal control to the job’s process group. The shell waits for the job to terminate or stop, then restores its own terminal control.

A pipe synchronizes initial child execution with terminal handoff. This prevents a foreground child from attempting terminal input before its process group receives the terminal.

### Job Tracking

The job table records each active job’s number, process group ID, command, and running or stopped state. The shell uses waitpid to detect termination, stopping, and continuation.

Completed background jobs are reaped at command and prompt boundaries. Their termination messages may therefore appear when the next command is entered.

### Built-in Commands

| Command | Function |
| --- | --- |
| jobs | Displays tracked jobs, their states, and process group IDs |
| bg %N | Resumes job N in the background |
| fg %N | Transfers job N to the foreground and resumes it |
| kill %N | Sends a termination signal to job N’s process group |
| exit | Exits the shell |

The kill built-in also supports positive PID targets and selected named or numeric signals.

## 5. Compilation

The program was compiled using make, which executed:

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic -O2 smallsh.c -o smallsh
```

Compilation completed without warning or error messages. Running the following command displayed the Command> prompt:

```bash
./smallsh
```

## 6. Testing and Observed Results

| Test | Observed result | Status |
| --- | --- | --- |
| Compile and launch | Program compiled and displayed Command> | Passed |
| Foreground Ctrl+C | sleep 300 terminated; the shell remained running | Passed |
| Command after interruption | echo printed “Shell survived Ctrl+C” | Passed |
| Foreground Ctrl+Z | Job 3 stopped and appeared in jobs with PGID 5057 | Passed |
| Background launch | sleep 600 & returned the prompt immediately | Passed |
| jobs | Job 6 appeared as Running with PGID 5084 | Passed |
| fg | fg %6 brought the job to the foreground; Ctrl+Z stopped it | Passed |
| bg | bg %6 resumed the stopped job in the background | Passed |
| kill | kill %6 terminated the job; jobs reported termination | Passed |
| Process-group inspection | Shell and sleep had distinct process group IDs | Passed |
| Semicolon-separated commands | echo first; echo second printed both lines in order | Passed |
| Invalid external command | An error appeared and the shell continued running | Passed |
| Invalid fg target | fg %999 reported “fg: no such job” | Passed |
| Invalid bg target | bg %999 reported “bg: no such job” | Passed |
| Invalid kill target | kill %999 reported “kill: no such job” | Passed |
| Background terminal input | cat & stopped when it attempted terminal input | Passed |
| Foreground terminal input | fg %7 resumed cat, which read and echoed hello | Passed |
| End-of-file for cat | Ctrl+D ended cat and returned the shell prompt | Passed |
| Shell exit | exit returned to the normal Linux terminal prompt | Passed |

### Process-Group Evidence

The following command inspected the shell and its background sleep process:

```text
Command> ps -o pid,ppid,pgid,tpgid,stat,args -p 5123,5124
  PID  PPID  PGID TPGID STAT COMMAND
 5123  5008  5123  5126 S    ./smallsh
 5124  5123  5124  5126 S    sleep 600
```

The shell had PID and PGID 5123. Its sleep child had PID and PGID 5124, showing that the child belonged to a separate process group. The child’s PPID of 5123 identified smallsh as its parent.

TPGID was 5126 because the foreground ps command temporarily owned the terminal while collecting this output. This observation does not independently show the terminal’s foreground group while the shell was idle.

The background sleep job was subsequently terminated using kill %1. The jobs command reported:

```text
[1] Terminated sleep 600
```

### Background Terminal-Input Evidence

The terminal interaction below demonstrated stopping and resuming a program that reads standard input:

```text
Command> cat &
[7] 5142
Command> jobs
[7] Stopped cat
[7] Stopped PGID=5142 cat
Command> fg %7
hello
hello
Command>
```

The background cat process stopped when it attempted to read from the terminal. After fg moved it to the foreground, it accepted and echoed input. Ctrl+D ended cat and returned control to the shell.

## 7. Limitations

The implementation tracks one direct child per job and supports up to 128 active jobs. It does not implement pipelines, input/output redirection, wildcard expansion, variable expansion, command substitution, comments, escape processing, or a cd built-in.

Background status updates occur at command and prompt boundaries. A background child that finishes while the shell is waiting for input can remain a zombie until the next command causes the shell to reap it.

Signals sent to a job’s process group can reach descendants that remain in that group. However, the shell tracks and waits for its direct child rather than tracking every descendant separately.

## 8. Conclusion

The terminal tests demonstrated the required jobs, bg, fg, and kill commands, foreground and background execution, stopping and resuming jobs, and Ctrl+C handling without terminating the shell. Process inspection confirmed separate process groups for the shell and its child. Additional tests showed successful command sequencing, error handling, and foreground terminal input.

These results establish successful execution in the tested Debian 12 Chromebook Linux environment. Execution in the assignment’s specified Ubuntu VirtualBox environment remains unverified.
