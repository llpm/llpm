#include "rtl_wrappers.hpp"

#include <backends/ipxact/ipxact.hpp>

namespace llpm {

void WrapLLPMMModule::validityCheck() const {
}

void WrapLLPMMModule::writeMetadata(IPXactBackend* ipxact) const {
    for (const auto& pr: _pinDefs) {
        for (const auto& pin: pr.second->pins()) {
            IPXactBackend::Port port(pin.name(), pin.input(), pin.width());
            ipxact->push(port);
        }
    }
}

} // namespace llpm
