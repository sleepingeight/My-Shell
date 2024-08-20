# My Shell

Implementation of a terminal/shell in C,made to learn processes, system-calls and signals, and to apply them.

Features I've tried to implement:
- normal commands and some complex commands like cd
- adding a & to the end of the command makes the process run in background
- multiple commands seperated by '&&' run serially and '&&&' run in parallel.
- CTRL+C stops all the foreground processes
- 'exit' command closes the shell
