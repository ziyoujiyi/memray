# support 3 modes: only track cpu / only track memory / both track cpu and memory simultaneously

kill %1 %2 %3 %4

# memray run -h  # for help

memray run --native --trace-mmap --trace-cpu=1 --trace-memory=1 --cpu-interval-ms=11 --memory-interval-ms=10 --force --quiet -o output.bin -m tutorial

memray parse output.bin > tmp

# memray flamegraph -h  # for help

memray flamegraph --trace-memory=1 --trace-allocation-index=100 --filter-boring-frame=0 output.bin -f -o xx.html

#界面：--trace-cpu=1 --trace-allocation-index-from N1 --trace-allocation-index-to N2
#语义：用N1-N2之间的cpu trace画火焰图，这样可以方便观察在一个程序运行的某一阶段时间主要花在哪了
memray flamegraph --trace-cpu=1 -tai-f=1000 -tai-t=2000 --filter-boring-frame=0 output.bin -f -o yy.html

#memray summary --trace-cpu=1 output.bin

#cat tmp |grep -i ALLOCATION_WITH_NATIVE | wc -l
#cat tmp |grep -i CPU_SAMPLE_WITH_NATIVE | wc -l