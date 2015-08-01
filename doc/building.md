# Building

## Common Linux distributions

The required packages are make, g++, automake, autoconf, libtool,
and the ronn nodejs package. With all the dependencies, it can be 
built as such:


```
    ./autogen.sh
    ./configure
    make
    make install

```

## RPM under Fedora

First,

```
    yum install mock rpm-build
```

Then, the following will generate the RPM and invoke mock:

```
    ./packaging/build-srpm -o . -- -r default

```

The reuslt outputs are in a subdirectory under the current directory.

```
    $ ls -1 with-overlayfs-0.1-2015.08.01.182506.git471b7f084d8d.fc21.src.rpm.mockresult
    build.log
    root.log
    state.log
    with-overlayfs-0.1-2015.08.01.182506.git471b7f084d8d.fc21.src.rpm
    with-overlayfs-0.1-2015.08.01.182506.git471b7f084d8d.fc21.x86_64.rpm
    with-overlayfs-debuginfo-0.1-2015.08.01.182506.git471b7f084d8d.fc21.x86_64.rpm

```
