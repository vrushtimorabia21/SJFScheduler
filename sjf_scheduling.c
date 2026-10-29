/*
 * Operating System - Self-Learning Activity 2 (Review 2)
 * Case Study : Preemptive and Non-Preemptive SJF Process Scheduling
 * Members    : Vrushti Morabia (33/125BTCM1117), Jinsiha Rathod (48/125BTCM1035)
 *
 * Algorithms : 1. Non-Preemptive SJF (Shortest Job Next)
 *              2. Preemptive SJF     (Shortest Remaining Time First, SRTF)
 *
 * Compile : gcc -Wall -Wextra -o sjf_scheduling sjf_scheduling.c
 * Run     : ./sjf_scheduling            (Windows: sjf_scheduling.exe)
 *
 * Assumptions: single CPU core, integer time units (ms), burst times known.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#define MAX_PROCESSES 20
#define MAX_TIME_VALUE 1000                 /* upper limit for arrival/burst */
#define MAX_SEGMENTS (MAX_PROCESSES * MAX_TIME_VALUE + MAX_TIME_VALUE + 10)
#define IDLE_PID 0                          /* pid used for CPU-idle segments */

/* ---------- Data structures ---------- */

typedef struct {
    int pid;                                /* process id (1..n)             */
    int arrival;                            /* arrival time                  */
    int burst;                              /* total CPU burst time          */
    int remaining;                          /* remaining burst (for SRTF)    */
    int completion;                         /* completion time (CT)          */
    int turnaround;                         /* TAT = CT - AT                 */
    int waiting;                            /* WT  = TAT - BT                */
    int response;                           /* RT  = first start - AT        */
    int done;                               /* 1 when finished               */
} Process;

typedef struct {
    int pid;                                /* IDLE_PID for idle time        */
    int start;
    int end;
} Segment;                                  /* one block of the Gantt chart  */

typedef struct {
    Segment seg[MAX_SEGMENTS];
    int count;
} Gantt;

typedef struct {
    double avgWT, avgTAT, avgRT;
    double throughput;                      /* processes per ms              */
    double utilization;                     /* percent                       */
    int contextSwitches;
    int totalTime;
} Metrics;

/* ---------- Input helpers (validation) ---------- */

/* Reads one integer in [lo, hi]. Re-prompts on bad input. Returns 0 on EOF. */
static int readInt(const char *prompt, int lo, int hi, int *out)
{
    char line[128], *end;
    long v;

    for (;;) {
        printf("%s", prompt);
        if (fgets(line, sizeof line, stdin) == NULL)
            return 0;                       /* EOF: stop asking              */
        if (strchr(line, '\n') == NULL && !feof(stdin)) {
            int c;                          /* discard an over-long line     */
            while ((c = getchar()) != '\n' && c != EOF) { }
            printf("  Invalid input: line too long.\n");
            continue;
        }
        errno = 0;
        v = strtol(line, &end, 10);
        while (*end == ' ' || *end == '\t' || *end == '\r' || *end == '\n')
            end++;
        if (end == line || *end != '\0' || errno == ERANGE) {
            printf("  Invalid input: please enter a whole number.\n");
            continue;
        }
        if (v < lo || v > hi) {
            printf("  Invalid input: value must be between %d and %d.\n", lo, hi);
            continue;
        }
        *out = (int)v;
        return 1;
    }
}

/* ---------- Gantt chart helpers ---------- */

/* Appends [start,end) for pid, merging with the previous block if same pid. */
static void addSegment(Gantt *g, int pid, int start, int end)
{
    if (end <= start)
        return;
    if (g->count > 0 && g->seg[g->count - 1].pid == pid &&
        g->seg[g->count - 1].end == start) {
        g->seg[g->count - 1].end = end;
        return;
    }
    if (g->count < MAX_SEGMENTS) {
        g->seg[g->count].pid = pid;
        g->seg[g->count].start = start;
        g->seg[g->count].end = end;
        g->count++;
    }
}

static void printGantt(const Gantt *g)
{
    int i;

    printf("Gantt chart : ");
    for (i = 0; i < g->count; i++) {
        if (g->seg[i].pid == IDLE_PID)
            printf("| IDLE (%d-%d) ", g->seg[i].start, g->seg[i].end);
        else
            printf("| P%d (%d-%d) ", g->seg[i].pid, g->seg[i].start, g->seg[i].end);
    }
    printf("|\n");
}

/* ---------- Common helpers ---------- */

/* Copies the user input into a fresh working array so each algorithm
 * starts from identical data (fair comparison). */
static void copyProcesses(Process dst[], const Process src[], int n)
{
    int i;

    for (i = 0; i < n; i++) {
        dst[i] = src[i];
        dst[i].remaining = src[i].burst;
        dst[i].completion = dst[i].turnaround = dst[i].waiting = 0;
        dst[i].response = -1;
        dst[i].done = 0;
    }
}

