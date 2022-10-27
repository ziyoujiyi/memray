<<'COMMENT'
Firstly, please ensure it is in gsim env.
1. Install node js env
   - cd ~ && mkdir nodeenv
   - cd nodeenv
   - curl -LJO https://nodejs.org/dist/v18.12.0/node-v18.12.0-linux-x64.tar.xz
   - tar xvf node-v18.12.0-linux-x64.tar.xz
   - export PATH=~/nodeenv/node-v18.12.0-linux-x64/bin:$PATH
   #follow https://webpack.js.org/guides/installation/#local-installation
   - npm config set registry http://registry.npm.taobao.org
   - npm install --save-dev webpack
   - export PATH=~/nodeenv/node-v18.12.0-linux-x64/node_modules/.bin:$PATH
   #if you update package.json and package-loc.json, please excute 'npm install' in memray(repo) root dir before webpack
   - cd ~/memray && webpack

2. Compile develop version
   - vim ~/.local/lib/python3.7/site-packages/easy-install.pth && add '/home/bwang/memray/src'
   - vim ~/.local/lib/python3.7/site-packages/memray.egg-link && add '/home/bwang/memray/src'
   - sh compile.sh

3. How to use?
   - support memory profiler, use 'memray -h' for any help
   - support cpu profiler(only summary / flamegraph)
   - examples:
    #memray run --native --trace-python-allocators --force -o output.bin -m tutorial
    #memray summary --cpu-profiler-switch 1 output.bin
    #memray flamegraph --cpu-profiler-switch 1 output.bin -f -o yy.html

4. Develop guides
   - python log tools: logger, c/c++ log tools: MY_DEBUG
COMMENT

rm -rf build/lib.linux-x86_64-cpython-37/memray/_memray.cpython-37m-x86_64-linux-gnu.so
rm -rf build/lib.linux-x86_64-cpython-37/memray/_test_utils.cpython-37m-x86_64-linux-gnu.so

rm -rf /home/bwang/memray/src/memray/_memray.cpp
rm -rf ./src/memray/_test_utils.cpython-37m-x86_64-linux-gnu.so
rm -rf ./src/memray/_memray.cpython-37m-x86_64-linux-gnu.so

python setup.py develop --editable -b build --prefix /home/bwang/.local/ --no-deps
