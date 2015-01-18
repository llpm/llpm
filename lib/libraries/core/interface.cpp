#include "interface.hpp"

#include <libraries/core/comm_intr.hpp>
#include <libraries/core/tags.hpp>
#include <libraries/core/logic_intr.hpp>
#include <util/misc.hpp>

#include <llvm/IR/Type.h>
#include <llvm/IR/Constants.h>

namespace llpm {

bool InterfaceMultiplexer::refine(ConnectionDB& conns) const {
    if (_servers.size() <= 1) {
        // If we have one or zero clients, then we are totally unnecessary

        Identity* reqI = new Identity(_client.dout()->type());
        Identity* respI = new Identity(_client.din()->type());
        conns.remap(_client.dout(), reqI->dout());
        conns.remap(_client.din(), respI->din());

        if (_servers.size() > 0) {
            Interface* serv = _servers[0].get();
            conns.remap(serv->din(), reqI->din());
            conns.remap(serv->dout(), respI->dout());
        }

        return true;
    } else {
        unsigned tag_width = idxwidth(_servers.size());
        llvm::Type* tag = llvm::Type::getIntNTy(
            _client.dout()->type()->getContext(), tag_width);
        auto tagger = new Tagger(_client.dout()->type(),
                                 _client.din()->type(),
                                 tag);
        auto select = new Select(
            _servers.size(), tagger->server()->din()->type());
        auto router = new Router(
            _servers.size(), _client.din()->type());
        conns.connect(router->din(), tagger->server()->dout());
        conns.connect(select->dout(), tagger->server()->din());
        conns.remap(client(), tagger->client());

        for (unsigned i=0; i<_servers.size(); i++) {
            auto join = new Join({tag, _client.dout()->type()});
            auto cnst = new Constant(
                llvm::ConstantInt::get(tag, i, false));
            conns.connect(join->dout(), select->din(i));
            conns.connect(cnst->dout(), join->din(0));
            conns.remap(_servers[i]->din(), join->din(1));
            conns.remap(_servers[i]->dout(), router->dout(i));
        }
        return true;
    }
}

} // namespace llpm
