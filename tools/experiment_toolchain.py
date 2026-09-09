"""Shared period-toolchain setup for code-generation experiments."""

import os
import shlex
import subprocess


DEFAULT_IMAGE = "dsplibs-tc342-gentoo"
GENTOO_COMPILER_PATH = "/usr/i386-pc-linux-gnu/gcc-bin/3.4"
STOCK_COMPILER_PATH = "/opt/gcc342/bin"
REPRODUCE_BUGS = "-DDSPLIB_REPRODUCE_BUGS"


def is_gentoo_image(image):
    """Recognize the recovered image with an explicit tag or registry prefix."""
    name = image.split("@", 1)[0].rsplit("/", 1)[-1].split(":", 1)[0]
    return name == DEFAULT_IMAGE


def compiler_path(image, requested):
    """Choose the compiler directory, preserving old explicit A/B images."""
    if requested:
        return requested
    if is_gentoo_image(image):
        return GENTOO_COMPILER_PATH
    return STOCK_COMPILER_PATH


def native_user(image, requested):
    """Gentoo's recovered driver needs its image user; old images do not."""
    if requested is not None:
        return requested
    return is_gentoo_image(image)


def add_reproduce_bugs(flags):
    """Put the reproduction define last so callers cannot accidentally unset it."""
    return list(flags) + [REPRODUCE_BUGS]


def docker_prefix(image, root, work, native, output=None, work_target="/work"):
    command = ["docker", "run", "--rm"]
    if not native:
        command += ["--user", "%d:%d" % (os.getuid(), os.getgid())]
    command += ["--platform", "linux/386", "-v", "%s:/src" % root,
                "-v", "%s:%s" % (work, work_target)]
    if output is not None:
        command += ["-v", "%s:/out" % output]
    command += ["-w", "/src", image]
    return command


def print_identity(image, path, native):
    """Print the exact compiler and assembler selected inside the container."""
    command = ["docker", "run", "--rm"]
    if not native:
        command += ["--user", "%d:%d" % (os.getuid(), os.getgid())]
    command += ["--platform", "linux/386", image, "/bin/sh", "-c",
                "set -e; export PATH=%s:$PATH; "
                "echo 'gcc executable:'; command -v gcc; "
                "gcc --version; printf 'gcc machine: '; gcc -dumpmachine; "
                "printf 'gcc version: '; gcc -dumpversion; "
                "as_path=$(gcc -print-prog-name=as); "
                "printf 'gcc assembler: %%s\\n' \"$as_path\"; "
                "\"$as_path\" --version" % shlex.quote(path)]
    print("toolchain identity: image %s; PATH=%s" % (image, path), flush=True)
    subprocess.run(command, check=True)


def compile_shell(path, flags, output, source):
    """Build a shell command with a final, mandatory reproduction define."""
    all_flags = add_reproduce_bugs(flags)
    return ("export PATH=%s:$PATH; exec gcc -c %s -o %s %s" %
            (shlex.quote(path), shlex.join(all_flags), shlex.quote(str(output)),
             shlex.quote(str(source))))
