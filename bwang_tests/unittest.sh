kill %1 %2 %3 %4

memray run --native --force -o output.bin -no output.native.bin -m tutorial

memray parse output.bin > tmp

memray flamegraph --cpu-profiler-switch=1 output.bin -f -o yy.html

memray flamegraph --cpu-profiler-switch=0 output.bin -f -o xx.html

#cat tmp |grep -i ALLOCATION_WITH_NATIVE | wc -l
#cat tmp |grep -i CPU_SAMPLE_WITH_NATIVE | wc -l