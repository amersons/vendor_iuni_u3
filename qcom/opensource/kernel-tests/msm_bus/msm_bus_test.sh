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



testpath=/data/kernel-tests
#
if [ -d /system/lib/modules/ ]; then
	modpath=/system/lib/modules
else
	modpath=/data
fi

msm_bus_ioctl=${modpath}/msm_bus_ioctl.ko
msm_bus_test=${testpath}/msm_bus_test
chmod 777 $msm_bus_test

CMD_LINE="$@"
NUM_ARGS=$#

# insert the msm_bus_ioctl.ko
insmod $msm_bus_ioctl
if [ $? -ne 0 ]; then
	echo "ERROR: failed to load module $msm_bus_ioctl"
	sleep 2
	exit 1
else
	echo "Inserted msm_bus_ioctl module\n"
fi

# Execute the tests
# Run nominal test case by default if there are no args

if [ $NUM_ARGS -ne 0 ]; then
	$msm_bus_test $CMD_LINE
else
	echo "Running nominal tests (default)"
	$msm_bus_test -n
fi

num_failures=$?

# Check for pass or fail status
if [ $num_failures -ne 0 ]; then
    echo "MSM_BUS tests failed ($num_failures)"
    rc=1
else
    echo "MSM_BUS tests passed"
    rc=0
fi

echo "Unloading $MSM_BUS_IOCTL.ko"
rmmod $msm_bus_ioctl

exit $rc
