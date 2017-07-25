=============================================
     V4L2 video overlay test application
=============================================
Test: v4l2overlay test

Usage steps:

adb shell
cd /usr/kernel-tests/v4l2overlay/
./v4l2overlay


OPTIONS can be:
  -n OR --nominal				Nominal test
  -a OR --adversarial				Adversarial test
  -r OR --repeatability				Repeatability test
  -s OR --stress				Stress test
  -v OR --verbose				Run with debug messages enabled
  -u OR --usage					Show usage
  -t OR --test					Test option

Description:

Nominal Test:
1. Run SIMPLE_OVERLAY test.

Adversarial Tests:
To be defined.

Repeatability Tests:
1. Run SIMPLE_OVERLAY, ROT_0, ROT_90, ROT_180, ROT_270, HFLIP, VFLIP and SCALE
   tests.

Stress tests:
To be defined.

Test options:
-t SIMPLE_OVERLAY		-- Display image
   ROT_0			-- Rotate image by 0 degree
   ROT_90			-- Rotate image by 90 degree
   ROT_180			-- Rotate image by 270 degree
   ROT_270			-- Rotate image by 270 degree
   HFLIP			-- Horizontally flip image
   VFLIP			-- Vertically flip image
   SCALE			-- Scale image
   V4L2_MEMORY_USERPTR_TEST     -- Display image using user pointer memory
   ALL				-- Run all the above test

Targets supported:7x27A, 8x55
