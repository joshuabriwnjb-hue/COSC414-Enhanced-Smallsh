# Phase 1 Enhanced UNIX Shell — Chromebook Linux instructions

This package addresses Phase 1 only. Chapter 5 is the program reference; its exercise questions are not answered. No Phase 2 benchmarks or experimental data are included.

## 1. Prepare your Chromebook

Download and extract Phase1_Smallsh_Project.zip in the Files app. Move the extracted phase1_smallsh folder into **Linux files**. Open the Linux Terminal and run:

```bash
sudo apt update
sudo apt install build-essential
cd ~/phase1_smallsh
make
./smallsh
```

If the extracted folder is nested, open it in Files and move the inner phase1_smallsh folder to Linux files. The Makefile must be in the folder where you run make. You can also compile directly:

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic -O2 smallsh.c -o smallsh
```

A successful build prints the gcc command and no warning/error messages. The shell prompt is `Command> `. Enter the following test commands there. Enter setup commands in your normal Linux terminal, before launching smallsh.

The instructor's PDF specifies Ubuntu in VirtualBox. Chromebook Linux is commonly Debian-based and is not automatically an Ubuntu VirtualBox VM. This code uses POSIX/Linux interfaces and should run in either environment, but Chromebook testing does not establish compliance with the required VM setup. Confirm the environment arrangement with your instructor; do not describe your Chromebook as VirtualBox unless that is actually your setup.

## 2. Record your actual setup

Run these in your normal Linux terminal:

```bash
cat /etc/os-release
uname -a
gcc --version
getconf _NPROCESSORS_ONLN
free -h
```

📸 SCREENSHOT REQUIRED: Capture your OS information, compiler version, and successful compilation. Use your actual output to complete the technical document.

## 3. Test foreground commands and Ctrl+C

Start a fresh shell so job IDs match the examples:

```bash
./smallsh
```

At Command>, enter:

```text
echo Shell is running
sleep 300
```

While sleep is running, press **Ctrl+C**. Then enter:

```text
echo Shell survived Ctrl+C
```

Expected: sleep ends, the Command> prompt returns, and echo succeeds. Press Ctrl+C again at an empty prompt: the shell should print a fresh prompt and remain usable.

📸 SCREENSHOT REQUIRED: Show the interrupted sleep and successful echo.

## 4. Test stopped jobs, bg, and fg

Continue in the same shell:

```text
sleep 300
```

Press **Ctrl+Z**, then enter:

```text
jobs
```

Expected: a Stopped job with a job ID and PGID. Use the displayed ID in place of N below. Job IDs count ALL launched external commands, including completed foreground commands; do not assume the ID is 1.

📸 SCREENSHOT REQUIRED: Show the stopped job and jobs output.

```text
bg %N
jobs
```

Expected: the same job is Running, with the same PGID.

📸 SCREENSHOT REQUIRED: Show bg and the Running job.

```text
fg %N
```

Expected: no new prompt while sleep owns the terminal. Press **Ctrl+C**. Then enter `jobs`. The terminated job should no longer be listed.

📸 SCREENSHOT REQUIRED: Show fg, Ctrl+C, and the returned shell prompt.

## 5. Test a background job and kill

```text
sleep 300 &
jobs
```

Expected: the prompt returns immediately; jobs lists Running and a PGID.

📸 SCREENSHOT REQUIRED: Capture that output. Use its job number as N:

```text
kill %N
sleep 1
jobs
```

Expected: termination is reported and the killed job is absent. The short foreground sleep allows the termination to complete before checking.

📸 SCREENSHOT REQUIRED: Show kill and the final jobs output.

Optional signal checks on another sleep job:

```text
sleep 300 &
kill -STOP %N
jobs
bg %N
jobs
kill -KILL %N
sleep 1
jobs
```

Replace N with this new job's number. If the first jobs runs before STOP takes effect, enter jobs again.

## 6. Verify process groups and terminal access

Run `sleep 300 &` and `jobs`. Record its PGID. Open a second Chromebook Linux terminal tab and run:

```bash
ps -eo pid,ppid,pgid,tpgid,stat,args --forest
```

Locate ./smallsh and its sleep child. Expected: the child has its own PGID, equal to its PID, and different from the shell's PGID. While the child is backgrounded, TPGID refers to the shell's foreground group. Run `fg %N` in smallsh and inspect again from the second tab: TPGID should now match the sleep group. Finish with Ctrl+C in smallsh.

📸 SCREENSHOT REQUIRED: Capture the process/group evidence; explain YOUR actual IDs in the report.

Additional terminal-input check:

```text
cat &
```

After a moment enter `jobs`. Expected: cat stops when it attempts background terminal input (SIGTTIN). Enter `fg %N`, type a short line and Enter; cat echoes it. Press Ctrl+D on an empty line to finish cat.

## 7. Basic parsing and error checks

```text
echo first; echo second
no_such_smallsh_command
fg %999
bg %999
kill %999
```

Expected: both echo commands execute in order; missing commands/jobs produce errors without terminating the shell. Finish with `exit`, or Ctrl+D at an empty shell prompt.

📸 SCREENSHOT REQUIRED: Capture representative successful and invalid commands.

## Commands supported

| Command | Meaning |
| --- | --- |
| jobs | List active tracked jobs with state and PGID |
| bg %N | Resume job N in the background |
| fg %N | Bring job N to the foreground and resume it |
| bg / fg | Use the active job with the greatest job ID |
| kill %N | Send SIGTERM to every process in job N's group |
| kill PID | Send SIGTERM to one positive PID |
| kill -STOP %N | Stop the job's process group |
| kill -CONT %N | Continue the job's process group |
| kill -INT %N | Send SIGINT to the job's process group |
| kill -KILL %N | Force termination of the job's process group |
| exit | Leave smallsh; remaining tracked groups receive SIGHUP and SIGCONT |

Positive numeric signals are also accepted. Invalid signals are rejected by kill(2). Built-ins run inside the shell; an & after a built-in does not create a child.

## Scope and limitations

The implementation preserves Chapter 5's named command-processing stages, foreground/background execution, execvp PATH lookup, and semicolon-separated commands. It consolidates definitions into one smallsh.c instead of requiring smallsh.h. Quotes allow arguments containing spaces or delimiters. This is an enhanced implementation informed by the scanned reference, not a byte-for-byte transcription of its original source.

One direct child is tracked per job. Pipelines, redirection, wildcard and variable expansion, comments, escape processing, cd, and command substitution are outside this implementation. A direct child may spawn descendants in its process group, so group signals reach those descendants, but the shell only waits for and tracks its direct child. Completed background jobs are reaped before the next prompt or command, so a finished child can remain a zombie while the shell is idle awaiting input. The next command reaps it. Notifications are therefore not instantaneous at an idle prompt.

## Submit

Include smallsh.c, the Makefile if desired, the completed technical document, and actual Chromebook/VM screenshots. Fill every bracketed field in TECHNICAL_DOCUMENT.md and replace Pending test results with observations. Follow the course's required upload format. The assignment also requires individual CURE assessments; complete any surveys assigned by the instructor.
