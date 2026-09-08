
# Mini Unix Shell

A lightweight Unix-like command-line shell written in C++17 for POSIX-compatible systems.

This project implements the core mechanisms behind a Unix shell from the ground up, including **process creation, program execution, pipelines, file-descriptor redirection, signals, process groups, terminal control, and foreground/background job management**.

The goal is not to recreate every feature of `bash` or `zsh`, but to build a practical understanding of how a Unix shell interacts with the operating system.

---

## 🎯 Project Overview

A Unix shell is more than a program that launches commands.

```markdown
When a user enters:

```bash
cat input.txt | grep hello > output.txt
````

the shell must:

1. Parse the command.
2. Identify the pipeline stages.
3. Create pipes between processes.
4. Create child processes using `fork()`.
5. Replace each child with the requested program using `execvp()`.
6. Connect standard input/output using `dup2()`.
7. Open and configure redirected files.
8. Place pipeline processes into a common process group.
9. Give the foreground process group control of the terminal.
10. Track child-process state and respond to signals.
11. Restore terminal control to the shell when the job finishes or stops.

This project implements those mechanisms directly using **C++ and POSIX system calls**.

---

## **✨ Key Highlights**

- ⚙️ **Process creation and execution** using `fork()` and `execvp()`
- 🔗 **Multi-stage pipelines** using Unix pipes
- 📂 **File descriptor redirection** using `open()` and `dup2()`
- 🚦 **Foreground and background execution**
- 🛑 **Interactive signal handling** with `SIGINT` and `SIGTSTP`
- 🔄 **Job suspension and resumption** with `Ctrl+Z`, `bg`, and `fg`
- 👥 **Process groups** for controlling complete pipelines as a single job
- 🖥️ **Terminal ownership management** using `tcsetpgrp()`
- 🧵 **Child-process state tracking** using `SIGCHLD` and `waitpid()`
- 🧩 **Quote-aware command parsing**
- ❌ **Basic syntax and error handling**

---

# **✨ Features**

## **1. Command Execution**

External commands are executed using the standard Unix process model:

```text
Shell
  │
  ├── fork()
  │
  ├── Child
  │     └── execvp()
  │            └── Requested program
  │
  └── Parent
        └── waitpid()
```

Example:

```bash
echo hello
ls
pwd
cat file.txt
```

The shell creates a child process and replaces the child’s program image with the requested executable.

---

## **2. Built-in Commands**

Some commands must be handled directly by the shell because they modify shell state or interact with the shell’s job table.

|**Command**|**Description**|
|---|---|
|`cd <directory>`|Change the shell’s working directory|
|`jobs`|Display active jobs|
|`fg <job_id>`|Bring a job to the foreground|
|`bg <job_id>`|Resume a stopped job in the background|
|`exit`|Exit the shell|

### **Example**

```bash
cd ..
pwd
```

Unlike an external program, `cd` must execute inside the shell process so that the shell’s working directory changes.

---

# **🔗 Pipelines**

The shell supports pipelines using the Unix `|` operator.

```bash
echo hello | cat
```

Multiple stages are supported:

```bash
cat input.txt | grep hello | wc -l
```

### **Pipeline Architecture**

For:

```bash
cat input.txt | grep hello | wc -l
```

the shell creates:

```text
                 PIPE 1                    PIPE 2

   ┌──────────┐            ┌──────────┐            ┌──────────┐
   │   cat    │ ──────────►│   grep   │ ──────────►│   wc     │
   │          │            │          │            │          │
   │ stdout   │            │ stdin    │            │ stdin    │
   └──────────┘            └──────────┘            └──────────┘
```

Conceptually:

```text
cat stdout
     │
     ▼
   pipe()
     │
     ▼
grep stdin
     │
     ▼
   pipe()
     │
     ▼
wc stdin
```

Each stage executes in its own process.

All processes belonging to the pipeline are placed into the same process group so the entire pipeline can be controlled as one job.

---

# **📂 File Descriptor Redirection**

The shell supports standard Unix redirection operators.

|**Operator**|**Meaning**|**File Descriptor**|
|---|---|---|
|`<`|Redirect standard input|`stdin` / `0`|
|`>`|Redirect standard output|`stdout` / `1`|
|`>>`|Append standard output|`stdout` / `1`|
|`2>`|Redirect standard error|`stderr` / `2`|

