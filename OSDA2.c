#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_PROCESSES 20
#define TIME_QUANTUM  3
#define AGING_THRESHOLD 10  // boost priority after waiting this many ticks

typedef enum { REAL_TIME=0, INTERACTIVE=1, BATCH=2 } ProcessType;

typedef struct {
    int pid;
    char name[20];
    ProcessType type;
    int arrival_time, burst_time, remaining_time;
    int priority;
    int waiting_time, turnaround_time, finish_time;
    int started, done, age;
} Process;

typedef struct { int pid, start, end; } GanttEntry;
GanttEntry gantt[1000];
int gantt_count = 0;

const char *type_str(ProcessType t) {
    return t==REAL_TIME?"RealTime":t==INTERACTIVE?"Interactive":"Batch";
}

void add_gantt(int pid, int start, int end) {
    if (gantt_count>0 && gantt[gantt_count-1].pid==pid)
        gantt[gantt_count-1].end=end;
    else { gantt[gantt_count].pid=pid; gantt[gantt_count].start=start; gantt[gantt_count].end=end; gantt_count++; }
}

void run_scheduler(Process p[], int n) {
    int time=0, completed=0, rr_index=0, rr_slice=0;
    printf("\nSimulation Log: \n");
    
    while (completed < n) {
        //aging
        for (int i=0;i<n;i++) {
            if (!p[i].done && p[i].arrival_time<=time && p[i].remaining_time>0) {
                p[i].age++;
                if (p[i].age%AGING_THRESHOLD==0 && p[i].priority>1) {
                    p[i].priority--;
                    printf("[t=%d] Aging: P%d priority -> %d\n",time,p[i].pid,p[i].priority);
                }
            }
        }
        //1. REAL-TIME: Preemptive Priority 
        int sel=-1, best=9999;
        for (int i=0;i<n;i++)
            if (!p[i].done && p[i].arrival_time<=time && p[i].type==REAL_TIME && p[i].priority<best)
                { best=p[i].priority; sel=i; }

        if (sel!=-1) {
            p[sel].age=0;
            add_gantt(p[sel].pid, time, time+1);
            p[sel].remaining_time--;
            if (p[sel].remaining_time==0) {
                p[sel].done=1;
                p[sel].finish_time=time+1;
                p[sel].turnaround_time=p[sel].finish_time - p[sel].arrival_time;
                p[sel].waiting_time=p[sel].turnaround_time - p[sel].burst_time;
                completed++;
                printf("[t=%d] P%d (%s) DONE\n", time+1, p[sel].pid, type_str(p[sel].type));
            }
            time++;
            continue;
        }

        //2. INTERACTIVE: Round Robin 
        int fi=0;
        for (int a=0;a<n;a++) {
            int idx=(rr_index+a)%n;
            if (!p[idx].done && p[idx].arrival_time<=time && p[idx].type==INTERACTIVE) {
                p[idx].age=0;
                add_gantt(p[idx].pid, time, time+1);
                p[idx].remaining_time--;
                rr_slice++;
                if (p[idx].remaining_time==0) {
                    p[idx].done=1;
                    p[idx].finish_time=time+1;
                    p[idx].turnaround_time=p[idx].finish_time - p[idx].arrival_time;
                    p[idx].waiting_time=p[idx].turnaround_time - p[idx].burst_time;
                    completed++;
                    printf("[t=%d] P%d (%s) DONE\n", time+1, p[idx].pid, type_str(p[idx].type));
                    rr_slice=0;
                    rr_index=(idx+1)%n;
                } else if (rr_slice>=TIME_QUANTUM) {
                    rr_slice=0;
                    rr_index=(idx+1)%n;
                }
                time++;
                fi=1;
                break;
            }
        }
        if (fi) continue;

        //3. BATCH: Preemptive SJF
        int sjf=-1, mb=9999;
        for (int i=0;i<n;i++)
            if (!p[i].done && p[i].arrival_time<=time && p[i].type==BATCH && p[i].remaining_time<mb)
                { mb=p[i].remaining_time; sjf=i; }

        if (sjf!=-1) {
            add_gantt(p[sjf].pid, time, time+1);
            p[sjf].remaining_time--;
            p[sjf].age=0;
            time++;
            if (p[sjf].remaining_time==0) {
                p[sjf].done=1;
                p[sjf].finish_time=time;
                p[sjf].turnaround_time=p[sjf].finish_time - p[sjf].arrival_time;
                p[sjf].waiting_time=p[sjf].turnaround_time - p[sjf].burst_time;
                completed++;
                printf("[t=%d] P%d (Batch/SRTN) DONE\n", time, p[sjf].pid);
            }
            continue;
        }

        // CPU Idle
        printf("[t=%d] CPU Idle\n", time);
        time++;
    }
}

