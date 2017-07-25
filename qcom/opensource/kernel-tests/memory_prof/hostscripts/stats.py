#!/usr/bin/env python

# Copyright (c) 2013, The Linux Foundation. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
#     * Redistributions of source code must retain the above copyright
#       notice, this list of conditions and the following disclaimer.
#     * Redistributions in binary form must reproduce the above
#       copyright notice, this list of conditions and the following
#       disclaimer in the documentation and/or other materials provided
#       with the distribution.
#     * Neither the name of The Linux Foundation nor the names of its
#       contributors may be used to endorse or promote products derived
#       from this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED "AS IS" AND ANY EXPRESS OR IMPLIED
# WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
# MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT
# ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS
# BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
# CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
# SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
# BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
# WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
# OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
# IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

import string
import re

def create_dict(str):
	"""
	takes key-value pairs contained in string
	and returns a dictionary of KVP's
	"""
	return dict(re.findall(r'(\S+)=(\S+)', str))

def return_ratio(prop, total):
	"""
	returns the percentage, or zero if denominator
	is zero
	"""
	return str(round(((float(prop)/float(total))
		*100), 3)) if (total != 0) else str(0)

class Incrementor(object):
	"""
	class for "counters" to simply
	incrementing and retrieving values
	"""
	def __init__(self, start):
		self.count=start
	def __call__(self, jump=1):
		self.count += jump
		return self.count

class MapEntry(object):
	def __init__(self, vals):
		self.v_addr=vals['v_addr']
		self.p_addr=vals['p_addr']
		self.chunk_size=vals['chunk_size']
		self.length=int(vals['len'])

class AllocEntry(object):
	def __init__(self, vals):
		self.gfp_flags=vals['gfp_flags']
		self.order=int(vals['order'])

class MapStat(object):
	def __init__(self):
		self.total_calls = Incrementor(0)
		self.size_sixteenm = Incrementor(0)
		self.size_onem = Incrementor(0)
		self.size_sixtyfourk = Incrementor(0)
		self.size_fourk = Incrementor(0)
	def total(self):
		return str(self.total_calls(0))
	def SM(self):
		return str(self.size_sixteenm(0))
	def OM(self):
		return str(self.size_onem(0))
	def SFK(self):
		return str(self.size_sixtyfourk(0))
	def fk(self):
		return str(self.size_fourk(0))
	def SM_ratio(self):
		return return_ratio(self.size_sixteenm(0), self.total_calls(0))
	def OM_ratio(self):
		return return_ratio(self.size_onem(0), self.total_calls(0))
	def SFK_ratio(self):
		return return_ratio(self.size_sixtyfourk(0), self.total_calls(0))
	def fk_ratio(self):
		return return_ratio(self.size_fourk(0), self.total_calls(0))

class AllocStat(object):
	def __init__(self):
		self.total_heap_alloc_calls = Incrementor(0)
		self.failed_heap_allocs = Incrementor(0)
		self.heap_alloc_order_zero = Incrementor(0)
		self.heap_alloc_order_four = Incrementor(0)
		self.heap_alloc_order_eight = Incrementor(0)
	def total(self):
		return str(self.total_heap_alloc_calls(0))
	def fail(self):
		return str(self.failed_heap_allocs(0))
	def zero(self):
		return str(self.heap_alloc_order_zero(0))
	def four(self):
		return str(self.heap_alloc_order_four(0))
	def eight(self):
		return str(self.heap_alloc_order_eight(0))
	def fail_ratio(self):
		return return_ratio(self.failed_heap_allocs(0), self.total_heap_alloc_calls(0))
	def zero_ratio(self):
		return return_ratio(self.heap_alloc_order_zero(0), self.total_heap_alloc_calls(0))
	def four_ratio(self):
		return return_ratio(self.heap_alloc_order_four(0), self.total_heap_alloc_calls(0))
	def eight_ratio(self):
		return return_ratio(self.heap_alloc_order_eight(0), self.total_heap_alloc_calls(0))

