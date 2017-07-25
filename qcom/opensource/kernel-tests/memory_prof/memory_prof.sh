#!/bin/sh

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

cd $(dirname $0 2>/dev/null || echo "/data/kernel-tests")

chmod 755 memory_prof

maybe_make_node()
{
	thedev=$1
	themisc=$2
	[ -e $thedev ] || \
	    mknod $thedev c $(cut -d: -f1 $themisc) $(cut -d: -f2 $themisc)
}

if [ -d /system/lib/modules/ ]; then
	modpath=/system/lib/modules
else
	modpath=/kernel-tests/modules/lib/modules/$(uname -r)/extra
fi

memory_prof_mod=${modpath}/the_memory_prof_module.ko

memory_prof_dev=/dev/memory_prof
memory_prof_dev_sys=/sys/class/memory_prof/memory_prof/dev
ion_dev=/dev/ion
ion_dev_misc=/sys/class/misc/ion/dev

# create ion device if it doesn't exist
maybe_make_node $ion_dev $ion_dev_misc

# insert memory_prof_mod if needed
if [ ! -e $memory_prof_dev_sys ]; then
	insmod $memory_prof_mod

	if [ $? -ne 0 ]; then
		echo "ERROR: failed to load module $memory_prof_mod"
		exit 1
	fi
fi

# create memory prof device if it doesn't exist
maybe_make_node $memory_prof_dev $memory_prof_dev_sys

./memory_prof $@

rmmod the_memory_prof_module
