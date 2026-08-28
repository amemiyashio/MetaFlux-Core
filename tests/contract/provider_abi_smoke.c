#ifndef METAFLUX_PROVIDER_HEADER
#error "METAFLUX_PROVIDER_HEADER must name the provider API header"
#endif

#ifndef METAFLUX_PROVIDER_BOOTSTRAP
#error "METAFLUX_PROVIDER_BOOTSTRAP must name the provider bootstrap function"
#endif

#include METAFLUX_PROVIDER_HEADER

#include "metaflux/client/protocol.h"

int main(void) { return METAFLUX_PROVIDER_BOOTSTRAP() == MF_CLIENT_PROTOCOL_ABI_VERSION_1 ? 0 : 1; }
