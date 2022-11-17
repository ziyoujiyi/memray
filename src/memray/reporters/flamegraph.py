import html
import linecache
import sys
from typing import Any
from typing import Dict
from typing import Iterable
from typing import Iterator
from typing import TextIO

from memray import AllocationRecord
from memray import CpuSampleRecord
from memray import MemorySnapshot
from memray import CpuSnapshot
from memray import Metadata
from memray import CpuMetadata
from memray.reporters.frame_tools import StackFrame, is_frame_boring
from memray.reporters.frame_tools import is_cpython_internal
from memray.reporters.frame_tools import is_frame_interesting
from memray.reporters.templates import render_report
from memray.reporters.templates import cpu_render_report

MAX_STACKS = int(sys.getrecursionlimit() // 2.5)


def with_converted_children_dict(node: Dict[str, Any]) -> Dict[str, Any]:
    stack = [node]
    while stack:
        the_node = stack.pop()
        the_node["children"] = [child for child in the_node["children"].values()]  # convert to list
        stack.extend(the_node["children"])
    #breakpoint()
    return node


def create_framegraph_node_from_stack_frame(stack_frame: StackFrame) -> Dict[str, Any]:
    function, filename, lineno = stack_frame

    name = (
        # Use the source file line.
        linecache.getline(filename, lineno)
        # Or just describe where it is from
        or f"{function} at {filename}:{lineno}"
    )
    return {
        "name": name,
        "location": [html.escape(str(part)) for part in stack_frame],
        "value": 0,
        "children": {},
        "n_allocations": 0,
        "thread_id": 0,
        "interesting": is_frame_interesting(stack_frame),
    }


def create_cpu_framegraph_node_from_stack_frame(stack_frame: StackFrame) -> Dict[str, Any]:
    function, filename, lineno = stack_frame

    name = (
        # Use the source file line.
        linecache.getline(filename, lineno)
        # Or just describe where it is from
        or f"{function} at {filename}:{lineno}"
    )
    return {
        "name": name,
        "location": [html.escape(str(part)) for part in stack_frame],
        "value": 0,  # if value == 0, it will not show in html
        "children": {},
        "n_cpu_samples": 0,
        "thread_id": 0,
        "interesting": is_frame_interesting(stack_frame),
    }


class FlameGraphReporter:
    def __init__(
        self,
        data: Dict[str, Any],
        cpu_data: Dict[str, Any],
        *,
        memory_records: Iterable[MemorySnapshot],
        cpu_records: Iterable[CpuSnapshot]
    ) -> None:
        super().__init__()
        self.data = data
        self.cpu_data = cpu_data
        self.memory_records = memory_records  # no use ?
        self.cpu_records = cpu_records

    @classmethod
    def from_snapshot(
        cls,
        allocations: Iterator[AllocationRecord],
        memory_records: Iterable[MemorySnapshot],
        *,
        native_traces: bool,
        **kwargs
    ) -> "FlameGraphReporter":
        data: Dict[str, Any] = {
            "name": "<root>",
            "location": [html.escape("<tracker>"), "<b>memray</b>", 0],
            "value": 0,
            "children": {},
            "n_allocations": 0,
            "thread_id": "0x0",
            "interesting": True,
        }

        unique_threads = set()
        for record in allocations:
            size = record.size
            thread_id = record.thread_name

            data["value"] += size
            data["n_allocations"] += record.n_allocations

            current_frame = data
            stack = (
                tuple(record.hybrid_stack_trace())
                if native_traces
                else record.stack_trace()
            )
            num_skipped_frames = 0
            for index, stack_frame in enumerate(reversed(stack)):  # reverse
                if is_cpython_internal(stack_frame):
                    num_skipped_frames += 1
                    continue
                #from memray.commands.common import logger
                if is_frame_boring(stack_frame, kwargs["filter_boring_frame"]):
                    num_skipped_frames += 1
                    continue
                if (stack_frame, thread_id) not in current_frame["children"]:
                    node = create_framegraph_node_from_stack_frame(stack_frame)
                    current_frame["children"][(stack_frame, thread_id)] = node

                current_frame = current_frame["children"][(stack_frame, thread_id)]
                current_frame["value"] += size
                current_frame["n_allocations"] += record.n_allocations
                current_frame["thread_id"] = thread_id
                unique_threads.add(thread_id)

                if index - num_skipped_frames > MAX_STACKS:
                    current_frame["name"] = "<STACK TOO DEEP>"
                    current_frame["location"] = ["...", "...", 0]
                    break

        transformed_data = with_converted_children_dict(data)
        transformed_data["unique_threads"] = sorted(unique_threads)
        return cls(data=transformed_data, cpu_data={}, memory_records=memory_records, cpu_records=[])

    @classmethod
    def from_cpu_snapshot(
        cls,
        cpu_samples: Iterator[CpuSampleRecord],
        cpu_records: Iterable[CpuSnapshot],
        *,
        native_traces: bool,
        **kwargs
    ) -> "FlameGraphReporter":
        data: Dict[str, Any] = {
            "name": "<root>",
            "location": [html.escape("<tracker>"), "<b>memray</b>", 0],
            "value": 0,
            "children": {},
            "n_cpu_samples": 0,
            "thread_id": "0x0",
            "interesting": True,
        }

        unique_threads = set()
        for cpu_sample in cpu_samples:
            size = cpu_sample.n_cpu_samples
            thread_id = cpu_sample.thread_name

            data["n_cpu_samples"] += cpu_sample.n_cpu_samples
            data["value"] += cpu_sample.n_cpu_samples

            current_frame = data
            stack = (
                tuple(cpu_sample.hybrid_stack_trace())
                if native_traces
                else cpu_sample.stack_trace()
            )
            num_skipped_frames = 0
            for index, stack_frame in enumerate(reversed(stack)):
                if is_cpython_internal(stack_frame):
                    num_skipped_frames += 1
                    continue
                if is_frame_boring(stack_frame, kwargs["filter_boring_frame"]):
                    num_skipped_frames += 1
                    continue
                if (stack_frame, thread_id) not in current_frame["children"]:
                    node = create_cpu_framegraph_node_from_stack_frame(stack_frame)
                    current_frame["children"][(stack_frame, thread_id)] = node

                current_frame = current_frame["children"][(stack_frame, thread_id)]
                current_frame["n_cpu_samples"] += cpu_sample.n_cpu_samples
                current_frame["value"] += cpu_sample.n_cpu_samples
                current_frame["thread_id"] = thread_id
                unique_threads.add(thread_id)

                if index - num_skipped_frames > MAX_STACKS:
                    current_frame["name"] = "<STACK TOO DEEP>"
                    current_frame["location"] = ["...", "...", 0]
                    break

        transformed_data = with_converted_children_dict(data)
        transformed_data["unique_threads"] = sorted(unique_threads)
        return cls(data={}, cpu_data=transformed_data, memory_records=[], cpu_records=cpu_records)

    def render(
        self,
        outfile: TextIO,
        metadata: Metadata,
        show_memory_leaks: bool,
        merge_threads: bool,
    ) -> None:
        html_code = render_report(
            kind="flamegraph",
            data=self.data,
            metadata=metadata,
            memory_records=self.memory_records,  # memory_records only for trend ?
            show_memory_leaks=show_memory_leaks,
            merge_threads=merge_threads,
        )
        #breakpoint()
        print(html_code, file=outfile)
    
    def cpu_render(
        self,
        outfile: TextIO,
        metadata: CpuMetadata,
        merge_threads: bool,
    ) -> None:
        html_code = cpu_render_report(
            kind="cpuflamegraph",
            data=self.cpu_data,
            metadata=metadata,
            cpu_records=self.cpu_records,
            merge_threads=merge_threads,
        )
        #breakpoint()
        print(html_code, file=outfile)
