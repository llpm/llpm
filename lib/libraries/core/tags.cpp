#include "tags.hpp"

#include <llpm/module.hpp>
#include <analysis/graph_queries.hpp>

#include <llvm/IR/DerivedTypes.h>

using namespace std;

namespace llpm {

Tagger::Tagger(llvm::Type* serverReq,
               llvm::Type* serverResp,
               llvm::Type* tag) :
    _server(this,
            GetServerInput(serverReq, serverResp, tag),
            GetServerOutput(serverReq, serverResp, tag),
            true,
            "server"),
    _client(this, serverResp, serverReq, false, "client") {
    _clientDinArr.push_back(_client.din());
}

llvm::Type* Tagger::GetServerInput(
    llvm::Type* serverReq, llvm::Type* serverResp, llvm::Type* tag) {
    return llvm::StructType::get(tag->getContext(), 
                                 vector<llvm::Type*>({tag, serverReq}));
}

llvm::Type* Tagger::GetServerOutput(
    llvm::Type* serverReq, llvm::Type* serverResp, llvm::Type* tag) {
    return llvm::StructType::get(tag->getContext(),
                                 vector<llvm::Type*>({tag, serverResp}));
}

void SynthesizeTagsPass::runInternal(Module* mod) {
    MutableModule* mm = dynamic_cast<MutableModule*>(mod);
    set<Block*> blocks;
    mm->blocks(blocks);
    for (auto block: blocks) {
        Tagger* tagger = dynamic_cast<Tagger*>(block);
        if (tagger != NULL)
            synthesizeTagger(mm, tagger);
    }
}

void SynthesizeTagsPass::synthesizeTagger(MutableModule* mm, Tagger* tagger) {
    ConnectionDB* conns = mm->conns();
    printf("    creating tag for tagger '%s'\n",
           tagger->globalName().c_str());
    if (!queries::CouldReorderTokens(tagger->client())) {
        // The server to which I am connected does not reorder tokens.
        // In this case, I merely need to join responses with request tags
        auto split = new Split(tagger->server()->din()->type());
        auto join = new Join(tagger->server()->dout()->type());
        conns->connect(split->dout(0), join->din(0));
        conns->remap(tagger->server()->din(), split->din());
        conns->remap(tagger->server()->dout(), join->dout());
        conns->remap(tagger->client()->dout(), split->dout(1));
        conns->remap(tagger->client()->din(), join->din(1));
    } else {
        // TODO: Need more strategies for embedding tags!
        fprintf(stderr, "Warning: could not find strategy to embed tag!\n");
    }
}

} // namespace llpm
