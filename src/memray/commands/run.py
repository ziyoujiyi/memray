import argparse
import ast
import contextlib
import os
import pathlib
import runpy
import socket
import subprocess
import sys
import textwrap
from textwrap import dedent
from contextlib import closing
from contextlib import suppress
from typing import List
from typing import Optional
import time

from memray import Destination
from memray import FileDestination
from memray import SocketDestination
from memray import Tracker
from memray._errors import MemrayCommandError
from memray.commands.live import LiveCommand
from memray.commands.common import logger


def _get_free_port() -> int:
    with closing(socket.socket(socket.AF_INET, socket.SOCK_STREAM)) as sock:
        sock.bind(("", 0))
        sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        return int(sock.getsockname()[1])


def _run_tracker(
    destination: Destination,
    args: argparse.Namespace,
    post_run_message: Optional[str] = None,
    follow_fork: bool = False,
    trace_python_allocators: bool = False,
) -> None:
    try:
        kwargs = {}
        if follow_fork:
            kwargs["follow_fork"] = True
        if trace_python_allocators:
            kwargs["trace_python_allocators"] = True
        tracker = Tracker(destination=destination, native_traces=args.native, trace_mmap=args.trace_mmap, memory_interval_ms=args.memory_interval_ms,
                          cpu_interval_ms=args.cpu_interval_ms, trace_cpu=args.trace_cpu, trace_memory=args.trace_memory, **kwargs)
    except OSError as error:
        raise MemrayCommandError(str(error), exit_code=1)

    with tracker:  # first, call __enter__ method
        pid = os.getpid()
        try:
            if args.run_as_module:
                sys.path[0] = os.getcwd()
                # run_module will replace argv[0] with the script's path
                sys.argv = ["", *args.script_args]
                runpy.run_module(args.script, run_name="__main__", alter_sys=True)
            elif args.run_as_cmd:
                sys.path[0] = ""
                sys.argv = ["-c", *args.script_args]
                exec(args.script, {"__name__": "__main__"})
            else:
                sys.path[0] = str(pathlib.Path(args.script).resolve().parent.absolute())
                sys.argv = [args.script, *args.script_args]
                runpy.run_path(args.script, run_name="__main__")
        finally:
            if not args.quiet and post_run_message is not None and pid == os.getpid():
                print(post_run_message)


def _child_process(
    port: int,
    native: bool,
    run_as_module: bool,
    run_as_cmd: bool,
    quiet: bool,
    script: str,
    script_args: List[str],
) -> None:
    args = argparse.Namespace(
        native=native,
        run_as_module=run_as_module,
        run_as_cmd=run_as_cmd,
        quiet=quiet,
        script=script,
        script_args=script_args,
    )
    _run_tracker(destination=SocketDestination(server_port=port), args=args)


def _run_child_process_and_attach(args: argparse.Namespace) -> None:
    port = args.live_port
    if port is None:
        port = _get_free_port()
    if not 2**16 > port > 0:
        raise MemrayCommandError(f"Invalid port: {port}", exit_code=1)

    arguments = (
        f"{port},{args.native},{args.run_as_module},{args.run_as_cmd},{args.quiet},"
        f"{args.script!r},{args.script_args}"
    )
    tracked_app_cmd = [
        sys.executable,
        "-c",
        f"from memray.commands.run import _child_process;_child_process({arguments})",
    ]
    with contextlib.suppress(KeyboardInterrupt):
        with subprocess.Popen(
            tracked_app_cmd,
            stderr=subprocess.PIPE,
            stdout=subprocess.DEVNULL,
            text=True,
        ) as process:
            try:
                LiveCommand().start_live_interface(port)
            except (Exception, KeyboardInterrupt) as error:
                process.terminate()
                raise error from None
            process.terminate()
            if process.returncode:
                if process.stderr:
                    print(process.stderr.read(), file=sys.stderr)
                raise (MemrayCommandError(exit_code=process.returncode))


def _run_with_socket_output(args: argparse.Namespace) -> None:
    port = args.live_port
    if port is None:
        port = _get_free_port()
    if not 2**16 > port > 0:
        raise MemrayCommandError(f"Invalid port: {port}", exit_code=1)

    if not args.quiet:
        memray_cli = f"memray{sys.version_info.major}.{sys.version_info.minor}"
        print(f"Run '{memray_cli} live {port}' in another shell to see live results")
    with suppress(KeyboardInterrupt):
        _run_tracker(destination=SocketDestination(server_port=port), args=args)


def _run_with_file_output(args: argparse.Namespace) -> None:
    if args.output is None:
        script_name = args.script
        if args.run_as_cmd:
            script_name = "string"

        output = f"memray-{os.path.basename(script_name)}.{os.getpid()}.bin"
        filename = os.path.join(os.path.dirname(script_name), output)
    else:
        filename = args.output

    if not args.quiet:
        print(f"Writing profile results into {filename}")

    example_report_generation_message = textwrap.dedent(
        f"""
        [memray] Successfully generated profile results.

        You can now generate reports from the stored allocation & records.
        Some example commands to generate reports:

        {sys.executable} -m memray flamegraph {filename}
        """
    ).strip()

    destination = FileDestination(
        path=filename, overwrite=args.force, compress_on_exit=args.compress_on_exit
    )
    try:
        _run_tracker(
            destination=destination,
            args=args,
            post_run_message=example_report_generation_message,
            follow_fork=args.follow_fork,
            trace_python_allocators=args.trace_python_allocators,
        )
    except OSError as error:
        raise MemrayCommandError(str(error), exit_code=1)


