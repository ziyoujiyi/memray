/share/cnliao/software/gsim/v0.4.0/bin/activate.sh

/share/cnliao/software/gsim/7ca4c5e/v0.4.0/lib/python3.7/site-packages/gsim/ns/feature.py

ipython

source aaa/bin/activate py37

python setup.py install --prefix=/home/bwang/.local/

ctrl + R: 查看 shell 中断最近敲的命令

develop 模式安装 python 包：python setup.py develop --editable -b build --prefix /home/bwang/.local/ --no-deps

* for scalene:
vim /home/bwang/.local/lib/python3.7/site-packages/scalene.egg-link：/home/bwang/scalene

* for memray:
- `vim ~/.local/lib/python3.7/site-packages/easy-install.pth` 加入 `/home/bwang/memray/src` 或者
- `vim /home/bwang/.local/lib/python3.7/site-packages/memray.egg-link` 加入 `/home/bwang/memray/src`
main.py 如何编译发布为 exe 文件？ PyInstaller

python -c 'import sys; print(sys.path)'
breakpoint()

```
In [5]: import scalene.__main__
In [6]: scalene.__main__
```

查看 linux 系统调用，如 man setitimer

pip install --user XX==0.0.0

python -m pdb -m memray run test.py

gdb 调试 python 进程：
```
gdb python
r -m scalene test.py
```

```
In [1]: import memray
In [2]: memray.__path__
Out[2]: ['/home/bwang/.local/lib/python3.7/site-packages/memray']
```

.pyx 文件类似于C 语言的.c 源代码文件, .pxd 文件类似于C 语言的.h 头文件

memray 源码安装：python setup.py build_ext --inplace

python3 -m memray summary output.bin

pdb 调试： interact + （ctrl + d）

cat tmp |grep -i CPU_RECORD | wc -l
jobs -l
kill %1
ps aux | grep bwang
htop -u bwang -t
gdb python -p xxx 调试正在运行的进程
watch -d cat /proc/softirqs 命令查看每个软中断类型的中断次数的变化速率

cat tmp |grep -i 'total processed' | wc -l
cat tmp |grep -i 'total used node num' | wc -l