### **Input**

```bash
cat < input.txt
```

Conceptually:

```text
input.txt
    │
    ▼
  open()
    │
    ▼
  dup2()
    │
    ▼
stdin ──► cat
```

### **Output**

```bash
echo hello > output.txt
```

```text
echo
 │
 ▼
stdout
 │
 ▼
dup2()
 │
 ▼
output.txt
```

### **Append**

```bash
echo hello >> output.txt
```

The file is opened using `O_APPEND` so new output is added to the end.

### **Standard Error**

```bash
ls does_not_exist 2> error.txt
```

Only the command’s standard error is redirected.

---

# **🔀 Pipeline + Redirection**

Redirection can also be used inside individual pipeline stages.

```bash
cat < input.txt | grep hello
```

```bash
echo hello | cat > output.txt
```

```bash
ls does_not_exist 2> error.txt | cat
```

Each pipeline stage has its own redirection configuration.

The shell first establishes the pipeline file descriptors and then applies explicit redirections for the stage.

This allows explicit redirection to override a pipeline endpoint where appropriate.

---

# **⚡ Background Execution**

Commands can be executed without blocking the shell by placing `&` at the end.

```bash
sleep 30 &
```

The shell immediately returns to the prompt:

```text
myshell$ sleep 30 &
[1] Running sleep 30 &
myshell$
```

Background jobs are stored in the shell’s job table.

---

# **📋 Job Management**

The shell maintains information about active jobs.

A job contains information such as:

```text
Job
├── Job ID
├── Process Group ID
├── Process IDs
├── Command
└── Stopped / Running state
```

Example:

```bash
sleep 30 &
jobs
```

Output:

```text
[1] Running sleep 30 &
```

---

# **🛑 Interactive Job Control**

One of the most important parts of the project is basic Unix-style job control.

The shell supports:

```text
Ctrl+C    → Interrupt foreground job
Ctrl+Z    → Stop foreground job
bg        → Resume stopped job in background
fg        → Bring job to foreground
jobs      → Display tracked jobs
```

### **Example**

Start a pipeline:

```bash
sleep 30 | cat
```

Press:

```text
Ctrl+Z
```

The shell reports:

```text
[pipeline stopped]
```

Then:

```bash
jobs
```

```text
[1] Stopped sleep 30 | cat
```

Resume it:

```bash
bg 1
```

```text
[1] Running sleep 30 | cat
```

Bring it back:

```bash
fg 1
```

The terminal is returned to the foreground pipeline.

---

# **👥 Process Groups**

Unix job control operates on **process groups**, rather than treating every process independently.

For a pipeline such as:

```bash
sleep 30 | cat
```

the shell creates multiple processes:

```text
             Process Group
                  │
          ┌───────┴───────┐
          │               │
       sleep             cat
       PID A             PID B
```

Both processes belong to the same process group.

The shell can then send a signal to the entire pipeline using the process-group ID.

For example:

```text
kill(-pgid, SIGCONT)
```

resumes the complete job rather than only one process.

---

# **🖥️ Terminal Control**

Interactive shells must control which process group owns the terminal.

This project uses:

```cpp
setpgid()
tcsetpgrp()
```

The basic relationship is:

```text
                 Terminal
                    │
                    ▼
              Foreground PG
                    │
             ┌──────┴──────┐
             │             │
           process       process
```

When a foreground job starts:

```text
Shell
  │
  │ tcsetpgrp()
  ▼
Job Process Group
```

When the job finishes or stops:

```text
Job Process Group
        │
        │ tcsetpgrp()
        ▼
      Shell
```

This is what allows terminal-generated signals such as `Ctrl+C` and `Ctrl+Z` to affect the foreground job instead of the shell.

---

# **🚦 Signal Handling**

The shell uses several Unix signals as part of job control.

|**Signal**|**Purpose**|
|---|---|
|`SIGINT`|Interrupt foreground processes|
|`SIGTSTP`|Stop foreground processes|
|`SIGCHLD`|Notify the shell about child-process state changes|
|`SIGTTIN`|Prevent background processes from reading the terminal|
|`SIGTTOU`|Prevent background processes from modifying terminal state|

The shell ignores interactive job-control signals that it must handle differently from its children.

Child processes restore the default signal behavior before executing external programs.

---

# **🔄 SIGCHLD and Child-State Tracking**

