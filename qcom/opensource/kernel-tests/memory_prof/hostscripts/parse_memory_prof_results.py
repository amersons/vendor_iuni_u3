#!/usr/bin/env python2

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

# Quick and very dirty parser for the output of `memory_prof.sh -e'

# This should be run on the host (not the target) with the output from
# the target piped in (e.g. copy/paste/cat) or passed as a file on the
# command line

# Dependencies: matplotlib, numpy


# NOTE: THIS HAS GOTTEN UNWIELDY. YOU HAVE BEEN WARNED.


import sys
import fileinput
import operator
from itertools import cycle
from optparse import OptionParser

force_text_only = False

try:
    import numpy as np
except:
    print """WARNING: Couldn't import numpy. Please install it to get plots.
One of these should do it:

    sudo apt-get install python-numpy
    sudo yum install numpy
    sudo pip install numpy
    sudo easy_install numpy

For now, we'll just disable plotting and go to text-only mode
"""
    force_text_only = True
else:
    # only try to import matplotlib if we have numpy
    try:
        import matplotlib.pyplot as plt
    except:
        print """WARNING: Couldn't import matplotlib. Please install it to get plots.
One of these should do it:

    sudo apt-get install python-matplotlib
    sudo yum install python-matplotlib
    sudo pip install matplotlib
    sudo easy_install matplotlib

For now, we'll just disable plotting and go to text-only mode
    """
        force_text_only = True

ST_PREFIX_DATA_ROW      = "=> "
ST_PREFIX_PREALLOC_SIZE = "==> "
ST_PREFIX_NUM_REPS      = "===> "

def get_data_lines(lines):
    return [line.lstrip(ST_PREFIX_DATA_ROW).rstrip('\n') for line in lines if line.startswith(ST_PREFIX_DATA_ROW)]

def size_string_to_bytes(st):
    if st.upper().startswith('0X'):
        return int(st, 16)
    multipliers = {
        'K': 1 << 10,
        'M': 1 << 20,
    }
    st = st.upper().replace('KB', 'K').replace('MB', 'M')
    return int(st[:-1]) * multipliers[st[-1]]

def print_table(table, header=False):
    col_widths = {}
    for row in table:
        for idx,cell in enumerate(row):
            col_widths.setdefault(idx, 0)
            col_widths[idx] = max(col_widths[idx], len(str(cell)))
    row_format = ""
    padding = 4
    total_width = 0
    for i,idx in enumerate(sorted(col_widths.keys())):
        row_format += "{%d:>%d}" % (i, col_widths[idx] + padding)
        total_width += col_widths[idx] + padding
    for cnt,row in enumerate(table):
        if header and cnt == 1:
            print "-" * total_width
        print row_format.format(*row)


def plot_against_time(data, end_time, init_time, max_latency, outfile, interactive, text_only):
    # we will put each "alloc config" (e.g. "ION_SYSTEM_HEAP_ID
    # ION_FLAG_CACHED") in its own figure. Within the figure there
    # will be a subplot for each allocation size.
    # x = np.linspace(0.1, 2 * np.pi, 10)
    total_plots = sum([1 for k,item in data.iteritems() for c in item.iterkeys()])
    current_plot = 1
    if not text_only:
        fig = plt.figure(1, figsize=(8, total_plots * 4), dpi=100)
    padding = 100
    timings = {}
    for i, (cfg, sizes) in enumerate(data.iteritems()):
        for j, (sz, alloc_data) in enumerate(sizes.iteritems()):
            timings[alloc_data['time'][0]] = {
                'cfg': cfg,
                'size': sz,
                'average': alloc_data['average'][0],
            }
            if text_only:
                continue
            ax = plt.subplot(total_plots, 1, current_plot)
            ax.set_title(cfg + ' ' + sz)
            ax.grid(True)
            x = [t - init_time + padding for t in alloc_data['time']]
            y = alloc_data['average']
            markerline, stemlines, baseline = ax.stem(x, y, '-.')
            plt.xlim([0, end_time - init_time + padding + padding])
            plt.ylim([0, max_latency])
            plt.setp(markerline, 'markerfacecolor', 'b')
            plt.setp(baseline, 'color', 'r', 'linewidth', 2)
            current_plot += 1

    times = sorted(timings.keys())
    table = [['timestamp', 'heap_id', 'flags', 'size', 'time']]
    for time in times:
        heap, flags = timings[time]['cfg'].split(' ')
        table.append([
            time - times[0],
            heap,
            flags if flags != '0' else "None",
            timings[time]['size'],
            timings[time]['average'],
        ])
    print_table(table, header=True)
    if text_only:
        return
    if outfile:
        plt.savefig(outfile, bbox_inches=0)
    if interactive:
        plt.show()

