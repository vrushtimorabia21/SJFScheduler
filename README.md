SJFScheduler

A CPU scheduling simulator for Non-Preemptive SJF (Shortest Job Next) and Preemptive SJF (Shortest Remaining Time First, SRTF), written in C with an optional Java GUI. Built for the Operating Systems course (SLA) at Shah & Anchor Kutchhi Engineering College.

Authors: Vrushti Morabia (33/125BTCM1117) and Jinisha Rathod (48/125BTCM1035)

Features
Runs Non-Preemptive SJF, Preemptive SRTF, or both on the same input for a fair comparison
Prints the Gantt chart, including CPU idle gaps
Reports per-process Completion (CT), Turnaround (TAT), Waiting (WT) and Response (RT) time
Reports average WT, TAT and RT, context switches, throughput and CPU utilization
Side-by-side comparison of the two algorithms
Input validation: 1–20 processes, arrival time 0–1000, burst time 1–1000, non-numeric input rejected
Ties are broken by earlier arrival, then by lower process ID
Files
File	Description
sjf_scheduling.c	Console program: SJF and SRTF with comparison
SJFSchedulerGUI.java	Java Swing GUI version of the same simulator
sjf_aging.c	Extension: Aging-based SRTF compared with traditional SRTF (reduces starvation of long jobs)
Formulas
Metric	Formula
Turnaround time (TAT)	Completion time − Arrival time
Waiting time (WT)	Turnaround time − Burst time
Response time (RT)	First CPU start − Arrival time
Throughput	Processes completed ÷ total elapsed time
CPU utilization	Busy CPU time ÷ total elapsed time × 100

A context switch is counted when the CPU passes directly from one process to a different one.

Build and run
C program
bash
gcc -Wall -Wextra -o sjf_scheduling sjf_scheduling.c
./sjf_scheduling            # Windows: sjf_scheduling.exe

The program asks for the number of processes, then the arrival and burst time of each, then the algorithm (1 = Non-Preemptive SJF, 2 = Preemptive SRTF, 3 = Both).

Aging SRTF extension
bash
gcc sjf_aging.c -o sjf_aging
./sjf_aging

It asks for the processes and an aging factor A (0 or more). It runs traditional SRTF (A = 0) and Aging SRTF on the same input. The aged key is remaining time − A × waiting time; the process with the smallest key runs.

Java GUI
bash
javac SJFSchedulerGUI.java
java SJFSchedulerGUI

Requires a JDK (Java 8 or newer). No external libraries are needed. Enter the processes in the table, choose an algorithm and click Run Simulation. Load Example fills in the sample below.

Example

Input:

Process	Arrival (ms)	Burst (ms)
P1	0	7
P2	2	4
P3	4	1
P4	5	4

Results:

Metric	Non-Preemptive SJF	Preemptive SRTF
Gantt chart	P1 (0–7), P3 (7–8), P2 (8–12), P4 (12–16)	P1 (0–2), P2 (2–4), P3 (4–5), P2 (5–7), P4 (7–11), P1 (11–16)
Average waiting time	4.00 ms	3.00 ms
Average turnaround time	8.00 ms	7.00 ms
Average response time	4.00 ms	0.50 ms
Context switches	3	5
Throughput	0.250 processes/ms	0.250 processes/ms
CPU utilization	100%	100%

SRTF gives lower waiting and turnaround times, at the cost of more context switches.

Assumptions and limitations
Single CPU core and integer time units (ms)
CPU burst times are known in advance
Context-switch time is not simulated
SJF and SRTF can starve long processes if short jobs keep arriving (the aging extension addresses this)
This is an educational simulation, not a measurement on a real operating system
