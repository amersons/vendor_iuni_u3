#!/bin/bash

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

# Script to interpret results of iommu tracepoints
# Note: Directory this script is run in must contain Python
# script that returns statistics

DEFAULT_TEST_CASE="sh /data/kernel-tests/memory_prof.sh -e"

usage()
{
    echo "Usage: $(basename $0) [test_case]"
    echo
    echo "test_case is a command to be run with \"adb shell\""
    echo "and defaults to \"$DEFAULT_TEST_CASE\"."
}

[[ "$1" = "-h" || "$1" = "--help" ]] && { usage; exit 1; }

DEBUGFS_ROOT=${DEBUGFS_ROOT:-/sys/kernel/debug}
TRACING_ROOT=${TRACING_ROOT:-${DEBUGFS_ROOT}/tracing}
EVENTS_ROOT=${EVENTS_ROOT:-${TRACING_ROOT}/events/kmem}

# Mount Debug Filsystem
adb wait-for-device
adb root
adb wait-for-device
adb shell "mkdir -p ${DEBUGFS_ROOT}"
adb shell "mount -t debugfs nodev ${DEBUGFS_ROOT}"
echo "INFO: Mounted Debug Filesystem"

# Enable Tracing
adb shell "echo function > ${TRACING_ROOT}/current_tracer"
adb shell "echo 1 > ${EVENTS_ROOT}/alloc_pages_iommu_end/enable"
adb shell "echo 1 > ${EVENTS_ROOT}/alloc_pages_iommu_start/enable"
adb shell "echo 1 > ${EVENTS_ROOT}/alloc_pages_iommu_fail/enable"
adb shell "echo 1 > ${EVENTS_ROOT}/alloc_pages_sys_end/enable"
adb shell "echo 1 > ${EVENTS_ROOT}/alloc_pages_sys_start/enable"
adb shell "echo 1 > ${EVENTS_ROOT}/alloc_pages_sys_fail/enable"
adb shell "echo 1 > ${EVENTS_ROOT}/iommu_map_range/enable"
echo "INFO: Enabled Ion/Iommu Tracing"

# Execute Some Test
if [ -z "$1" ]; then
	KERNELTEST=$DEFAULT_TEST_CASE
else
	KERNELTEST=$1
fi
echo "INFO: Running $KERNELTEST"
adb shell "$KERNELTEST"

# Pull
echo "INFO: Pulling Trace File into Current Directory"
rm -vf trace
adb pull ${TRACING_ROOT}/trace

# Disable Tracing
adb shell "echo 0 > ${EVENTS_ROOT}/enable"
adb shell "echo nop > ${TRACING_ROOT}/current_tracer"
echo "INFO: Disabled Ion/Iommu Tracing"

# Invoke Python Script
echo "INFO: Gathering Trace Statistics..."
./stats.py
rm -vf trace
echo "Done"
