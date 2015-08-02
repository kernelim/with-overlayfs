/*
 * Copyright 2015 Dan Aloni, Kernelim Ltd. See the LICENSE-2.0.txt
 * file at the top-level directory of this distribution.
 */

#include "common.h"
#include "config.h"

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <sched.h>
#include <errno.h>
#include <sys/mount.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sys/socket.h>

#include <string>
#include <vector>
#include <iostream>
#include <memory>
#include <fstream>

/*
 * Utility section
 */

class AssertException : public std::exception {
};

#define ASSERT(cond) do {                                                   \
        int _cond_ = cond;                                                  \
        if (!_cond_) {                                                      \
            fprintf(stderr, "error: %s failed, errno=%d\n", #cond, errno);  \
            throw AssertException();                                        \
        }                                                                   \
    } while (0)


class TempPath {
public:
    std::string path;
    std::vector<std::string> dirs_to_unlink;
    std::vector<std::string> files_to_unlink;

    TempPath() {
        char xtemplate[] = "/tmp/with-overlayfsXXXXXX";
        path = mkdtemp(xtemplate);
    };
    ~TempPath() {
        for (auto filename: files_to_unlink) {
            unlink((path + "/" + filename).c_str());
        }
        for (auto dir: dirs_to_unlink) {
            rmdir((path + "/" + dir).c_str());
        }
        rmdir(path.c_str());
    };
};

/*
 * Parsed command line
 */
struct Params {
    struct Dir {
        std::string path;

        struct Replacement {
            std::string target;
            std::string source;
        };
        std::vector<Replacement> replacements;
    };

    std::vector<Dir> dirs;
    bool discard_after_exec = false;
};

/*
 * Program state
 */
struct State {
    struct DirState {
        const Params::Dir &dir;
        std::unique_ptr<TempPath> uppertmp;
        std::unique_ptr<TempPath> workdirtmp;

        struct Replacement {
            const Params::Dir::Replacement &replacement;
            std::unique_ptr<std::ifstream> input_file;
            struct stat st;
        };
        std::vector<Replacement> replacements;
    };

    std::vector<DirState> dirs;
};

void print_version(void)
{
    printf("%s\n", PACKAGE_STRING);
}

void syntax(void)
{
    print_version();

    printf("\n");
    printf("syntax: with-overlayfs (options...) -- (command args...)\n");
    printf("\n");
    printf("  where (options...):\n");
    printf("\n");
    printf("    --discard-after-exec\n");
    printf("    --help, -h\n");
    printf("    -v\n");
    printf("    --dir-start [pathname] (dir options...) --dir-end\n");
    printf("\n");
    printf("    where (dir options...) are a list of:\n");
    printf("\n");
    printf("      --replace [target] [source]\n");
    printf("\n");
}

/*
 * Program functions
 */

int parse_command_line(Params &params, int argc, char *argv[], int &program_args)
{
    int argi = 1;

    class stop : public std::exception {
    };

    auto expect = [&argi, argv, argc](std::string val) {
        auto arg = std::string(argv[argi]);
        if (arg == val) {
            argi++;
            return true;
        }

        return false;
    };

    std::vector<std::string> context;

    auto err = [&argi, argv, argc, &context](std::string msg) {
        std::string s;

        if (context.size() > 0) {
            s += " (under ";
            for (auto c : context) {
                s += ": " + c;
            }
            s += ")";
        }

        fprintf(stderr, "error: %s%s\n", msg.c_str(), s.c_str());
        throw stop();
    };

    auto take = [&argi, argv, argc, err](std::string what) -> std::string {
        if (argi < argc) {
            auto arg = std::string(argv[argi]);
            argi++;
            return arg;
        }

        err("unexpected end of argument list, expected " + what);
    };

    auto invalid = [&argi, argv, err]() {
        auto arg = std::string(argv[argi]);

        err("invalid argument " + arg);
    };

    class Push {
    private:
        std::vector<std::string> &vector;
    public:
        Push(std::vector<std::string> &vector, std::string str)
            : vector(vector) {
            vector.push_back(str);
        }
        ~Push() {
            vector.pop_back();
        }
    };

    try {
        while (argi < argc) {
            if (expect("--")) {
                program_args = argi;
                return 0;
            }

            if (expect("-v")) {
                print_version();
                exit(-1);
            }

            if (expect("-v")  ||
                expect("--help") ||
                expect("-h")) {
                syntax();
                exit(-1);
            }

            if (expect("--discard-after-exec")) {
                params.discard_after_exec = true;
                continue;
            }

            if (expect("--dir-start")) {
                auto dir_path = take("path");
                Push p(context, dir_path);
                Params::Dir dir {dir_path, {}};

                while (argi < argc) {
                    if (expect("--replace")) {
                        auto filename = take("target");
                        auto other_filename = take("source");
                        dir.replacements.push_back({filename, other_filename});
                        continue;
                    }
                    if (expect("--dir-end")) {
                        params.dirs.push_back(dir);
                        break;
                    }

                    invalid();
                }
                continue;
            }

            invalid();
        }

        fprintf(stderr, "error: program and arguments not specified\n");
        throw stop();

    } catch (stop e) {
        return -1;
    }
}

