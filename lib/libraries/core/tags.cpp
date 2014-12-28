#include "tags.hpp"

#include <llvm/IR/DerivedTypes.h>

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
    return llvm::StructType::get(tag->getContext(), {tag, serverReq});
}

llvm::Type* Tagger::GetServerOutput(
    llvm::Type* serverReq, llvm::Type* serverResp, llvm::Type* tag) {
    return llvm::StructType::get(tag->getContext(), {tag, serverResp});
}

} // namespace llpm
