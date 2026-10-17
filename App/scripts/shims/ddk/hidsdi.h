/* teensy_loader_cli.c includes <ddk/hidsdi.h>, which older MinGW installs have. Current
 * MinGW-w64 provides the same header as plain <hidsdi.h>, so forward to it. Only used by
 * scripts/build-loader.mjs on Windows (-I scripts/shims); the vendored source is not changed. */
#include <hidsdi.h>
