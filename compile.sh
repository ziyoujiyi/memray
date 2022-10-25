#vim ~/.local/lib/python3.7/site-packages/easy-install.pth: /home/bwang/memray/src
#vim /home/bwang/.local/lib/python3.7/site-packages/memray.egg-link: /home/bwang/memray/src

rm -rf build/lib.linux-x86_64-cpython-37/memray/_memray.cpython-37m-x86_64-linux-gnu.so
rm -rf build/lib.linux-x86_64-cpython-37/memray/_test_utils.cpython-37m-x86_64-linux-gnu.so

rm -rf /home/bwang/memray/src/memray/_memray.cpp
rm -rf ./src/memray/_test_utils.cpython-37m-x86_64-linux-gnu.so
rm -rf ./src/memray/_memray.cpython-37m-x86_64-linux-gnu.so

python setup.py develop --editable -b build --prefix /home/bwang/.local/ --no-deps

#memray run --native --trace-python-allocators --force -o output.bin -m tutorial
#memray summary --cpu-profiler-switch 1 output.bin
