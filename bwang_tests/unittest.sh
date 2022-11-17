kill %1 %2 %3 %4

# memray run -h

memray run --native --trace-cpu=1 --trace-memory=1 --cpu-interval-ms=11 --memory-interval-ms=10 --force --quiet -o tmp-output.bin -m tutorial

memray parse output.bin > tmp

# memray flamegraph -h

memray flamegraph --trace-memory=1 --filter-boring-frame=0 output.bin -f -o xx.html

memray flamegraph --trace-cpu=1 --filter-boring-frame=0 output.bin -f -o yy.html

#cat tmp |grep -i ALLOCATION_WITH_NATIVE | wc -l
#cat tmp |grep -i CPU_SAMPLE_WITH_NATIVE | wc -l