#ifndef __LIBRARIES_CORE_TAGS_HPP__
#define __LIBRARIES_CORE_TAGS_HPP__

#include <libraries/core/interface.hpp>
#include <passes/pass.hpp>

namespace llpm {
    class MutableModule;

class Tagger : public Block {
    Interface _server;
    Interface _client;

    std::vector<InputPort*> _clientDinArr;

    static llvm::Type* GetServerInput(
        llvm::Type* serverReq, llvm::Type* serverResp, llvm::Type* tag);
    static llvm::Type* GetServerOutput(
        llvm::Type* serverReq, llvm::Type* serverResp, llvm::Type* tag);

public:
    Tagger(llvm::Type* serverReq, llvm::Type* serverResp, llvm::Type* tag);
    virtual ~Tagger() { }

    DEF_GET(server);
    DEF_GET(client);

    virtual bool hasState() const {
        return false;
    }

    virtual DependenceRule deps(const OutputPort* op) const {
        assert(std::find(outputs().begin(), outputs().end(), op)
               != outputs().end());
        if (op == _client.dout())
            return DependenceRule(DependenceRule::Custom, inputs());
        else
            return DependenceRule(DependenceRule::Custom, _clientDinArr);
    }
};

class SynthesizeTagsPass : public ModulePass {
    void synthesizeTagger(MutableModule*, Tagger*);

public:
    SynthesizeTagsPass(Design& d) :
        ModulePass(d)
    { }

    virtual void runInternal(Module*);
};

} // namespace llpm

#endif // __LIBRARIES_CORE_TAGS_HPP__
