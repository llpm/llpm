#include "interface.hpp"

#include <libraries/core/comm_intr.hpp>

namespace llpm {

bool InterfaceMultiplexer::refine(ConnectionDB& conns) const {
    if (_servers.size() > 1)
        return false;


    Identity* reqI = new Identity(_client.dout()->type());
    Identity* respI = new Identity(_client.din()->type());
    conns.remap(_client.dout(), reqI->dout());
    conns.remap(_client.din(), respI->din());

    if (_servers.size() > 0) {
        Interface* serv = _servers[0];
        conns.remap(serv->din(), reqI->din());
        conns.remap(serv->dout(), respI->dout());
    }

    return true;
}

} // namespace llpm
