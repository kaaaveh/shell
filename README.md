# Basic Shell

A simple Unix-like shell implementation that supports basic command execution, background processes, and signal handling.

## Features

- Command execution with argument parsing
- Background process support (using &)
- Built-in commands:
  - `jobs`: List all running background processes
  - `kill`: Terminate background processes by index
- Signal handling:
  - SIGINT (Ctrl+C) handling for foreground processes
  - SIGCHLD handling for background processes
- Process management:
  - Background process tracking
  - Automatic cleanup of terminated processes
  - Process group management

## Building the Program

To compile the shell, use the following command:

```bash
make
```

This will create an executable named `shell`.

## Running the Shell

To start the shell, run:

```bash
./shell
```

## Usage

### Basic Commands

The shell supports execution of basic Unix commands:

```bash
ls -l
pwd
echo "Hello, World!"
```

### Background Processes

To run a process in the background, append `&` to the command:

```bash
sleep 100 &
```

### Built-in Commands

#### jobs
Lists all currently running background processes:
```bash
jobs
```

#### kill
Terminates a background process by its index:
```bash
kill <index> [signal]
```
- `<index>`: The index number of the background process
- `[signal]`: (Optional) The signal number to send (defaults to SIGTERM)

### Signal Handling

- Press `Ctrl+C` to send SIGINT to the foreground process
- Background processes are automatically cleaned up when they terminate

## Implementation Details

The shell is implemented in C and uses the following key components:

- Process management using `fork()`, `exec()`, and `wait()`
- Signal handling with `sigaction()`
- Command parsing and execution
- Background process tracking
- File redirection support

## Limitations

- Maximum number of background processes: 100
- Maximum command length: 100 characters
- Maximum number of arguments per command: 100
- Maximum number of commands in a pipeline: 10

## Error Handling

The shell includes error handling for:
- Invalid commands
- Process creation failures
- Signal handling errors
- Invalid background process indices
- File operation errors

## Exit Codes

- 0: Successful execution
- 1: General error
- 2: Command not found or invalid arguments 