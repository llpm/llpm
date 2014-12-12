#include "control_region.hpp"

#include <boost/format.hpp>
#include <boost/function.hpp>
#include <boost/bind.hpp>
#include <algorithm>
#include <deque>

namespace llpm {

void FormControlRegionPass::run(Module* mod) {
    ContainerModule* cm = dynamic_cast<ContainerModule*>(mod);
    if (cm == NULL)
        return;

    printf("Forming module into control regions...\n");

    ConnectionDB* conns = cm->conns();
    unsigned counter = 1;

    set<ControlRegion*> regions;
    deque<InputPort*> crInputs;

    // Start with outputs
    for (OutputPort* op: cm->outputs()) {
        InputPort* opSink = cm->getSink(op);
        assert(opSink != NULL);
        crInputs.push_back(opSink);
    }
    
    while (crInputs.size() > 0) {
        InputPort* ip = crInputs.front();
        crInputs.pop_front();

        OutputPort* opDriver = conns->findSource(ip);
        if (opDriver == NULL)
            // An un-driven input? Odd, but whatever...
            continue;

        Block* b = opDriver->owner();
        if (dynamic_cast<ControlRegion*>(b) != NULL)
            // It's already a control region? Cool
            continue;

        std::string crName = str(boost::format("cr%1%") % counter);
        ControlRegion* cr = new ControlRegion(cm, opDriver->owner(), crName);
        cr->grow();
        regions.insert(cr);
        crInputs.insert(crInputs.end(),
                        cr->inputs().begin(), cr->inputs().end());
        counter++;       
    }
    
    printf("Formed %lu control regions\n", regions.size());
}

bool ControlRegion::grow() {
    bool ret = false;

    // This loop will probably only run once to grow and fail the second time,
    // but maybe not...
    while (true) {
        auto gu = growUp();
        auto gd = growDown();
        if (gu || gd) {
            ret = true;
        } else {
            break;
        }
    }

    return ret;
}

bool ControlRegion::growUp() {
    ConnectionDB* pdb = _parent->conns();
    for (InputPort* ip: this->inputs()) {
        OutputPort* driver = pdb->findSource(ip);
        if (driver)
            if (add(driver->owner()))
                return true;
    }
    return false;
}

bool ControlRegion::growDown() {
    return false;
}

bool ControlRegion::add(Block* b) {
    ConnectionDB* pdb = _parent->conns();

    if (dynamic_cast<Module*>(b) != NULL) {
        // Don't merge in unrefined modules
        return false;
    }

    if (b->hasCycle())
        // Don't allow cycles
        return false;

    if (_conns.raw().size() != 0) {
        // Make sure the block is adjacent to this CR
        bool foundAsSink = false;
        for (InputPort* ip: b->inputs()) {
            OutputPort* oldSource = pdb->findSource(ip);
            if (oldSource && oldSource->owner() == this)
                foundAsSink = true;
        } 

        bool foundAsDriver = false;
        for (OutputPort* op: b->outputs()) {
            vector<InputPort*> oldSinks;
            pdb->findSinks(op, oldSinks);
            for (auto ip: oldSinks)
                if (ip->owner() == this)
                    foundAsDriver = true;
        }

        /**
         * Maintain the constraints:
         */

        // Must be adjacent
        if (!foundAsDriver && !foundAsSink) {
            printf("not adj\n");
            return false;
        }

        // Must not create additional valid bits within CR
        if (b->outputsIndependent()) {
            printf("is router\n");
            return false;
        }

        // Must not create additional back pressure bits with CR
        if (b->firing() == OR) {
            printf("is select\n");
            return false;
        }
    }

    bool internallyDriven = false;
    for (InputPort* ip: b->inputs()) {
        OutputPort* oldSource = pdb->findSource(ip);
        if (oldSource)
            pdb->disconnect(ip, oldSource);

        if (oldSource && oldSource->owner() == this &&
            oldSource->owner()->firing() == AND) {
            // Special case: merging block connected to this guy, so just bring
            // the connection internal
            InputPort* internalSink = getSink(oldSource);
            _conns.remap(internalSink, ip);
            internallyDriven = true;

            vector<InputPort*> remainingExtSinks;
            pdb->findSinks(oldSource, remainingExtSinks);
            if (remainingExtSinks.size() == 0)
                removeOutputPort(oldSource);
        } else {
            auto newIP = addInputPort(ip);
            if (oldSource)
                pdb->connect(newIP, oldSource);
        }
    }

    for (OutputPort* op: b->outputs()) {
        vector<InputPort*> oldSinks;
        pdb->findSinks(op, oldSinks);
        for (auto ip: oldSinks)
            pdb->disconnect(op, ip);

        OutputPort* newOP = NULL;
        for (auto ip: oldSinks) {
            if (ip->owner() == this &&
                !ip->owner()->outputsIndependent() &&
                !internallyDriven) {
                // Special case: outputs drive us, so connect internally
                auto internalDriver = getDriver(ip);
                _conns.remap(internalDriver, op);
                removeInputPort(ip);
            } else {
                if (newOP == NULL)
                    newOP = addOutputPort(op);
                pdb->connect(ip, newOP);
            }
        }
    }

    return true;
}

} // namespace llpm
