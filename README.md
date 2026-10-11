# Process Simulation with System V IPC (C)

A Linux simulation in C where a parent process spawns and terminates child processes according to a config file, and sends them text messages through System V shared memory, synchronized with semaphores.

*Coursework for the Operating Systems course (K22), National and Kapodistrian University of Athens (2024-25).*

## How It Works
- The **parent** reads a config file of timed commands. Each command has a time step, a process label (e.g. `C1`) and an action: `S` (spawn the child), `T` (terminate it), or an `EXIT` line that terminates all remaining children.
- At every time step, after executing that step's commands, the parent picks a **random line** from the text file and sends it to a **random active child** through shared memory, then signals that child's semaphore and waits for its acknowledgement.
- Each **child** waits on its own semaphore, reads the message, prints it and counts it. When it receives a termination message, it reports how many messages it processed and exits.
- The parent prints per-child statistics: lifetime in steps and number of messages received.

## Implementation Details
- **Synchronization:** one semaphore per child plus a shared sync semaphore, all initialized to 0 and driven with small `semop`-based wait/signal helpers.
- **Process lifecycle:** children are created with `fork()` and reaped with `waitpid()`, so no zombie or orphan processes are left behind.
- **Permissions:** shared memory and semaphores are created with mode `600` instead of `666`, to restrict access to the owner.
- **Cleanup:** at the end of a normal run, all semaphores and the shared memory segment are removed (`destroy_semaphores()`, `destroy_shared_memory()`) and all heap allocations are freed, so nothing stays behind in the system.
- **Input validation:** the program checks the argument count, that `max_children` is positive and large enough for the labels used in the config file, and that the text file is not empty.
- **Build automation:** a Makefile with ready-made run targets.

## Build and Run
Tested on Linux (Ubuntu).

    make
    make run_config_3_100
    make run ARGS="<max_children> <text_file> <config_file>"
    make clean

## Third-Party Materials
The `config_*.txt` files (and `my_textfile.txt`, if applicable) were provided by the course and are not my work. All source code in `my_IPC.c` is mine.

A config file has one command per line: `<time step> <process label> <action>`, for example `0 C1 S` (spawn C1) or `4 C1 T` (terminate C1), and a final `<time step> EXIT` line.

## What I Learned
- **Synchronization between processes needs a clear protocol.** Giving each child its own semaphore, plus a sync semaphore for the parent, made the order of reads and writes on the shared memory predictable.
- **System V IPC objects outlive the program.** Shared memory and semaphores stay in the system until explicitly removed, so cleanup has to be part of the design.
- **Small details in C matter.** Sizing buffers for the longest label (including the terminating `\0`) and freeing every allocation are easy to miss and cause bugs that only show up under some inputs.
