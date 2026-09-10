#include "metaflux/compiler/ptx_frontend.hpp"

int main() { return metaflux::compiler::ptx::bootstrap_epoch() == 1U ? 0 : 1; }
