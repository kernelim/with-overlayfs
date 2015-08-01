with-overlayfs(1) -- Execution wrapper for private volatile global path modifications
=====================================================================================

## SYNOPSIS

``with-overlayfs (options...) -- (command args...)``

## DESCRIPTION

The `with-overlayfs` program is designed to mimick the modification of
global paths which otherwise cannot be modified by the current
user. The original paths can be read-only paths. The purpose is to run
the provided command with its arugments under a temporary environment
in which the changes have seemingly taken place, but are not visible
to any other process, and would vanish with the termination of the
command.

## OPTIONS

* `-v`
       Print version and exit

* `--help`,`-h`, or no args

       Short a command line syntax summery

* `--discard-after-exec`

       Discard the effects from the namespace immediately after execution (uses am LD_PRELOAD wrapper)

* `--dir-start [path] (dir options...) --dir-end`

       Describe operations under the provided path. `(dir options...)` can be:

    `--replace [target] [source]`

    Replace 'target' under 'path' with the filename provided as 'source'.
    The file will be copied, and the storage location is for the overlayfs
    mounts taking place is /tmp.

## EXAMPLES

* Example A

We can experiment with a program that reads a file under `/etc`, without
modifying `/etc`.

```
    with-overlayfs                                  \
        --dir-start /etc                            \
        --replace  myconf.conf myconf-modified.conf \
        --dir-end                                   \
        -- ./program my-arg
```

* Example B

One can imagine a scenario where a dynamically linked executable must
run on a system that has conflicting shared libraries. If such
executable might fork and invoke other executables, then
`LD_LIBRARY_PATH` cannot be used.

Without resorting to solving the problem with VMs (or dockers), one
can put a copy of the libaries on which the executable is dependant in
a special directory, and instruct with-overlayfs to virtually copy the
files under their expected resting place, `/usr/lib64`:

A wrapper script for the program can be made using `with-overlayfs`, e.g.:

```
    #!/bin/bash

    exec with-overlayfs                                                             \
        --discard-after-exec                                                        \
        --dir-start /usr/lib64                                                      \
        --replace  libc.so.6            /opt/program/special/libc.so.6              \
        --replace  libpthread.so.0      /opt/program/special/libpthread.so.0        \
        --replace  libm.so.6            /opt/program/special/libm.so.6              \
        --replace  libgcc_s.so.1        /opt/program/special/libgcc_s.so.1          \
        --replace  libdl.so.2           /opt/program/special/libdl.so.2             \
        --replace  librt.so.1           /opt/program/special/librt.so.1             \
        --replace  ld-linux-x86-64.so.2 /opt/program/special/ld-linux-x86-64.so.2   \
        --replace  libstdc++.so.6       /opt/program/special/libstdc++.so.6         \
        --dir-end                                                                   \
        --  /usr/bin/program "$@"
```