class Stats(object):
	def __init__(self):
		self.map_calls = MapStat()
		self.iommu = AllocStat()
		self.sys = AllocStat()
		with open('trace', 'r') as trace_file:
			for line in trace_file:
				self.process_line(line)

	def process_line(self, line):
		index=string.find(line, "iommu_map_range:")
		if index >= 0:
			return self.iommu_map_range(create_dict(line))
		index=string.find(line, "alloc_pages_iommu_start:")
		if index >= 0:
			return self.alloc_pages_iommu_start(create_dict(line))
		index=string.find(line, "alloc_pages_iommu_fail:")
		if index >= 0:
			return self.alloc_pages_iommu_fail(create_dict(line))
		index=string.find(line, "alloc_pages_sys_start:")
		if index >= 0:
			return self.alloc_pages_sys_start(create_dict(line))
		index=string.find(line, "alloc_pages_sys_fail:")
		if index >= 0:
			return self.alloc_pages_sys_fail(create_dict(line))

	def iommu_map_range(self, vals):
		stat = MapEntry(vals)
		self.map_calls.total_calls()
		if stat.length==(1 << 24):
			self.map_calls.size_sixteenm()
		elif stat.length==(1 << 20):
			self.map_calls.size_onem()
		elif stat.length==(1 << 16):
			self.map_calls.size_sixtyfourk()
		elif stat.length==(1 << 12):
			self.map_calls.size_fourk()
		else:
			print "Unsupported Length: " + str(stat.length)

	def alloc_pages_iommu_start(self, vals):
		stat = AllocEntry(vals)
		self.iommu.total_heap_alloc_calls()
		if stat.order == 0:
			self.iommu.heap_alloc_order_zero()
		elif stat.order == 4:
			self.iommu.heap_alloc_order_four()
		elif stat.order == 8:
			self.iommu.heap_alloc_order_eight()
		else:
			print "Unsupported Order: " + str(stat.order)

	def alloc_pages_iommu_fail(self, vals):
		stat = AllocEntry(vals)
		self.iommu.failed_heap_allocs()

	def alloc_pages_sys_start(self, vals):
		stat = AllocEntry(vals)
		self.sys.total_heap_alloc_calls()
		if stat.order == 0:
			self.sys.heap_alloc_order_zero()
		elif stat.order == 4:
			self.sys.heap_alloc_order_four()
		else:
			self.sys.heap_alloc_order_eight()

	def alloc_pages_sys_fail(self, vals):
		stat = AllocEntry(vals)
		self.sys.failed_heap_allocs()

	def return_results(self):
		print ''.join(["Total Calls to Map Range: ", self.map_calls.total()])
		print ''.join(["Mapped Pages of Size 16MB: ",
			self.map_calls.SM(), "(", self.map_calls.SM_ratio(), "%)"])
		print ''.join(["Mapped Pages of Size 1MB: ",
			self.map_calls.OM(), "(", self.map_calls.OM_ratio(), "%)"])
		print ''.join(["Mapped Pages of Size 64KB: ",
			self.map_calls.SFK(), "(", self.map_calls.SFK_ratio(), "%)"])
		print ''.join(["Mapped Pages of Size 4KB: ",
			self.map_calls.fk(), "(", self.map_calls.fk_ratio(), "%)"])
		print "---"
		print ''.join(["Total Calls to Alloc in iommu Heap: ", self.iommu.total()])
		print ''.join(["Failed iommu Heap Allocations: ",
			self.iommu.fail(),"(", self.iommu.fail_ratio(),"%)"])
		print ''.join(["iommu Heap Allocations of Order Zero: ",
			self.iommu.zero(), "(", self.iommu.zero_ratio(), "%)"])
		print ''.join(["iommu Heap Allocations of Order Four: ",
			self.iommu.four(), "(", self.iommu.four_ratio(), "%)"])
		print ''.join(["iommu Heap Allocations of Order Eight: ",
			self.iommu.eight(), "(", self.iommu.eight_ratio(), "%)"])
		print "---"
		print ''.join(["Total Calls to Alloc in sys Heap: ", self.sys.total()])
		print ''.join(["Failed sys Heap Allocations: ",
			self.sys.fail(),"(", self.sys.fail_ratio(),"%)"])
		print ''.join(["sys Heap Allocations of Order Zero: ",
			self.sys.zero(), "(", self.sys.zero_ratio(), "%)"])
		print ''.join(["sys Heap Allocations of Order Four: ",
			self.sys.four(), "(", self.sys.four_ratio(), "%)"])
		print ''.join(["sys Heap Allocations of Order Eight: ",
			self.sys.eight(), "(", self.sys.eight_ratio(), "%)"])

if __name__ == '__main__':
	stat = Stats()
	stat.return_results()