The shell uses `SIGCHLD` to detect changes in child processes.

The handler sets a lightweight flag:

```text
SIGCHLD
   │
   ▼
signal handler
   │
   ▼
child_finished = 1
   │
   ▼
main shell loop
   │
   ▼
waitpid()
```

The shell then uses:

```cpp
waitpid(
    -1,
    &status,
    WNOHANG | WUNTRACED | WCONTINUED
);
```

to determine whether children have:

- exited
- been terminated by a signal
- stopped
- continued

This allows the shell to maintain the correct state for `jobs`, `fg`, and `bg`.

---

# **🧠 Command Parsing**

The shell contains a quote-aware lexer that recognizes:

```text
Arguments
'
"
|
<
>
>>
2>
&
```

Whitespace separates arguments only when the shell is not currently inside a quoted string.

For example:

```bash
echo "hello world"
```

produces a single argument:

```text
hello world
```

rather than two separate arguments.

The parser also detects unmatched quotes and malformed pipelines.

---

# **🔧 Technical Implementation**

The shell is implemented using standard C++17 together with POSIX APIs available on Unix-like systems.

### **Core APIs**

```text
fork()
execvp()
waitpid()

pipe()
dup2()

open()
close()

setpgid()
getpgrp()
tcsetpgrp()

kill()
signal()
```

### **Supporting facilities**

```text
errno
perror()
std::vector
std::string
std::stringstream
```

---

# **🏗️ Architecture**

The overall shell can be viewed as several cooperating components:

```text
                         ┌─────────────────┐
                         │   User Input    │
                         └────────┬────────┘
                                  │
                                  ▼
                         ┌─────────────────┐
                         │ Lexer / Parser  │
                         └────────┬────────┘
                                  │
                     ┌────────────┴────────────┐
                     │                         │
                     ▼                         ▼
              Built-in Command          External Command
                     │                         │
          ┌──────────┼──────────┐              │
          │          │          │              │
          ▼          ▼          ▼              ▼
         cd        jobs       fg/bg         fork()
                                               │
                              ┌────────────────┼────────────────┐
                              │                │                │
                              ▼                ▼                ▼
                           pipe()           dup2()           execvp()
                              │                │                │
                              └────────────────┴────────────────┘
                                               │
                                               ▼
                                      Process Group
                                               │
                                  ┌────────────┼────────────┐
                                  │            │            │
                                  ▼            ▼            ▼
                               SIGINT       SIGTSTP      SIGCHLD
                                  │            │            │
                                  └────────────┴────────────┘
                                               │
                                               ▼
                                        Terminal Control
                                               │
                                               ▼
                                          Job Table
```

---

# **🧪 Testing**

The shell has been tested across the major functionality implemented in the project.

## **Basic Commands**

```bash
echo hello
echo "hello world"
```

## **Pipelines**

```bash
echo hello | cat
echo hello | grep hello
echo hello | cat | cat
```

## **Redirection**

```bash
echo hello > output.txt
cat < output.txt
echo world >> output.txt
ls does_not_exist 2> error.txt
```

## **Pipeline Redirection**

```bash
cat < input.txt | grep hello
echo hello | cat > output.txt
echo hello | cat >> output.txt
ls does_not_exist 2> error.txt | cat
```

## **Background Jobs**

```bash
sleep 30 &
jobs
```

## **Interactive Job Control**

```bash
sleep 30 | cat
```

Tested with:

```text
Ctrl+Z
jobs
bg 1
jobs
fg 1
Ctrl+C
```

## **Error Handling**

Tested cases include:

- Invalid job IDs
- Malformed pipelines
- Missing redirection filenames
- Unmatched quotes
- Interrupted `waitpid()` calls

## **Compiler Validation**

The project successfully compiles using:

```bash
g++ -std=c++17 shell.cpp -o myshell
```

and with additional warnings:

```bash
g++ -std=c++17 -Wall -Wextra -pedantic shell.cpp -o myshell
```

---

# **💻 Building & Running**

## **Prerequisites**

- **Target Platform:** Unix-like / POSIX systems (macOS and Linux)
- **Compiler:** Any C++17-compatible compiler (Apple Clang, GCC, or Clang)
- **Language:** C++17
- **APIs:** POSIX system calls and Unix terminal-control APIs

No external libraries or third-party dependencies are required.

### **Platform Support**

