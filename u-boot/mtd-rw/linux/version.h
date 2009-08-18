/* This is a quicky and dirty fix to ensure that the very simple module we
 * build can be loaded by the Libera kernel.  All the other linux header
 * files we need are already in the 3.3.3 toolchain, but the UTS_RELEASE
 * symbol has the wrong value (missing the final -z).  That is all! */

#define UTS_RELEASE "2.4.21-rmk1-pxa1-z"
#define LINUX_VERSION_CODE 132117
#define KERNEL_VERSION(a,b,c) (((a) << 16) + ((b) << 8) + (c))
