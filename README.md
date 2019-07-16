# DataScript
A lightweight but powerful lisp written in C. It allows for the creation of complicated functions and expression of multiple paradigms with a small set of powerful functions.

## Features
- Error handling with intelligent and informative error statements
- Relatively compact, core language just 1500 lines and compiles to 59kb
- List manipulation functions such as head, tail, body, pop, len, fetch, eval and join
- Conditionals and if statements / comparison functions to allow for logical programs
- Ability to read user input, allowing for interactivity
- Uses quoted expressions and eval function to allow for runtime code modification
- File load functions allowing for library support and command line loading
- Command line file handling and repl
- Supports MacOS, Windows and Linux based operating systems

## Work in progress features
- [X] Loops, currently has issue with memory allocation
- [X] Range function, currently crashes due to memory access violation
- [ ] For function to loop over each item in a list
- [ ] Push function to insert a value into a list and push all remaining items to the right
- [ ] Replace to replace an item in a list
- [X] While, print only one loop then hangs currently
- [X] File handling, functions broken on windows

## Setup - From source
1) Download and unpack the project
2) Open the csproj file within visual studio
3) Press Ctrl-F5 to compile
4) You should be presented with the repl; To access the exe go to the project directory and check the debug or release folder depending on the configuration set when compiled

## Setup - From release
1) Navigate to https://github.com/Fhoughton/DataScript/releases
2) Download and unpack the most recent version
3) Run DataScript.exe or use the command line with supplied arguments to run a file; The examples directory contains some files to run.

## Credits
Based off [Build Your Own Lisp](https://www.amazon.com/Build-Your-Lisp-Daniel-Holden/dp/1501006622) by [Daniel Holden](https://github.com/orangeduck) and uses his [mpc library](https://github.com/orangeduck/mpc) for parsing
