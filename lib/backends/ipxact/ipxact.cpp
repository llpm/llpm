#include "ipxact.hpp"

#include <tinyxml2.h>

using namespace std;
using namespace tinyxml2;

namespace llpm {

IPXactBackend::IPXactBackend(Design& design) :
    Backend(design),
    _verilog(design)
{
    _ports.emplace_back("clk", true);
    _ports.emplace_back("resetn", true);
}

void IPXactBackend::buildClkRst() {
    BusInterface clk("clk");
    clk.busType = {
        {"spirit:vendor", "xilinx.com"},
        {"spirit:library", "signal"},
        {"spirit:name", "clock"},
        {"spirit:version", "1.0"}
    };
    clk.abstractionType = {
        {"spirit:vendor", "xilinx.com"},
        {"spirit:library", "signal"},
        {"spirit:name", "clock_rtl"},
        {"spirit:version", "1.0"}
    };
    clk.portMaps = {
        {"CLK", "clk"}
    };
    string busses;
    for (unsigned i=0; i<_busInterfaces.size(); i++) {
        if (i > 0)
            busses += ":";
        busses += _busInterfaces[i].name;
    }
    clk.parameters = {
        {"ASSOCIATED_BUSIF",
            {
                {"value", busses},
                {"spirit:resolve", "immediate"},
                {"spirit:id", "BUSIFPARAM_VALUE.ACLK.ASSOCIATED_BUSIF"}
             }
        },
        {"ASSOCIATED_RESET",
            {
                {"value", "resetn"},
                {"spirit:resolve", "immediate"},
                {"spirit:id", "BUSIFPARAM_VALUE.ACLK.ASSOCIATED_RESET"}
             }
        }
    };
    push(clk); 

    BusInterface resetn("resetn");
    resetn.busType = {
        {"spirit:vendor", "xilinx.com"},
        {"spirit:library", "signal"},
        {"spirit:name", "reset"},
        {"spirit:version", "1.0"}
    };
    resetn.abstractionType = {
        {"spirit:vendor", "xilinx.com"},
        {"spirit:library", "signal"},
        {"spirit:name", "reset_rtl"},
        {"spirit:version", "1.0"}
    };
    resetn.portMaps = {
        {"RST", "resetn"}
    };
    resetn.parameters = {
        {"POLARITY",
            {
                {"value", "ACTIVE_LOW"},
                {"spirit:resolve", "immediate"},
                {"spirit:id", "BUSIFPARAM_VALUE.ARESETN.POLARITY"}
             }
        }
    };
    push(resetn); 
}

static const std::string tclDummy = R"STRING(
# Definitional proc to organize widgets for parameters.
proc init_gui { IPINST } {
  ipgui::add_param $IPINST -name "Component_Name"
  #Adding Page
  ipgui::add_page $IPINST -name "Page 0"
}
)STRING";

void IPXactBackend::writeModule(FileSet& dir,
                                Module* mod,
                                std::set<FileSet::File*>& files)
{
    std::set<FileSet::File*> vsFiles;
    _verilog.writeModule(dir, mod, vsFiles);

    dir.mkdir("ipxact/hdl/verilog");
    dir.mkdir("ipxact/xgui");

    for (auto vsfile: vsFiles) {
        auto c = dir.copy(vsfile->name(),
                          "ipxact/hdl/verilog/" + vsfile->baseName());
        files.insert(c);
        _pkgFiles.insert("hdl/verilog/" + vsfile->baseName());
    }

    auto tclFile = dir.create("ipxact/xgui/gui.tcl");
    auto& tclos = tclFile->openStream();
    tclos << tclDummy;
    tclFile->close();
    files.insert(tclFile);

    auto xmlFile = dir.create("ipxact/component.xml");
    buildClkRst();
    writeComponent(xmlFile, mod);
}

class EasyPrinter : public XMLPrinter {
public:
    EasyPrinter(FILE* f) :
        XMLPrinter(f)
    { }