/* Chooses the ready process with the smallest key.
 * useRemaining = 0 -> compare burst time (SJF), 1 -> remaining time (SRTF).
 * Ties are broken by earlier arrival, then by lower pid.
 * Returns the array index or -1 if nothing has arrived yet. */
static int pickShortest(const Process p[], int n, int time, int useRemaining)
{
    int i, best = -1;

    for (i = 0; i < n; i++) {
        int key, bestKey;
        if (p[i].done || p[i].arrival > time)
            continue;
        if (best == -1) {
            best = i;
            continue;
        }
        key = useRemaining ? p[i].remaining : p[i].burst;
        bestKey = useRemaining ? p[best].remaining : p[best].burst;
        if (key < bestKey ||
            (key == bestKey && p[i].arrival < p[best].arrival))
            best = i;
    }
    return best;
}

static int nextArrivalAfter(const Process p[], int n, int time)
{
    int i, next = -1;

    for (i = 0; i < n; i++)
        if (!p[i].done && p[i].arrival > time &&
            (next == -1 || p[i].arrival < next))
            next = p[i].arrival;
    return next;
}

static void finishProcess(Process *p, int time)
{
    p->completion = time;
    p->turnaround = p->completion - p->arrival;
    p->waiting = p->turnaround - p->burst;
    p->done = 1;
}

/* Context switch = CPU handed from one process directly to a different one. */
static int countContextSwitches(const Gantt *g)
{
    int i, last = IDLE_PID, switches = 0;

    for (i = 0; i < g->count; i++) {
        if (g->seg[i].pid == IDLE_PID)
            continue;
        if (last != IDLE_PID && g->seg[i].pid != last)
            switches++;
        last = g->seg[i].pid;
    }
    return switches;
}

static Metrics computeMetrics(const Process p[], int n, const Gantt *g)
{
    Metrics m;
    int i, busy = 0;
    double wt = 0, tat = 0, rt = 0;

    memset(&m, 0, sizeof m);
    for (i = 0; i < n; i++) {
        wt += p[i].waiting;
        tat += p[i].turnaround;
        rt += p[i].response;
        if (p[i].completion > m.totalTime)
            m.totalTime = p[i].completion;
    }
    for (i = 0; i < g->count; i++)
        if (g->seg[i].pid != IDLE_PID)
            busy += g->seg[i].end - g->seg[i].start;

    m.avgWT = wt / n;
    m.avgTAT = tat / n;
    m.avgRT = rt / n;
    m.throughput = (double)n / m.totalTime;
    m.utilization = 100.0 * busy / m.totalTime;
    m.contextSwitches = countContextSwitches(g);
    return m;
}

/* ---------- Output ---------- */

static void printResults(const char *title, const Process p[], int n,
                         const Gantt *g, const Metrics *m)
{
    int i;

    printf("\n==================== %s ====================\n", title);
    printGantt(g);
    printf("\n%-5s %-4s %-4s %-4s %-5s %-4s %-4s\n",
           "PID", "AT", "BT", "CT", "TAT", "WT", "RT");
    for (i = 0; i < n; i++)
        printf("P%-4d %-4d %-4d %-4d %-5d %-4d %-4d\n", p[i].pid, p[i].arrival,
               p[i].burst, p[i].completion, p[i].turnaround, p[i].waiting,
               p[i].response);
    printf("\nAverage waiting time    : %.2f ms\n", m->avgWT);
    printf("Average turnaround time : %.2f ms\n", m->avgTAT);
    printf("Average response time   : %.2f ms\n", m->avgRT);
    printf("Context switches        : %d\n", m->contextSwitches);
    printf("Throughput              : %.3f processes/ms (%d in %d ms)\n",
           m->throughput, n, m->totalTime);
    printf("CPU utilization         : %.2f %%\n", m->utilization);
}

static void printComparison(const Metrics *a, const Metrics *b)
{
    printf("\n==================== COMPARISON ====================\n");
    printf("%-24s %-16s %-16s\n", "Metric", "Non-Preemptive", "Preemptive SRTF");
    printf("%-24s %-16.2f %-16.2f\n", "Avg waiting time (ms)", a->avgWT, b->avgWT);
    printf("%-24s %-16.2f %-16.2f\n", "Avg turnaround (ms)", a->avgTAT, b->avgTAT);
    printf("%-24s %-16.2f %-16.2f\n", "Avg response (ms)", a->avgRT, b->avgRT);
    printf("%-24s %-16d %-16d\n", "Context switches", a->contextSwitches,
           b->contextSwitches);
    printf("%-24s %-16.3f %-16.3f\n", "Throughput (proc/ms)", a->throughput,
           b->throughput);
    printf("%-24s %-16.2f %-16.2f\n", "CPU utilization (%)", a->utilization,
           b->utilization);
    if (a->avgWT < b->avgWT)
        printf("\nLower average waiting time: Non-Preemptive SJF\n");
    else if (b->avgWT < a->avgWT)
        printf("\nLower average waiting time: Preemptive SRTF\n");
    else
        printf("\nBoth algorithms give the same average waiting time.\n");
    if (a->contextSwitches != b->contextSwitches)
        printf("Fewer context switches    : %s\n",
               a->contextSwitches < b->contextSwitches ? "Non-Preemptive SJF"
                                                       : "Preemptive SRTF");
}

