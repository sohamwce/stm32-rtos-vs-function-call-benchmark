# STM32 RTOS Context Switching vs Function-Call Benchmark


(CMSIS-RTOS2 on STM32G4 Nucleo)

---------------------------------------------------------------------------------------------------------------------

## 1. Problem Statement 

We want to measure how much overhead the RTOS scheduler adds compared to running the same work without the RTOS.

The requirement (from the question’s frame):

Two tasks of equal priority do 1 microsecond of work.
They switch continuously for 1 second.
Measure how many iterations can be completed in 1 second.
Higher count ⇒ less time wasted in context switching.

So effectively:

Baseline: Simulate task switching without RTOS → using normal function calls (cooperative).

RTOS test: Use two real RTOS threads that repeatedly do 1 µs of work and then yield (syscall).

Compare the throughput.
--------------------------------------------------------------------------------------------------------------------

## 2. Hardware / OS Setup

Board: STM32 Nucleo-G491RE (Cortex-M4F @ 170 MHz).

Debugger: ST-Link through STM32CubeIDE.

RTOS: CMSIS-RTOS2 (FreeRTOS underneath).

Timing Source: DWT cycle counter → gives cycle-accurate microsecond timing.

Workload: Burn_1us() → deterministic 1 µs busy-wait based on CPU cycles.

-------------------------------------------------------------------------------------------------------------------

## 3. Measurement Logic Overview
Goal:
Measure how many “task cycles” are completed in exactly 1 second.
Each “task cycle” = 1 µs of work.

-------------------------------------------------------------------------------------------------------------------

## 4. Two Tests Performed

Test A — Function Call Based (Cooperative Baseline)
No RTOS running.
A simple loop runs two logical tasks sequentially:
“Task A”: run 1 µs
“Task B”: run 1 µs
Both increments share one counter: function_call_count.
This gives a clean, scheduler-free measurement of how many iterations the CPU can complete per second.

Test B — RTOS Based Switching (Syscall + Context Switch)
RTOS kernel running with two threads of equal priority.
Thread 1: run 1 µs → increment counter → osThreadYield()
Thread 2: run 1 µs → increment counter → osThreadYield()
RTOS performs a syscall-based context switch on every iteration.
This gives us the real cost of RTOS scheduler overhead.

-----------------------------------------------------------------------------------------------------------------

## 5. Why We Increment Twice in the Cooperative Test?
Because the cooperative test is meant to simulate two tasks, but without using RTOS.

So in the baseline we do:
Burn_1us();  // Logical Task A
function_call_count++;

Burn_1us();  // Logical Task B
function_call_count++;

This ensures:

Both methods do the exact same amount of work
Both methods do 2 task units per switching cycle
If we only incremented once, the cooperative method would artificially look faster.
Thus, incrementing twice keeps the comparison fair and symmetric.

------------------------------------------------------------------------------------------------------------------

## 6. What Additional Code We Added
6.1 DWT Initialization

We added:

DWT_Init() — to enable cycle counter

Burn_1us() — 1 µs workload calibrated to CPU clock

6.2 Cooperative Baseline Function

run_function_call_test() — performs sequential task simulation for 1 second.

6.3 RTOS Worker Tasks

Both worker tasks:

Do 1 µs of work

Increment rtos_call_count

Yield to scheduler with osThreadYield()

Exit automatically when 1-second window ends (checked via DWT cycles)

6.4 Monitor Task

Starts the RTOS test for exactly 1 second

Computes the end time

Terminates workers safely

Leaves final values visible to debugger

------------------------------------------------------------------------------------------------------------------

## 7. Live Expressions (Debugger Screenshot Placeholder)


<img width="1296" height="210" alt="Screenshot from 2025-12-07 22-30-51" src="https://github.com/user-attachments/assets/e2cd4ada-ea77-4555-b2fa-7af9005cec82" />

 
 rtos_call_count = 372900
 
 function call count = 751334

Where:

function_call_count > rtos_call_count

The difference = RTOS scheduler overhead

----------------------------------------------------------------------------------------------------------------

## 8. Observed Outcome
As expected:

The function-call baseline completes the highest iteration count
The RTOS-based test completes fewer cycles in the same 1 second
The difference between the two directly represents the context switching overhead of CMSIS-RTOS2 on STM32G4.
The RTOS test costs more cycles because:
It executes an SVC instruction
It triggers PendSV
Scheduler selects next task
Registers are saved/restored on every context switch
All of this adds real CPU overhead.

-------------------------------------------------------------------------------------------------------------------

## 9. Conclusion (Short & Simple)

This project measures how much time the RTOS scheduler consumes by comparing a pure function-call approach 
with a real context-switching approach using two equal-priority tasks. 
The cooperative method gives the maximum possible throughput, and 
the RTOS method shows reduced throughput because of syscall + context switch overhead. 
The difference quantifies the real scheduling cost on STM32.


-------------------------------------------------------------------------------------------------------------------

## 10. Repository Structure:

/Core/Src/main.c        -> Benchmark logic (modified)

DWT_Init(), Burn_1us()  -> Timing helpers (added)

run_function_call_test  -> Cooperative baseline (added)

StartTask_*             -> RTOS workers (added)

MonitorTask             -> RTOS test controller (added)

-------------------------------------------------------------------------------------------------------------------

## 11. How to Run

1.Flash program onto STM32 Nucleo.

2.Start debugger.

3.Wait for MonitorTask to finish (program stays in infinite loop).

4.Observe function_call_count and rtos_call_count in Live Expressions.

5.Compare values → lower RTOS count = higher switching overhead.
