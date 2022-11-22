from _memray.hooks cimport Allocator
from libc.stdint cimport uintptr_t
from libcpp cimport bool
from libcpp.string cimport string
from libcpp.vector cimport vector


cdef extern from "records.h" namespace "memray::tracking_api":

   struct Frame:
       string function_name
       string filename
       int lineno

   struct TrackerStats:
       size_t n_allocations
       size_t n_cpu_samples
       size_t n_frames
       long long start_time
       long long end_time
       long long cpu_profiler_start_time
       long long cpu_profiler_end_time

   struct HeaderRecord:
       int version
       bool native_traces
       TrackerStats stats
       string command_line
       int pid
       int python_allocator

   cdef cppclass Allocation:
       Allocator allocator
       size_t frame_index
       size_t n_allocations
       object toPythonObject()

   cdef cppclass CpuSample:
       Allocator allocator
       size_t frame_index
       size_t n_cpu_samples
       object toPythonObject()

   struct MemoryRecord:
       unsigned long long int ms_since_epoch
       unsigned long long int latest_allocation_index
       size_t rss

   struct CpuRecord:
       unsigned long int ms_since_epoch

   struct MemorySnapshot:
       unsigned long long int ms_since_epoch
       unsigned long long int latest_allocation_index
       size_t rss
       size_t heap

   struct CpuSnapshot:
       unsigned long int ms_since_epoch


cdef extern from "<optional>":
   # Cython doesn't have libcpp.optional yet, so just declare this opaquely.
   cdef cppclass optional_frame_id_t "std::optional<memray::tracking_api::frame_id_t>":
       pass