    void operator()(const char* name, const char* value) {
        OpenElement(name);
        PushText(value);
        CloseElement();
    }

    void operator()(const char* name, const std::string& value) {
        operator()(name, value.c_str());
    }

    void operator()(const char* name, uint64_t value) {
        operator()(name, str(boost::format("%1%") % value));
    }
};

struct Elem {
    EasyPrinter& ep;

    Elem(EasyPrinter& ep, const char* name) :
        ep(ep) {
        ep.OpenElement(name);
    }

    ~Elem() {
        ep.CloseElement();
    }

    void operator()(const char* k, const char* v) {
        ep.PushAttribute(k, v);
    }
    void operator()(const char* k, const string& v) {
        ep.PushAttribute(k, v.c_str());
    }
    void operator()(const string& k, const string& v) {
        ep.PushAttribute(k.c_str(), v.c_str());
    }
};

void IPXactBackend::writeComponent(FileSet::File* xmlFile, Module* mod) {
    EasyPrinter pr(xmlFile->openFile("w"));
    pr.PushHeader(false, true);
    pr.OpenElement("spirit:component");
    pr.PushAttribute(
        "xmlns:spirit",
        "http://www.spiritconsortium.org/XMLSchema/SPIRIT/1685-2009");
    pr.PushAttribute(
        "xmlns:xsi",
        "http://www.w3.org/2001/XMLSchema-instance");
    pr.PushAttribute(
        "xmlns:xilinx",
        "http://www.xilinx.com");

    /***** Header info *****/
    pr("spirit:vendor", "llpm.org");
    pr("spirit:library", "LLPM");
    pr("spirit:name", mod->name());
    pr("spirit:version", "1.0");

    /***** Bus Interfaces section ******/
    pr.OpenElement("spirit:busInterfaces");
    for (const auto& bi: _busInterfaces) {
        Elem sbi(pr, "spirit:busInterface");
        pr("spirit:name", bi.name);
        {Elem tpe(pr, "spirit:busType");
            for (auto& p: bi.busType) {
                tpe(p.first, p.second);
            }
        }
        {Elem abt(pr, "spirit:abstractionType");
            for (auto& p: bi.abstractionType) {
                abt(p.first, p.second);
            }
        }
        {Elem slave(pr, "spirit:slave");
            if (bi.mmName != "") {
                Elem mmref(pr, "spirit:memoryMapRef");
                mmref("spirit:memoryMapRef", bi.mmName);
            }
        }
        {Elem portMaps(pr, "spirit:portMaps");
            for (auto& p: bi.portMaps) {
                Elem portMap(pr, "spirit:portMap");
                {Elem log(pr, "spirit:logicalPort");
                    pr("spirit:name", p.first);
                }
                {Elem phys(pr, "spirit:physicalPort");
                    pr("spirit:name", p.second);
                }
            }
        }
        pr("spirit:endianness",
           bi.littleEndian ? "little" : "big");
        if (bi.parameters.size() > 0) {
            Elem abt(pr, "spirit:parameters");
            for (const auto& p: bi.parameters) {
                Elem param(pr, "spirit:parameter");
                pr("spirit:name", p.first);
                Elem value(pr, "spirit:value");
                std::string valStr;
                for (const auto& p2: p.second) {
                    if (p2.first != "value")
                        value(p2.first, p2.second);
                    else
                        valStr = p2.second;
                }
                pr.PushText(valStr.c_str());
            }
        }
    }
    pr.CloseElement(); // busInterfaces

    /******* Memory maps ********/
    if (_memoryMaps.size() > 0) {
        Elem mms(pr, "spirit:memoryMaps");
        for (const auto& mm: _memoryMaps) {
            Elem smm(pr, "spirit:memoryMap");
            pr("spirit:name", mm.name);
            Elem blk(pr, "spirit:addressBlock");
            pr("spirit:name", mm.name);
            pr("spirit:baseAddress", mm.baseAddress);
            pr("spirit:range", mm.range);
            pr("spirit:width", mm.width);
            pr("spirit:usage", "register");
            pr("spirit:access", "read-write");
        }
    }

    /***** "Model" section ******/
    pr.OpenElement("spirit:model");
    pr.OpenElement("spirit:views");

    pr.OpenElement("spirit:view");
    pr("spirit:name", "synthesis");
    pr("spirit:envIdentifier", "verilog:vivado.xilinx.com:synthesis");
    pr("spirit:language", "verilog");
    pr("spirit:modelName", mod->name());
    pr.OpenElement("spirit:fileSetRef");
    pr("spirit:localName", "synthesis_files");
    pr.CloseElement();
    pr.CloseElement(); // view

    {
        Elem view(pr, "spirit:view");
        pr("spirit:name", "xilinx_xpgui");
        pr("spirit:displayName", "UI Layout");
        pr("spirit:envIdentifier", ":vivado.xilinx.com:xgui.ui");
        Elem fref(pr, "spirit:fileSetRef");
        pr("spirit:localName", "xpgui");
    }

    pr.CloseElement(); // views

    pr.OpenElement("spirit:ports");
    for (Port port: _ports) {
        Elem sp(pr, "spirit:port");
        pr("spirit:name", port.name);
        Elem sw(pr, "spirit:wire");
        pr("spirit:direction",
            port.input ? "in" : "out");
        if (port.width > 1) {
            Elem sv(pr, "spirit:vector");
            pr("spirit:left",
               str(boost::format("%1%") % (port.width-1)));
            pr("spirit:right", "0");
        }
        Elem swdfs(pr, "spirit:wireTypeDefs");
        Elem swdf(pr, "spirit:wireTypeDef");
        pr("spirit:typeName", "std_logic_vector");
        pr("spirit:viewNameRef", "synthesis");
    }
    pr.CloseElement(); // ports
    pr.CloseElement(); // model


    /******** File Sets section *****/
    pr.OpenElement("spirit:fileSets");
    pr.OpenElement("spirit:fileSet");
    pr.OpenElement("spirit:name");
    pr.PushText("synthesis_files");
    pr.CloseElement();
    for (auto pf: _pkgFiles) {
        pr.OpenElement("spirit:file");
        pr.OpenElement("spirit:name");
        pr.PushText(pf.c_str());
        pr.CloseElement();
        pr.OpenElement("spirit:fileType");
        pr.PushText("systemVerilogSource");
        pr.CloseElement();
        pr.CloseElement();
    }
    pr.CloseElement();
    {
        Elem fs(pr, "spirit:fileSet");
        pr("spirit:name", "xpgui");
        Elem file(pr, "spirit:file");
        pr("spirit:name", "xgui/gui.tcl");
        pr("spirit:userFileType", "XGUI_VERSION_2");
        pr("spirit:fileType", "tclSource");
    }
    pr.CloseElement();

    pr("spirit:description", "An IP generated by LLPM");

    /******* Parameters ******/
    {
        Elem params(pr, "spirit:parameters");
        {
            Elem para(pr, "spirit:parameter");
            pr("spirit:name", "Component_name");
            Elem value(pr, "spirit:value");
            pr.PushText(mod->name().c_str());
        }
    }

    /****** Vendor Extensions *******/
    {
        Elem ve(pr, "spirit:vendorExtensions");
        Elem ce(pr, "xilinx:coreExtensions");
        {
            Elem se(pr, "xilinx:supportedFamilies");
            Elem fam(pr, "xilinx:family");
            fam("xilinx:lifeCycle", "Pre-Production");
            pr.PushText("zynq");
        }
        pr("xilinx:displayName", mod->name());
        pr("xilinx:coreRevision", "1");
    }

    pr.CloseElement();
    xmlFile->close();
}

} // namespace llpm

