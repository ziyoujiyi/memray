from functools import partial
from re import A
import gsim.preset
from aleph import logging
from gsim import lib

import numpy as np
#import sys


def f(ticker, x, t0, t1):

    o = np.nanmean(x, axis=0)
    for i in range(1000):
        o -= np.nanmean(o)
    o /= np.nanstd(o)
    logging.info(f"t0: {t0.now()}, t1: {t1.now()}, {o}")
    return o + 0.5

def gsim_main():
    common = gsim.preset.Basic(from_date="20210104", to_date="20210110")
    common.create_tasks()
    D = common.get_data_bundle()
    x = D["30min/Return"]
    x = lib.tsMat(x,90)
    ticker = x.gsim_kernel_instrument_axis.ticker
    t0 = common.t_dict["eod"]
    t1 = common.t_dict["30min"]
    y = lib.applyFollowFirst(partial(f, ticker), x, t0, t1)
    t_y = common.save_as_ts(y)
    common.unamed.append(y)
    #common.run(progress=True)
    common.run(progress=False)

def test1():
	allocate_mem_sz = 1 * 1024 * 1024
	a = [i for i in range(allocate_mem_sz)]
	#for i in range(allocate_mem_sz):
	#	a.append(i)
	allocate_mem_sz_2 = 1 * 1024 * 1024
	del_mem_sz = (int)(allocate_mem_sz / 2)
	del a[del_mem_sz: ]
	#import gc
	#gc.collect()
	for i in range(allocate_mem_sz_2):
		a.append(i)


def test4():
	a = []
	a.append(10)

def test2():
	for i in range(1 * 102 * 1024):
		test4()

	a = []
	for i in range(1 * 102 * 1024):
		a.append(i)

def test3():
	for i in range(1 * 102 * 1024):
		test4()

	a = []
	for i in range(1 * 102 * 1024):
		a.append(i)

"""
x = np.ones((1,1))
print(sys.getsizeof(x) / 1048576)

x = np.ones((1000,1000))
print(sys.getsizeof(x) / 1048576)

x = np.ones((1000,2000))
print(sys.getsizeof(x) / 1048576)

x = np.ones((1000,20000))
print(sys.getsizeof(x) / 1048576)
"""
def allocate():
	#test1()
	test2()
	test3()
	"""
	for i in range(100):
		x = np.ones((1000,1000))
		x = np.ones((1,1))
		x = np.ones((1,1))
		x = np.ones((1,1))
		x = np.ones((1000,2000))
		x = np.ones((1,1))
		x = np.ones((1,1))
		x = np.ones((1,1))
		x = np.ones((1000,20000))
		x = 1
		x += 1
		x += 1
		x += 1
	"""

import time
if __name__ == "__main__":
	start_time = time.time()
	gsim_main()
	end_time = time.time()
	elapsed_time = end_time - start_time
	print("gsim time cost: {}".format(elapsed_time))
