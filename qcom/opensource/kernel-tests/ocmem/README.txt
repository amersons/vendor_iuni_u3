Test: ocmem_test

Usage steps:

# On android:
adb shell
cd /data/kernel-tests
sh ocmem_test.sh [OPTIONS]

# On kdev
/kernel-test/ocmem/run.sh [OPTIONS]

OPTIONS can be:
  -n OR --nominal				Nominal test (Run by default)
  -a OR --adversarial				Adversarial tests
  -r OR --repeatability [Number of iterations]	Repeatability tests
  -v OR --verbose				Run with debug messages enabled

Description:

Nominal Test:
1. Invoke all OCMEM APIs used by performance mode clients.
2. Invoke all OCMEM APIs used by low power clients.

Adversarial Tests:
1. Invoke all invalid combinations of OCMEM APIs.

Repeatability Tests:
1. Run nominal and adversarial test cases for number of iterations
specified or a maximum of 10 iterations.

Stress tests:
To be defined.

Targets supported: 8974
