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

import sys
import os
import fileinput
import select
from optparse import OptionParser

ion_heaps = {
    "INVALID_HEAP_ID": {'value': -1, 'short': 'invalid'},
    "ION_CP_MM_HEAP_ID": {'value': 8, 'short': "mm"},
    "ION_CP_MFC_HEAP_ID": {'value': 12, 'short': "mfc"},
    "ION_CP_WB_HEAP_ID": {'value': 16, 'short': "wb"},
    "ION_CAMERA_HEAP_ID": {'value': 20, 'short': "camera_preview"},
    "ION_SYSTEM_CONTIG_HEAP_ID": {'value': 21, 'short': "kmalloc"},
    "ION_ADSP_HEAP_ID": {'value': 22, 'short': "adsp"},
    "ION_PIL1_HEAP_ID": {'value': 23, 'short': "pil_1"},
    "ION_SF_HEAP_ID": {'value': 24, 'short': "sf"},
    "ION_IOMMU_HEAP_ID": {'value': 25, 'short': "iommu"},
    "ION_PIL2_HEAP_ID": {'value': 26, 'short': "pil_2"},
    "ION_QSECOM_HEAP_ID": {'value': 27, 'short': "qsecom"},
    "ION_AUDIO_HEAP_ID": {'value': 28, 'short': "audio"},
    "ION_MM_FIRMWARE_HEAP_ID": {'value': 29, 'short': "mm_fw"},
    "ION_SYSTEM_HEAP_ID": {'value': 30, 'short': "vmalloc"},
    "ION_HEAP_ID_RESERVED": {'value': 31, 'short': "reserved"},
}

# flags
ion_flags = {
    "ION_FLAG_CACHED": 1,
    "ION_FLAG_CACHED_NEEDS_SYNC": 2,
    "ION_FLAG_SECURE": (1 << ion_heaps['ION_HEAP_ID_RESERVED']['value']),
    "ION_FLAG_FORCE_CONTIGUOUS": (1 << 30),
}

# some deprecated flags:
ion_flags["ION_FORCE_CONTIGUOUS"] = ion_flags["ION_FLAG_FORCE_CONTIGUOUS"]
ion_flags["ION_SECURE"] = ion_flags["ION_FLAG_SECURE"]

# look up flag name from value:
ion_flags_reverse_map = dict(zip(ion_flags.values(), ion_flags.keys()))

def die(msg):
    print msg
    sys.exit(1)

def squeeze_spaces(st):
    return ' '.join(st.split())

def convert_flags(flags_string):
    flags = int(flags_string.split('=')[1], 16)
    ret = []
    for i in range(31):
        test_bit = (1 << i)
        if test_bit & flags:
            if test_bit not in ion_flags.values():
                die("Unknown flag bit set: bit %d, (%x)" % (i, test_bit))
            ret.append(ion_flags_reverse_map[test_bit])
    return '|'.join(ret) if len(ret) > 0 else '0'

def convert_heap_id(heap_id_string):
    heap_id_string = heap_id_string.split('=')[1]
    for heap_id, heap in ion_heaps.iteritems():
        if heap['short'] == heap_id_string:
            return heap_id
    die("Unknown heap_name: %s" % heap_name)


def next_is_fallback(lines, idx):
    idx_to_check = idx + 1
    if len(lines) > idx_to_check:
        return squeeze_spaces(lines[idx_to_check]).split(' ')[4] == 'ion_alloc_buffer_fallback'
    return False

def convert_alloc_size(size_string):
    size = int(size_string.split('=')[1])
    return size

def trace_2_alloc_profile(lines, quiet_on_failure, profile_mmap, profile_memset):
    for idx,line in enumerate(lines):
        if line.startswith("#"):
            continue
        l = squeeze_spaces(line)
        fields = l.split(' ')
        (task_pid, cpu, flags, timestamp, function) = fields[:5]
        if function != 'ion_alloc_buffer_end:' or next_is_fallback(lines, idx):
            continue
        (client_name, heap_name, length, mask, flags) = fields[5:]
        size = convert_alloc_size(length)
        data = {
            'op': 'alloc',
            'reps': 1,
            'heap_id': convert_heap_id(heap_name),
            'flags': convert_flags(flags),
            'alloc_size_bytes': size,
            'alloc_size_label': size, # TODO: prettify (e.g. "2MB")
            'quiet_on_failure': 'true' if quiet_on_failure else 'false',
            'profile_mmap': 'true' if profile_mmap else 'false',
            'profile_memset': 'true' if profile_memset else 'false',
        }
        print '#', line
        print "%(op)s,%(reps)d,%(heap_id)s,%(flags)s,0x%(alloc_size_bytes)X,0x%(alloc_size_label)X,%(quiet_on_failure)s,%(profile_mmap)s,%(profile_memset)s" % data

if __name__ == "__main__":
    usage = """Usage: %prog [options] [tracefile]

Converts a trace file into an allocation profile (format suitable to
feed into `memory_prof -e -i'.

You must either provide an input file on the command line or pipe it
in on stdin."""
    parser = OptionParser(usage=usage)
    parser.add_option("--quiet-on-failure", action="store_true",
                      help="Make allocations quiet on errors")
    parser.add_option("--profile-mmap", action="store_true",
                      help="Also emit commands to profile mmap")
    parser.add_option("--profile-memset", action="store_true",
                      help="Also emit commands to profile memset")

    (options, args) = parser.parse_args()

    # check for some data or an input file
    if not ((len(args) == 1 and os.path.isfile(args[0])) or \
            select.select([sys.stdin], [], [], 0.0)[0]):
        parser.print_usage()
        sys.exit(1)

    # snarf:
    lines = [line.rstrip("\n") for line in fileinput.input(args)]

    if len(lines) == 0:
        parser.print_usage()
        sys.exit(1)

    trace_2_alloc_profile(lines,
                          options.quiet_on_failure,
                          options.profile_mmap,
                          options.profile_memset)
