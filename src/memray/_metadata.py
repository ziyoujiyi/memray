from dataclasses import dataclass
from datetime import datetime


@dataclass
class Metadata:
    start_time: datetime
    end_time: datetime
    total_allocations: int
    total_frames: int
    peak_memory: int
    command_line: str
    pid: int
    python_allocator: str
    has_native_traces: bool

@dataclass
class CpuMetadata:
    cpu_profiler_start_time: datetime
    cpu_profiler_end_time: datetime
    total_cpu_samples: int
    total_frames: int
    command_line: str
    pid: int
    has_native_traces: bool
