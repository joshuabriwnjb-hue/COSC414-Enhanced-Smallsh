#define _POSIX_C_SOURCE 200809L
/* Phase 1: enhanced smallsh. Based on the Chapter 5 command-processing
 * structure: userin -> gettok -> procline -> runcommand.
 * One process per job; no pipelines, expansion, or redirection. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <limits.h>
#include <termios.h>
#include <sys/wait.h>

#define MAXBUF 4096
#define MAXARG 512
#define MAXJOBS 128
enum { EOL, ARG, AMPERSAND, SEMICOLON, BAD };
typedef struct {
    int id, stopped, has_modes;
    pid_t pgid;
    char command[MAXBUF];
    struct termios modes;
} Job;
static Job jobs[MAXJOBS];
static int next_id = 1, interactive;
static pid_t shell_pgid;
static struct termios shell_modes;
static volatile sig_atomic_t interrupted;
static char inpbuf[MAXBUF], tokbuf[2 * MAXBUF], *ptr, *tok;

static void on_interrupt(int sig) {
    (void)sig;
    interrupted = 1;
    /* write is async-signal-safe; no stdio or table access in handlers. */
    if (interactive) { ssize_t ignored = write(STDOUT_FILENO, "\n", 1); (void)ignored; }
}
static void disposition(int sig, void (*handler)(int)) {
    struct sigaction sa;
    memset(&sa, 0, sizeof sa);
    sa.sa_handler = handler;
    sigemptyset(&sa.sa_mask);
    if (sigaction(sig, &sa, NULL) == -1) { perror("sigaction"); exit(1); }
}
static void terminal_owner(pid_t pgid) {
    if (!interactive) return;
    int rc;
    do { rc = tcsetpgrp(STDIN_FILENO, pgid); } while (rc == -1 && errno == EINTR);
    if (rc == -1) perror("tcsetpgrp");
}
static void initialize(void) {
    interactive = isatty(STDIN_FILENO);
    if (interactive) {
        disposition(SIGTTIN, SIG_DFL);
        while (tcgetpgrp(STDIN_FILENO) != getpgrp())
            kill(-getpgrp(), SIGTTIN);
        shell_pgid = getpid();
        if (setpgid(0, shell_pgid) == -1 && getpgrp() != shell_pgid) {
            perror("setpgid shell"); exit(1);
        }
        disposition(SIGTTOU, SIG_IGN);
        terminal_owner(shell_pgid);
        if (tcgetattr(STDIN_FILENO, &shell_modes) == -1) { perror("tcgetattr"); exit(1); }
    } else shell_pgid = getpgrp();
    disposition(SIGINT, on_interrupt);
    disposition(SIGQUIT, SIG_IGN);
    disposition(SIGTSTP, SIG_IGN);
    disposition(SIGTTIN, SIG_IGN);
    disposition(SIGTTOU, SIG_IGN);
    /* Reap explicitly with waitpid, never discard exit status with SIG_IGN. */
    disposition(SIGCHLD, SIG_DFL);
}
static void update_job(Job *j, int status, int notify) {
    if (WIFSTOPPED(status)) {
        j->stopped = 1;
        if (notify) printf("[%d] Stopped %s\n", j->id, j->command);
    } else if (WIFCONTINUED(status)) j->stopped = 0;
    else if (WIFEXITED(status) || WIFSIGNALED(status)) {
        if (notify) printf("[%d] %s %s\n", j->id,
                            WIFSIGNALED(status) ? "Terminated" : "Done", j->command);
        memset(j, 0, sizeof *j);
    }
}
static void reap_jobs(void) {
    for (int i = 0; i < MAXJOBS; ++i) {
        Job *j = &jobs[i];
        while (j->id) {
            int status;
            pid_t p = waitpid(j->pgid, &status, WNOHANG | WUNTRACED | WCONTINUED);
            if (p > 0) update_job(j, status, 1);
            else if (p == -1 && errno == EINTR) continue;
            else break;
        }
    }
}
static void wait_foreground(Job *j, int resume) {
    terminal_owner(j->pgid);
    if (interactive && resume && j->has_modes)
        tcsetattr(STDIN_FILENO, TCSADRAIN, &j->modes);
    if (resume) {
        if (kill(-j->pgid, SIGCONT) == -1) perror("fg: SIGCONT");
        else j->stopped = 0;
    }
    int status;
    pid_t p;
    do { p = waitpid(j->pgid, &status, WUNTRACED); } while (p == -1 && errno == EINTR);
    if (p > 0) {
        if (interactive && WIFSTOPPED(status))
            j->has_modes = tcgetattr(STDIN_FILENO, &j->modes) == 0;
        update_job(j, status, WIFSTOPPED(status) || WIFSIGNALED(status));
    } else if (errno == ECHILD) memset(j, 0, sizeof *j);
    else perror("waitpid");
    terminal_owner(shell_pgid);
    if (interactive) tcsetattr(STDIN_FILENO, TCSADRAIN, &shell_modes);
}
static long positive_number(const char *s) {
    char *end;
    errno = 0;
    long n = strtol(s, &end, 10);
    return errno || !*s || *end || n <= 0 || n > INT_MAX ? -1 : n;
}
static Job *find_job(const char *spec) {
    Job *latest = NULL;
    long id = spec ? positive_number(spec[0] == '%' ? spec + 1 : spec) : -1;
    for (int i = 0; i < MAXJOBS; ++i) {
        if (!jobs[i].id) continue;
        if (spec && jobs[i].id == id) return &jobs[i];
        if (!latest || jobs[i].id > latest->id) latest = &jobs[i];
    }
    return spec ? NULL : latest;
}
static int builtin(char **argv) {
    if (!strcmp(argv[0], "jobs")) {
        if (argv[1]) { fprintf(stderr, "usage: jobs\n"); return 1; }
        for (int i = 0; i < MAXJOBS; ++i) if (jobs[i].id)
            printf("[%d] %-7s PGID=%ld %s\n", jobs[i].id,
                   jobs[i].stopped ? "Stopped" : "Running", (long)jobs[i].pgid, jobs[i].command);
    } else if (!strcmp(argv[0], "bg") || !strcmp(argv[0], "fg")) {
        if (argv[1] && argv[2]) { fprintf(stderr, "usage: %s [%%job]\n", argv[0]); return 1; }
        Job *j = find_job(argv[1]);
        if (!j) { fprintf(stderr, "%s: no such job\n", argv[0]); return 1; }
        if (argv[0][0] == 'f') wait_foreground(j, 1);
        else if (kill(-j->pgid, SIGCONT) == -1) perror("bg");
        else { j->stopped = 0; printf("[%d] Running %s\n", j->id, j->command); }
    } else if (!strcmp(argv[0], "kill")) {
        int sig = SIGTERM, arg = 1;
        if (argv[arg] && argv[arg][0] == '-') {
            const char *s = argv[arg++] + 1;
            if (!strcmp(s, "TERM")) sig = SIGTERM;
            else if (!strcmp(s, "KILL")) sig = SIGKILL;
            else if (!strcmp(s, "STOP")) sig = SIGSTOP;
            else if (!strcmp(s, "CONT")) sig = SIGCONT;
            else if (!strcmp(s, "INT")) sig = SIGINT;
            else {
                long n = positive_number(s);
                if (n < 0) { fprintf(stderr, "kill: invalid signal\n"); return 1; }
                sig = (int)n;
            }
        }
        if (!argv[arg] || argv[arg + 1]) {
            fprintf(stderr, "usage: kill [-TERM|-KILL|-STOP|-CONT|-INT|number] %%job|PID\n"); return 1;
        }
        pid_t target;
        Job *j = NULL;
        if (argv[arg][0] == '%') {
            j = find_job(argv[arg]);
            if (!j) { fprintf(stderr, "kill: no such job\n"); return 1; }
            target = -j->pgid;
        } else {
            long n = positive_number(argv[arg]);
            if (n < 0) { fprintf(stderr, "kill: invalid PID\n"); return 1; }
            target = (pid_t)n;
        }
        if (kill(target, sig) == -1) perror("kill");
        /* A stopped process must continue to act on a pending SIGTERM. */
        else if (j && j->stopped && sig == SIGTERM) kill(target, SIGCONT);
    } else if (!strcmp(argv[0], "exit")) {
        if (argv[1]) fprintf(stderr, "usage: exit\n");
        else return 2;
    } else return 0;
    return 1;
}
static int runcommand(char **argv, int background) {
    reap_jobs();
    int b = builtin(argv);
    if (b) return b == 2;
    Job *j = NULL;
    for (int i = 0; i < MAXJOBS; ++i) if (!jobs[i].id) { j = &jobs[i]; break; }
    if (!j) { fprintf(stderr, "smallsh: job table full\n"); return 0; }
    /* Gate prevents a foreground child reading the terminal before handoff. */
    int gate[2];
    if (pipe(gate) == -1) { perror("pipe"); return 0; }
    fflush(NULL);
    pid_t pid = fork();
    if (pid == -1) { perror("fork"); close(gate[0]); close(gate[1]); return 0; }
    if (!pid) {
        close(gate[1]);
        if (setpgid(0, 0) == -1) _exit(126);
        disposition(SIGINT, SIG_DFL); disposition(SIGQUIT, SIG_DFL);
        disposition(SIGTSTP, SIG_DFL); disposition(SIGTTIN, SIG_DFL);
        disposition(SIGTTOU, SIG_DFL); disposition(SIGCHLD, SIG_DFL);
        char byte;
        while (read(gate[0], &byte, 1) == -1 && errno == EINTR) {}
        close(gate[0]);
        execvp(argv[0], argv);
        perror(argv[0]); _exit(127);
    }
    close(gate[0]);
    if (setpgid(pid, pid) == -1) perror("setpgid child");
    j->id = next_id++; j->pgid = pid;
    size_t used = 0;
    for (int i = 0; argv[i]; ++i) {
        int n = snprintf(j->command + used, sizeof j->command - used, "%s%s", i ? " " : "", argv[i]);
        if (n < 0 || (size_t)n >= sizeof j->command - used) break;
        used += (size_t)n;
    }
    if (!background) terminal_owner(pid);
    close(gate[1]);
    if (background) printf("[%d] %ld\n", j->id, (long)pid);
    else wait_foreground(j, 0);
    return 0;
}
static int userin(void) {
    interrupted = 0;
    ptr = inpbuf; tok = tokbuf;
    if (interactive) { printf("Command> "); fflush(stdout); }
    errno = 0;
    if (!fgets(inpbuf, sizeof inpbuf, stdin)) {
        if (errno == EINTR || interrupted) { clearerr(stdin); return 0; }
        return -1;
    }
    if (!strchr(inpbuf, '\n') && !feof(stdin)) {
        int c; while ((c = getchar()) != '\n' && c != EOF) {}
        fprintf(stderr, "smallsh: input line too long\n"); return 0;
    }
    return 1;
}
static int gettok(char **out) {
    while (*ptr == ' ' || *ptr == '\t') ++ptr;
    if (!*ptr || *ptr == '\n') return EOL;
    if (*ptr == '&') { ++ptr; return AMPERSAND; }
    if (*ptr == ';') { ++ptr; return SEMICOLON; }
    *out = tok;
    /* Basic quoted arguments for commands such as sh -c 'sleep 10'. */
    char quote = 0;
    while (*ptr) {
        char c = *ptr;
        if (!quote && (c == ' ' || c == '\t' || c == '\n' || c == '&' || c == ';')) break;
        ++ptr;
        if (c == '\'' || c == '"') {
            if (!quote) { quote = c; continue; }
            if (quote == c) { quote = 0; continue; }
        }
        *tok++ = c;
    }
    *tok++ = '\0';
    if (quote) { fprintf(stderr, "smallsh: unmatched quote\n"); return BAD; }
    return ARG;
}
static int procline(void) {
    char *argv[MAXARG + 1], *word;
    int argc = 0, t;
    for (;;) {
        t = gettok(&word);
        if (t == BAD) return 0;
        if (t == ARG) {
            if (argc == MAXARG) { fprintf(stderr, "smallsh: too many arguments\n"); return 0; }
            argv[argc++] = word;
        } else {
            argv[argc] = NULL;
            if (argc && runcommand(argv, t == AMPERSAND)) return 1;
            argc = 0;
            if (t == EOL) return 0;
        }
    }
}
int main(void) {
    initialize();
    for (;;) {
        reap_jobs();
        int r = userin();
        if (r < 0 || (r > 0 && procline())) break;
    }
    /* Notify remaining groups on shell exit, as interactive shells do. */
    for (int i = 0; i < MAXJOBS; ++i) if (jobs[i].id) {
        kill(-jobs[i].pgid, SIGHUP);
        kill(-jobs[i].pgid, SIGCONT);
    }
    return 0;
}
