# Copyright (c) 2012, The Linux Foundation. All rights reserved.
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


chmod 755 ocmem_test

if [ -d /system/lib/modules/ ]; then
	modpath=/system/lib/modules
else
	modpath=/kernel-tests/modules/lib/modules/$(uname -r)/extra
fi

ocmem_test_mod=${modpath}/msm_ocmem_test_module.ko
ocmem_test_dev_misc=/sys/class/misc/ocmemtest/dev
ocmem_test_dev=/dev/ocmemtest

CMD_LINE="$@"
NUM_ARGS=$#

maybe_make_node()
{
	thedev=$1
	themisc=$2
	[ -e $thedev ] || \
	    mknod $thedev c $(cut -d: -f1 $themisc) $(cut -d: -f2 $themisc)
}

#Parse the args for valid switches
while [ $# -gt 0 ]; do
        case $1 in
	-n | --nominal)
		shift 1
		;;
	-a | --adversarial)
		shift 1
		;;
	-r | --repeatability)
		shift 1
                if [ "$1" -eq "$1" ] ; then
                        echo "Specified $1 iterations"
                        shift 1
                else
                        echo "Invalid number of iterations"
                        exit 1
                fi
		;;
	-s | --stress)
		shift 1
		;;
	-v | --verbose)
		shift 1
		;;
	-h | --help | *)
		exit 1
		;;
        esac
done

# insert the msm_ocmem_test_module if needed
if [ ! -e $ocmem_test_dev_misc ]; then
	insmod $ocmem_test_mod

	if [ $? -ne 0 ]; then
		echo "ERROR: failed to load module $ocmem_test_mod"
	exit 1
	fi
fi

# create the ocmem test device if it doesn't exist
maybe_make_node $ocmem_test_dev $ocmem_test_dev_misc

# Execute the tests
# Run nominal test case by default if there are no args

if [ $NUM_ARGS -ne 0 ]; then
	./ocmem_test $CMD_LINE
else
	echo "Running nominal tests (default)"
	./ocmem_test -n
fi

num_failures=$?

# Check for pass or fail status
if [ $num_failures -ne 0 ]; then
    echo "OCMEM tests failed ($num_failures)"
    rc=1
else
    echo "OCMEM tests passed"
    rc=0
fi

echo "Unloading $MSM_OCMEM_TEST_MODULE.ko"
rmmod $ocmem_test_mod

exit $rc