/* ---------- Algorithm 1: Non-Preemptive SJF ---------- */

static Metrics nonPreemptiveSJF(const Process input[], int n, int show)
{
    Process p[MAX_PROCESSES];
    Gantt *g = malloc(sizeof *g);
    Metrics m;
    int time = 0, completed = 0;

    if (g == NULL) { fprintf(stderr, "Out of memory.\n"); exit(1); }
    g->count = 0;
    copyProcesses(p, input, n);

    while (completed < n) {
        int best = pickShortest(p, n, time, 0);
        if (best == -1) {                   /* CPU idle until next arrival   */
            int next = nextArrivalAfter(p, n, time);
            addSegment(g, IDLE_PID, time, next);
            time = next;
            continue;
        }
        p[best].response = time - p[best].arrival;
        addSegment(g, p[best].pid, time, time + p[best].burst);
        time += p[best].burst;              /* runs to completion            */
        finishProcess(&p[best], time);
        completed++;
    }
    m = computeMetrics(p, n, g);
    if (show)
        printResults("NON-PREEMPTIVE SJF", p, n, g, &m);
    free(g);
    return m;
}

/* ---------- Algorithm 2: Preemptive SJF (SRTF) ---------- */

static Metrics preemptiveSRTF(const Process input[], int n, int show)
{
    Process p[MAX_PROCESSES];
    Gantt *g = malloc(sizeof *g);
    Metrics m;
    int time = 0, completed = 0;

    if (g == NULL) { fprintf(stderr, "Out of memory.\n"); exit(1); }
    g->count = 0;
    copyProcesses(p, input, n);

    while (completed < n) {
        int best = pickShortest(p, n, time, 1);
        if (best == -1) {
            int next = nextArrivalAfter(p, n, time);
            addSegment(g, IDLE_PID, time, next);
            time = next;
            continue;
        }
        if (p[best].response == -1)
            p[best].response = time - p[best].arrival;
        /* run for 1 time unit, then re-evaluate (a new arrival may preempt) */
        addSegment(g, p[best].pid, time, time + 1);
        p[best].remaining--;
        time++;
        if (p[best].remaining == 0) {
            finishProcess(&p[best], time);
            completed++;
        }
    }
    m = computeMetrics(p, n, g);
    if (show)
        printResults("PREEMPTIVE SJF (SRTF)", p, n, g, &m);
    free(g);
    return m;
}

/* ---------- Menu and main ---------- */

static int readProcesses(Process p[], int *n)
{
    char prompt[80];
    int i;

    if (!readInt("Enter number of processes (1-20): ", 1, MAX_PROCESSES, n))
        return 0;
    for (i = 0; i < *n; i++) {
        p[i].pid = i + 1;
        sprintf(prompt, "P%d arrival time (0-%d): ", i + 1, MAX_TIME_VALUE);
        if (!readInt(prompt, 0, MAX_TIME_VALUE, &p[i].arrival))
            return 0;
        sprintf(prompt, "P%d burst time   (1-%d): ", i + 1, MAX_TIME_VALUE);
        if (!readInt(prompt, 1, MAX_TIME_VALUE, &p[i].burst))
            return 0;
    }
    return 1;
}

int main(void)
{
    Process p[MAX_PROCESSES];
    Metrics a, b;
    int n = 0, choice = 0;

    printf("=== CPU Scheduling Simulator: SJF and SRTF ===\n");
    if (!readProcesses(p, &n)) {
        fprintf(stderr, "Input ended before all data was entered.\n");
        return 1;
    }

    printf("\n1. Non-Preemptive SJF\n2. Preemptive SJF (SRTF)\n3. Both (compare)\n");
    if (!readInt("Select algorithm (1-3): ", 1, 3, &choice)) {
        fprintf(stderr, "No algorithm selected.\n");
        return 1;
    }

    if (choice == 1) {
        nonPreemptiveSJF(p, n, 1);
    } else if (choice == 2) {
        preemptiveSRTF(p, n, 1);
    } else {
        a = nonPreemptiveSJF(p, n, 1);
        b = preemptiveSRTF(p, n, 1);
        printComparison(&a, &b);
    }
    return 0;
}
