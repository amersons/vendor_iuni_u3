memory_prof is a suite of tools for profiling memory-related
performance as well as catching regressions. The suite is named after
the original memory_prof program, but now includes other programs and
scripts that can be used for memory profiling.


                                                                  __
                                                                 / _|
 _ __ ___   ___ _ __ ___   ___  _ __ _   _       _ __  _ __ ___ | |_
| '_ ` _ \ / _ \ '_ ` _ \ / _ \| '__| | | |     | '_ \| '__/ _ \|  _|
| | | | | |  __/ | | | | | (_) | |  | |_| |     | |_) | | | (_) | |
|_| |_| |_|\___|_| |_| |_|\___/|_|   \__, |     | .__/|_|  \___/|_|
                                      __/ |_____| |
                                     |___/______|_|

Usage: See memory_prof -h

Description:

    These tests are useful for catching performance regressions in Ion or
    general memory code (using the -e and -k options). They can also catch
    other Ion regressions by performing some basic sanity tests (the -b,
    -m, and -l options).

Notes:

    This test suite is accompanied by a kernel module that must be
    inserted for certain test cases (namely -k and -m). The memory_prof.sh
    script will take care of inserting the kernel module and running the
    memory_prof binary for you. However, sometimes it's useful to be able
    run the memory_prof binary directly without inserting the kernel
    module.

    Information about the format of allocation profiles (specified
    with -i) can be found at the end of this document in Appendix A.

Target support: All


                           __               _
                          / _|             | |
 _ __ ___   ___ _ __ ___ | |_ ___  __ _ ___| |_
| '_ ` _ \ / _ \ '_ ` _ \|  _/ _ \/ _` / __| __|
| | | | | |  __/ | | | | | ||  __/ (_| \__ \ |_
|_| |_| |_|\___|_| |_| |_|_| \___|\__,_|___/\__|

Usage: See memfeast -h

Description:

    memfeast reliably and predictably forces the system into a
    low-memory condition.

Target support: All





-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*


Appendix A: Allocation Profiles for memory_prof

    The heap profiling test (-e) has support for custom "allocation
    profile" input files (specified with -i). The format of the
    allocation profile file is a comma-separated values file with the
    following columns:

        op
        [rest...]

    The `op' field specifies what kind of operation this line
    holds. Should be `alloc', `sleep', or `flush'. The remaining
    fields ([rest...])  are defined differently for different values
    of `op'.

    In all cases, all defined fields are *required* and cannot be left
    empty (e.g. `something,,other'). For example, if you don't have
    any flags to pass for alloc, put a 0 rather than leaving it blank.

    o `op' == alloc

      When `op' == alloc, an ION_IOC_ALLOC/ION_IOC_FREE will be
      performed and profiled. Additionally, mmap'ing and memset'ing
      the buffer will be performed and profiled if profile_mmap and
      profile_memset are set.

      The following remaining fields are defined:

          reps
          heap_id
          flags
          alloc_size_bytes
          alloc_size_label
          quiet_on_failure
          profile_mmap
          profile_memset

      - reps :: How many times to repeat this allocation

      - heap_id :: Heap to use for allocation. Should correspond to a
                   heap_id from `enum ion_heap_ids'. E.g.:
                   ION_CP_MM_HEAP_ID

      - flags :: Flags to be used for allocation. Can parse bitwise OR'd
                 ION_FLAG_* constants (e.g.:
                 ION_SECURE|ION_FLAG_CACHED). No spaces please.

      - alloc_size_bytes :: The number of bytes to be allocated. This
           field can be formatted in octal, decimal, or hex, but '0' and
           '0x' prefixes must be used for octal and hex,
           respectively. See STRTOL(3) where base == 0.

      - alloc_size_label :: A human- (and script-) readable string
           describing the allocation size

      - quiet_on_failure :: Whether we should print an error message if
           this allocation fails

      - profile_mmap :: Whether we should profile mmap

      - profile_memset :: Whether we should profile memset

      Blank lines and lines beginning with '#' are skipped.

      See alloc_profiles/general.txt for a full example.


    o `op' == sleep

      When `op' == sleep, a usleep will be inserted with the specified
      number of microseconds.

      The following remaining fields are defined:

          time_us

      - time_us :: The time (in microseconds) to sleep
