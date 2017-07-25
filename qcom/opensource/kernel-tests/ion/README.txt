Test: iontest

Usage: iontest [OPTIONS]...

OPTIONS can be (defaults in parenthesis):
  -n, --nominal		nominal test cases
  -a, --adversarial	adversarial test cases
  -V			run with debug messages on (off)

Description:
Ion is a memory allocator used by multimedia clients.
This test simulates a ion client to exercise the allocator framework
The ion framework provides, kernel space and user space interfaces.
Ion apis : linux/msm_ion.h and linux/ion.h

For running tests, the iontest.sh script should be used.
The script loads msm_ion_test_module kernel module.
This module is used to drive kernel space ion test cases.

Target support: 8960, 8930, 8064, 8974