void print_gantt() {
    printf("\nGantt Chart :\n|");
    for (int i=0;i<gantt_count;i++) printf(" P%-2d |",gantt[i].pid);
    printf("\n0");
    for (int i=0;i<gantt_count;i++) printf("%5d",gantt[i].end);
    printf("\n");
}

void print_metrics(Process p[], int n) {
    printf("\nMetrics: \n");
    printf("%-5s %-14s %-12s %-7s %-8s %-9s %-14s %-13s\n",
           "PID","Name","Type","Burst","Arrival","Priority","WaitTime","Turnaround");
    printf("--------------------------------------------------------------------------------\n");
    double tw=0,tt=0; int tb=0,fm=0;
    for (int i=0;i<n;i++) {
        printf("%-5d %-14s %-12s %-7d %-8d %-9d %-14d %-13d\n",
               p[i].pid,p[i].name,type_str(p[i].type),p[i].burst_time,
               p[i].arrival_time,p[i].priority,p[i].waiting_time,p[i].turnaround_time);
        tw+=p[i].waiting_time; tt+=p[i].turnaround_time;
        tb+=p[i].burst_time;
        if (p[i].finish_time>fm) fm=p[i].finish_time;
    }
    printf("\nAvg Waiting Time    : %.2f\n", tw/n);
    printf("Avg Turnaround Time : %.2f\n", tt/n);
    printf("CPU Utilization     : %.2f%%\n",(double)tb/fm*100);
}

int main() {
    Process procs[] = {
        {1,"RT_Sensor",  REAL_TIME,   0, 4, 4,1,0,0,0,0,0,0},
        {2,"UI_Handler", INTERACTIVE, 0, 8, 8,3,0,0,0,0,0,0},
        {3,"DataBackup", BATCH,       0,12,12,5,0,0,0,0,0,0},
        {4,"RT_Alarm",   REAL_TIME,   2, 3, 3,2,0,0,0,0,0,0},
        {5,"Browser",    INTERACTIVE, 3, 6, 6,4,0,0,0,0,0,0},
        {6,"Compile",    BATCH,       4,10,10,5,0,0,0,0,0,0},
        {7,"RT_Control", REAL_TIME,   5, 2, 2,1,0,0,0,0,0,0},
        {8,"TextEditor", INTERACTIVE, 6, 5, 5,3,0,0,0,0,0,0},
    };
    int n = sizeof(procs)/sizeof(procs[0]);

    printf("=== Intelligent CPU Scheduler for Mixed Workloads ===\n");
    printf("Strategy: RealTime->Priority | Interactive->RR(Q=%d) | Batch->SJF\n\n",TIME_QUANTUM);
    printf("%-5s %-14s %-12s %-8s %-8s %-8s\n","PID","Name","Type","Arrival","Burst","Priority");
    printf("----------------------------------------------------------\n");
    for (int i=0;i<n;i++)
        printf("%-5d %-14s %-12s %-8d %-8d %-8d\n",
               procs[i].pid,procs[i].name,type_str(procs[i].type),
               procs[i].arrival_time,procs[i].burst_time,procs[i].priority);

    Process original[MAX_PROCESSES];
    memcpy(original,procs,sizeof(Process)*n);

    run_scheduler(procs, n);
    print_gantt();
    print_metrics(procs, n);
    return 0;
}









