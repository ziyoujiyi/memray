from typing import IO
from typing import Iterable
from typing import Optional

from rich import print as rprint

from memray import AllocationRecord, CpuSampleRecord
from memray.reporters.tui import TUI


class SummaryReporter:

    N_COLUMNS = len(TUI.KEY_TO_COLUMN_NAME)

    def __init__(self, data: Iterable[AllocationRecord], cpu_data: Iterable[CpuSampleRecord], native: bool):
        super().__init__()
        self.data = data
        self.cpu_data = cpu_data
        self._tui = TUI(pid=None, cmd_line=None, native=native)
        self._tui.update_snapshot(data)
        self._tui.update_cpu_snapshot(cpu_data)

    @classmethod
    def from_snapshot(
        cls, allocations: Iterable[AllocationRecord], native: bool = False
    ) -> "SummaryReporter":
        return cls(allocations, [], native=native)

    @classmethod
    def from_cpu_snapshot(
        cls, cpu_samples: Iterable[CpuSampleRecord], native: bool = False
    ) -> "SummaryReporter":
        return cls([], cpu_samples, native=native)

    def render(
        self,
        sort_column: int,
        *,
        max_rows: Optional[int] = None,
        file: Optional[IO[str]] = None,
    ) -> None:
        self._tui.update_sort_key(sort_column)
        rprint(self._tui.get_body(max_rows=max_rows), file=file)

    def cpu_render(
        self,
        sort_column: int,
        *,
        max_rows: Optional[int] = None,
        file: Optional[IO[str]] = None,
    ) -> None:
        self._tui.update_sort_key(sort_column)
        rprint(self._tui.get_cpu_body(max_rows=max_rows), file=file)
