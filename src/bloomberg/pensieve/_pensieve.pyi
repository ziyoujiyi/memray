from typing import Any, ClassVar, List, Tuple, Iterable, Union, Callable, Optional, Type
from types import TracebackType
from pathlib import Path
import enum

from ._metadata import Metadata

PythonStackElement = Tuple[str, str, int]
NativeStackElement = Tuple[str, str, int]

class AllocationRecord:
    @property
    def address(self) -> int: ...
    @property
    def allocator(self) -> int: ...
    @property
    def n_allocations(self) -> int: ...
    @property
    def size(self) -> int: ...
    @property
    def stack_id(self) -> int: ...
    @property
    def tid(self) -> int: ...
    def hybrid_stack_trace(
        self,
        max_stacks: Optional[int] = None,
    ) -> Iterable[Union[PythonStackElement, NativeStackElement]]: ...
    def native_stack_trace(
        self, max_stacks: Optional[int] = None
    ) -> List[NativeStackElement]: ...
    def stack_trace(
        self, max_stacks: Optional[int] = None
    ) -> List[PythonStackElement]: ...
    def __eq__(self, other: Any) -> Any: ...
    def __ge__(self, other: Any) -> Any: ...
    def __gt__(self, other: Any) -> Any: ...
    def __hash__(self) -> Any: ...
    def __le__(self, other: Any) -> Any: ...
    def __lt__(self, other: Any) -> Any: ...
    def __ne__(self, other: Any) -> Any: ...

class AllocatorType(enum.IntEnum):
    MALLOC: int
    FREE: int
    CALLOC: int
    REALLOC: int
    POSIX_MEMALIGN: int
    MEMALIGN: int
    VALLOC: int
    PVALLOC: int
    MMAP: int
    MUNMAP: int

class FileReader:
    @property
    def has_native_traces(self) -> bool: ...
    @property
    def metadata(self) -> Metadata: ...
    def __init__(self, file_name: Union[str, Path]) -> None: ...
    def get_allocation_records(self) -> Iterable[AllocationRecord]: ...
    def get_high_watermark_allocation_records(
        self, merge_threads: bool
    ) -> Iterable[AllocationRecord]: ...
    def get_leaked_allocation_records(
        self, merge_threads: bool
    ) -> Iterable[AllocationRecord]: ...
    def __enter__(self) -> Any: ...
    def __exit__(
        self,
        exctype: Optional[Type[BaseException]],
        excinst: Optional[BaseException],
        exctb: Optional[TracebackType],
    ) -> bool: ...
    @property
    def closed(self) -> bool: ...
    def close(self) -> None: ...

class Tracker:
    @property
    def reader(self) -> FileReader: ...
    def __init__(
        self, file_name: Union[Path, str], *, native_traces: bool = False
    ) -> None: ...
    def __enter__(self) -> Any: ...
    def __exit__(
        self,
        exctype: Optional[Type[BaseException]],
        excinst: Optional[BaseException],
        exctb: Optional[TracebackType],
    ) -> bool: ...

class MemoryAllocator:
    def __init__(self) -> None: ...
    def free(self) -> None: ...
    def malloc(self, size: int) -> None: ...
    def calloc(self, size: int) -> None: ...
    def realloc(self, size: int) -> None: ...
    def posix_memalign(self, size: int) -> None: ...
    def memalign(self, size: int) -> None: ...
    def valloc(self, size: int) -> None: ...
    def pvalloc(self, size: int) -> None: ...
    def run_in_pthread(self, callback: Callable[[], None]) -> None: ...

class MmapAllocator:
    def __init__(self, size: int, address: int = 0) -> None: ...
    @property
    def address(self) -> int: ...
    def munmap(self, length: int, offset: int = 0) -> None: ...

def _cython_nested_allocation(
    allocator_fn: Callable[[int], None], size: int
) -> None: ...