int prepare_state(const Params &params, State &state, int ruid, int rgid)
{
    for (auto &dir : params.dirs) {
        /*
         * Create temporary directories.
         */
        TempPath tmp_mountpoint;
        std::string lowerdir = dir.path;
        state.dirs.push_back({
                dir,
                std::unique_ptr<TempPath>(new TempPath()),
                std::unique_ptr<TempPath>(new TempPath())
           });

        auto &dirstate = state.dirs.back();
        std::string upperdir = dirstate.uppertmp->path;
        std::string workdir = dirstate.workdirtmp->path;

        ASSERT(0 == chown(upperdir.c_str(), ruid, rgid));
        ASSERT(0 == chown(workdir.c_str(), ruid, rgid));
        ASSERT(0 == chown(tmp_mountpoint.path.c_str(), ruid, rgid));
        dirstate.workdirtmp->dirs_to_unlink.push_back("work");

        /*
         * Open files to be operated upon.
         */
        for (auto &replacement : dir.replacements) {
            struct stat st;

            ASSERT(stat(replacement.source.c_str(), &st) == 0);

            dirstate.replacements.push_back({
                    replacement,
                    std::unique_ptr<std::ifstream>(
                           new std::ifstream(replacement.source,
                                             std::ios::binary)),
                                             st});
            dirstate.uppertmp->files_to_unlink.push_back(replacement.target);
        }

        /*
         * Mount overlayfs
         */
        std::string  opts = "lowerdir=" + lowerdir +
            ",upperdir=" +  upperdir +
            ",workdir=" + workdir;
        std::string merged = tmp_mountpoint.path;

        ASSERT(mount("overlay", merged.c_str(), "overlay", 0, opts.c_str()) == 0);

        /*
         * Move the new mount point over the directory it overlays.
         */

        ASSERT(mount(merged.c_str(), lowerdir.c_str(), NULL, MS_MOVE, NULL) == 0);
    }
}

int lazy_unmount(const Params &params)
{
    for (auto &dir : params.dirs) {
        ASSERT(umount2(dir.path.c_str(), MNT_DETACH) == 0);
    }
}

int crux(int argc, char *argv[])
{
    if (argc <= 1) {
        syntax();
        return -1;
    }

    /*
     * First, parse the command line.
     */

    Params params;
    int program_args = 1;

    int rc = parse_command_line(params, argc, argv, program_args);
    if (rc < 0) {
        return rc;
    }

    /*
     * Next, get the original IDs of the user executing this setuid program.
     */

    uid_t ruid, euid, suid;
    gid_t rgid, egid, sgid;
    ASSERT(getresgid(&rgid, &egid, &sgid) >= 0);
    ASSERT(getresuid(&ruid, &euid, &suid) >= 0);

    ASSERT(unshare(CLONE_NEWNS) >= 0);

    /* The following is needed for MS_MOVE to work */

    ASSERT(mount(NULL, "/", "none", MS_PRIVATE | MS_REC, NULL) == 0);

    State state;
    prepare_state(params, state, ruid, rgid);

    int sockets[2];
    pid_t discard_pid = -1;
    if (params.discard_after_exec) {
        /* Fork a subprocess for the umount */

        ASSERT(socketpair(AF_UNIX, SOCK_DGRAM, 0, sockets) == 0);

        discard_pid = fork();
        if (discard_pid == 0) {
            close(sockets[1]);

            char q[1];
            ASSERT(0 <= read(sockets[0], q, 1));
            lazy_unmount(params);
            ASSERT(0 <= write(sockets[0], q, 1));

            exit(-1);
            return 0;
        }

        close(sockets[0]);
    }

    /* Drop back to regular user */

    ASSERT(setgid(rgid) >= 0);
    ASSERT(setuid(ruid) >= 0);

    /*
     * Operate on the files.
     */
    for (auto &dirstate : state.dirs) {
        auto &dir = dirstate.dir;

        for (auto &replacement_state : dirstate.replacements) {
            auto &replacement = replacement_state.replacement;
            auto target_fullpath = dir.path + "/" + replacement.target;
            unlink(target_fullpath.c_str());

            std::ofstream dst(target_fullpath, std::ios::binary);
            dst << replacement_state.input_file->rdbuf();

            ASSERT(chmod(target_fullpath.c_str(),
                         replacement_state.st.st_mode & ~0170000) >= 0);
        }
    }

    /*
     * Execute the program in our crafted environment.
     */
    pid_t pid = vfork();
    if (pid == 0) {
        if (params.discard_after_exec) {
            static char str_1[0x100];
            snprintf(str_1, sizeof(str_1), "%s=%s",
                     "LD_PRELOAD",
                     // TODO: Find a better way to locate the shared library
                     PREFIX_PATH "/share/with-overlayfs/hook.so");
            ASSERT(putenv(str_1) == 0);

            static char str_2[0x100];
            snprintf(str_2, sizeof(str_2), "%s=%d",
                     AFTER_LOAD_ENV, sockets[1]);

            ASSERT(putenv(str_2) == 0);
        }

        int r = execvp(argv[program_args], &argv[program_args]);
        exit(-1);
        return r;
    }

    if (params.discard_after_exec) {
        close(sockets[1]);
    }

    if (discard_pid != -1) {
        int status = 0;
        waitpid(pid, &status, 0);
    }

    int status = 0;
    waitpid(pid, &status, 0);

    if (WIFEXITED(status)) {
        return WEXITSTATUS(status);
    }

    return -1;
}

int main(int argc, char *argv[])
{
    try {
        return crux(argc, argv);
    } catch (AssertException e) {
        return -1;
    }
}