class RunCommand:
    """Run the specified application and track memory usage"""

    def prepare_parser(self, parser: argparse.ArgumentParser) -> None:
        parser.usage = "%(prog)s [-m module | -c cmd | file] [args]"
        cpu_group = parser.add_mutually_exclusive_group()
        parser.add_argument(
            "--trace-cpu",
            # is the action to be taken when this argument is found on the command line.
            action="store",
            dest="trace_cpu",
            help=textwrap.dedent('''\
            cpu profiler switch \n
            type: int, default value is 1(on),
            '''),
            type=int,
            default=1,
        )
        parser.add_argument(
            "--trace-memory",
            # is the action to be taken when this argument is found on the command line.
            action="store",
            dest="trace_memory",
            help="memory profiler switch \n type: int, default value is 1(on)",
            type=int,
            default=1,
        )
        parser.add_argument(
            "--cpu-interval-ms",
            help=textwrap.dedent(
                '''
                type: int(ms), default value is 11 \n
                cpu sampling frequency
                '''
            ),
            action="store",
            dest="cpu_interval_ms",
            type=int,
            default=11,

        )
        output_group = parser.add_mutually_exclusive_group()
        output_group.add_argument(
            "-o",
            "--output",
            help="Output file name (default: <process_name>.<pid>.bin)",
        )
        output_group.add_argument(
            "--live",
            help="Start a live tracking session and immediately connect a live server",
            action="store_true",
            dest="live_mode",
            default=False,
        )
        output_group.add_argument(
            "--live-remote",
            help="Start a live tracking session and wait until a client connects",
            action="store_true",
            dest="live_remote_mode",
            default=False,
        )
        parser.add_argument(
            "--live-port",
            "-p",
            help="Port to use when starting live tracking (default: random free port)",
            default=None,
            type=int,
        )
        parser.add_argument(
            "--native",
            help="Track native (C/C++) stack frames as well",
            action="store_true",
            dest="native",
            default=False,
        )
        parser.add_argument(
            "--trace-mmap",
            help="trace mmap/munmap, type: bool, default value is False",
            action="store_true",  # store_true 表示一旦有这个参数，做出动作“将其值标为 True”，也就是没有时，默认状态下其值为 False
            dest="trace_mmap",
            default=False,
        )
        parser.add_argument(
            "--follow-fork",
            action="store_true",
            help="Record allocations in child processes forked from the tracked script",
            default=False,
        )
        parser.add_argument(
            "--trace-python-allocators",
            action="store_true",
            help="Record allocations made by the Pymalloc allocator",
            default=False,
        )
        parser.add_argument(
            "--memory-interval-ms",
            metavar="N",
            help=dedent(
                """
                type: int(ms), default value is 10 
                get rss: read /proc/self/statm frequency
                """
            ),
            action="store",
            dest="memory_interval_ms",
            type=int,
            default=100,
        )
        parser.add_argument(
            "-q",
            "--quiet",
            help="Don't show any tracking-specific output while running",
            action="store_true",
        )
        parser.add_argument(
            "-f",
            "--force",
            help="If the output file already exists, overwrite it",
            action="store_true",
            default=False,
        )
        compression = parser.add_mutually_exclusive_group()
        compression.add_argument(
            "--compress-on-exit",
            help="Compress the resulting file using lz4 after tracking completes",
            default=True,
            action="store_true",
        )
        compression.add_argument(
            "--no-compress",
            help="Do not compress the resulting file using lz4",
            default=False,
            action="store_true",
        )
        parser.add_argument(
            "-c",
            help="Program passed in as string",
            action="store_true",
            dest="run_as_cmd",
            default=False,
        )
        parser.add_argument(
            "-m",
            help="Run library module as a script (terminates option list)",
            action="store_true",
            dest="run_as_module",
        )
        parser.add_argument("script", help=argparse.SUPPRESS, metavar="file")
        parser.add_argument(
            "script_args",
            help=argparse.SUPPRESS,
            nargs=argparse.REMAINDER,
            metavar="module",
        )

    def validate_target_file(self, args: argparse.Namespace) -> None:
        """Ensure we are running a Python file"""
        if args.run_as_module:
            return
        try:
            if args.run_as_cmd:
                source = bytes(args.script, "UTF-8")
            else:
                source = pathlib.Path(args.script).read_bytes()
            ast.parse(source)
        except (SyntaxError, ValueError):
            raise MemrayCommandError(
                "Only valid Python files or commands can be executed under memray",
                exit_code=1,
            )

    def run(self, args: argparse.Namespace, parser: argparse.ArgumentParser) -> None:
        start_time = time.time()
        if args.no_compress:
            args.compress_on_exit = False

        if args.live_port is not None and not args.live_remote_mode:
            parser.error("The --live-port argument requires --live-remote")
        if args.follow_fork is True and (args.live_mode or args.live_remote_mode):
            parser.error("--follow-fork cannot be used with the live TUI")
        with contextlib.suppress(OSError):
            if args.run_as_cmd and pathlib.Path(args.script).exists():
                parser.error("remove the option -c to run a file")

        self.validate_target_file(args)

        if args.live_mode:
            _run_child_process_and_attach(args)
        elif args.live_remote_mode:
            _run_with_socket_output(args)
        else:
            _run_with_file_output(args)
        end_time = time.time()
        elapsed_time = end_time - start_time
        logger.info("memray profiler time cost: {}".format(elapsed_time))