| Platform | Support |
|---|---|
| **macOS** | ✅ Supported |
| **Linux** | ✅ Supported |
| **Windows (Native)** | ❌ Not supported |
| **Windows via WSL** | ✅ Supported |

> **Note:** Native Windows is not currently supported because the shell relies on POSIX APIs such as `fork()`, `execvp()`, `pipe()`, `dup2()`, `waitpid()`, `setpgid()`, and `tcsetpgrp()`. Windows users can run the project through **WSL (Windows Subsystem for Linux)**.
## **Clone**

```bash
git clone <YOUR_REPOSITORY_URL>
cd mini-unix-shell
```

## **Compile**

```bash
cd src
g++ -std=c++17 shell.cpp -o myshell
```

Optional warning-enabled build:

```bash
g++ -std=c++17 -Wall -Wextra -pedantic shell.cpp -o myshell
```

## **Run**

```bash
./myshell
```

---

# **📂 Project Structure**

```text
mini-unix-shell/
│
├── src/
│   └── shell.cpp
│
├── README.md
│
└── .gitignore
```

The implementation is intentionally kept in a single source file.

This makes the system-level mechanisms easier to trace while learning how processes, pipes, signals, and terminal control interact.

---

# **📊 System-Level Concepts Demonstrated**

This project brings together several fundamental operating-system concepts.

### **Process Management**

```text
fork()
execvp()
waitpid()
```

### **Inter-Process Communication**

```text
pipe()
```

### **File Descriptors**

```text
open()
dup2()
close()
```

### **Signal Handling**

```text
SIGINT
SIGTSTP
SIGCHLD
SIGTTIN
SIGTTOU
```

### **Process Groups**

```text
setpgid()
getpgrp()
kill()
```

### **Terminal Management**

```text
tcsetpgrp()
```

### **Synchronization**

```text
waitpid()
EINTR
WNOHANG
WUNTRACED
WCONTINUED
```

---

# **⚠️ Limitations**

This shell intentionally implements a focused subset of Unix shell functionality.

The following features are currently outside the scope of the project:

- Environment-variable expansion
- Command substitution
- Wildcard/glob expansion
- Shell scripting
- `if`, `for`, `while`, etc.
- Shell functions
- Aliases
- Command history
- Tab completion
- Here-documents
- Advanced file-descriptor operations such as `2>&1`
- Full POSIX shell grammar
- Shell startup/configuration files

These limitations are intentional. The project focuses on understanding **process management and Unix job control** rather than reproducing every feature of a production shell.

---

# **🎓 Learning Outcomes**

This project provides practical experience with:

### **1. Process Creation**

Understanding how a shell uses `fork()` to create processes.

### **2. Program Execution**

Understanding how `execvp()` replaces a process image with another program.

### **3. Inter-Process Communication**

Understanding how `pipe()` allows independent processes to communicate.

### **4. File Descriptors**

Understanding Unix’s standard input/output/error streams and how `dup2()` redirects them.

### **5. Process Groups**

Understanding how multiple processes can be treated as a single job.

### **6. Signals**

Understanding how Unix signals control and communicate process state.

### **7. Terminal Ownership**

Understanding why a shell must transfer terminal control to foreground jobs.

### **8. Job Management**

Understanding how a shell tracks running, stopped, continued, and completed processes.

---

# **🔮 Future Improvements**

Potential extensions include:

- Environment-variable expansion
- `$PATH` and environment management
- Wildcard/glob expansion
- `2>&1` and advanced file-descriptor redirection
- Command history
- Tab completion
- Command substitution
- More complete POSIX parsing
- Shell scripting support
- Automated regression-test framework
- Modularize the implementation into lexer, parser, executor, and job-control components

---

# **📌 Project Status**

**Core implementation: Complete**

The current implementation supports:

```text
✓ Command execution
✓ Built-in commands
✓ Quote-aware parsing
✓ Pipelines
✓ Multi-stage pipelines
✓ Input/output redirection
✓ Append redirection
✓ stderr redirection
✓ Pipeline-stage redirection
✓ Background jobs
✓ Job tracking
✓ Foreground/background control
✓ Process groups
✓ Terminal control
✓ Signal handling
✓ SIGCHLD child tracking
✓ Basic error handling
```



---

# **📄 License**

This project is licensed under the **MIT License**. See the [LICENSE](LICENSE) file for details.