def plot_against_alloc_size(data, op, outfile, interactive, text_only):
    size_data = {}
    table = []
    for alloc_config,iitem in data.iteritems():
        for size_string,jitem in iitem.iteritems():
            size_data.setdefault(alloc_config, {})
            size_data[alloc_config][size_string] = np.average(jitem["average"])
            heap, flags = alloc_config.split(' ')
            table.append([
                heap,
                flags if flags != '0' else "None",
                size_string,
                size_data[alloc_config][size_string],
            ])

    # sort by heap_ids, then sizes
    table = sorted(table,
                   key=lambda row: (row[0], size_string_to_bytes(row[2])))
    # add the header post sorting
    table.insert(0, ["heap_id", "flags", "size", "time"])

    print_table(table, header=True)

    if text_only:
        return

    fig = plt.figure(figsize=(10, 10), dpi=100)
    ax = fig.add_subplot(111)

    shapes_cycler = cycle(["o", "v", "^" , "<", ">"])

    current_plot = 1
    for alloc_config,item in size_data.iteritems():
        sorted_sizes = sorted(item.keys(), key=size_string_to_bytes)
        lbl = alloc_config
        if lbl.endswith(' 0'):
            lbl = lbl.rstrip('0') + '(No flags)'
        ax.plot([size_string_to_bytes(s) / 1024.0 / 1024.0
                 for s in sorted_sizes],
                [item[s] for s in sorted_sizes],
                next(shapes_cycler) + '-',
                label=lbl)

    title = "%s times" % op

    ax.set_ylabel("Time (ms)")
    ax.set_xlabel("Allocation size (MB)")
    ax.set_title(title)
    ax.legend(loc="upper left")
    plt.grid(True)
    if outfile:
        plt.savefig(outfile, bbox_inches=0)
    if interactive:
        plt.show()


def plot_it(lines, op, xaxis, outfile, interactive, text_only):
    """Plots some stuff.

    - lines: the data

    - op: the operation to plot (one of 'ION_IOC_ALLOC', 'mmap',
      'memset', 'ION_IOC_FREE')

    - xaxis: Sets what the x-axis will be for this plot. One of 'time'
      or 'alloc_size'. When set to 'alloc_size' all of the allocations
      for each (heap_id,flags) pair will be aggregated per-size before
      plotting. When set to 'time' we just plot all of the data
      against time.
    """
    data = {}
    init_time = None
    end_time = 0
    max_latency = 0
    for line in get_data_lines(lines):
        # for the line format, see print_stats_results in
        # memory_prof.c
        (time,
         ion_op,
         heap_id,
         caching,
         alloc_size,
         lbl_av,
         average,
         lbl_std,
         std_dev,
         lbl_reps,
         reps) = line.split(' ')

        if ion_op != op: continue

        alloc_config = ' '.join([heap_id, caching])

        time = int(time)
        average = float(average)
        std_dev = float(std_dev)
        reps = int(reps)

        if init_time is None:
            init_time = time
        else:
            init_time = min(init_time, time)

        end_time = max(end_time, time)
        max_latency = max(max_latency, average)

        # there must be a cleaner way of setting up a nested dict
        data.setdefault(alloc_config, {})
        data[alloc_config].setdefault(alloc_size, {})
        data[alloc_config][alloc_size].setdefault("time", [])
        data[alloc_config][alloc_size].setdefault("average", [])
        data[alloc_config][alloc_size].setdefault("std_dev", [])
        data[alloc_config][alloc_size].setdefault("reps", [])
        data[alloc_config][alloc_size]["time"].append(time)
        data[alloc_config][alloc_size]["average"].append(average)
        data[alloc_config][alloc_size]["std_dev"].append(std_dev)
        data[alloc_config][alloc_size]["reps"].append(reps)

    if xaxis == 'time':
        plot_against_time(data, end_time, init_time, max_latency, outfile, interactive, text_only)
    elif xaxis == 'alloc_size':
        plot_against_alloc_size(data, op, outfile, interactive, text_only)


if __name__ == "__main__":
    usage = """Usage: %prog <outputmode> [options]

where outputmode is one of: """ + ', '.join(["--interactive", "--outfile", "--text-only"])
    parser = OptionParser(usage=usage)
    parser.add_option("-t", "--text-only", action="store_true")
    parser.add_option("--target", help="Name of the device (used for plot titles)")
    parser.add_option("-p", "--ion-op",
                      default="ION_IOC_ALLOC",
                      help="Ion (etc) operation to display (currently supported: ION_IOC_ALLOC, mmap, memset, ION_IOC_FREE)")
    parser.add_option("-x", "--x-axis",
                      default="alloc_size",
                      help="The x-axis to be used for the plot. One of 'time' or 'alloc_size'.")
    parser.add_option("-o", "--outfile",
                      help="Filename to save the plots to (e.g. output.png, output.svg, etc.)")
    parser.add_option("-i", "--interactive", action="store_true",
                      help="Use the interactive viewer")

    (options, args) = parser.parse_args()

    valid_x_axes = ['time', 'alloc_size']
    if not options.x_axis in valid_x_axes:
        print 'Invalid x-axis requested:', options.xaxis
        print 'You should supply one of', ' '.join(valid_x_axes)
        sys.exit(1)

    if not options.outfile and not options.interactive and not options.text_only:
        print "Nothing to do."
        print 'You must specify one of: %s' % ', '.join(["--interactive", "--outfile", "--text-only"])
        sys.exit(1)

    # snarf:
    lines = [line for line in fileinput.input(args)]

    plot_it(lines,
            options.ion_op,
            options.x_axis,
            options.outfile,
            options.interactive,
            options.text_only or force_text_only)
