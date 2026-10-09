set -e
cd "$(dirname "$0")"
# The archive supplies bios_bin.h alongside bios.s. Use that exact supplied
# HLE BIOS; regenerating it is not needed for these screenshot checks.
gcc -O2 -fPIC -shared -o libgba.so gba.c -lm
