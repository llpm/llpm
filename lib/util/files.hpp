#ifndef __LLPM_UTIL_FILES_HPP__
#define __LLPM_UTIL_FILES_HPP__

#include <set>
#include <string>
#include <fstream>

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>
#include <cstdlib>
#include <linux/limits.h>

#include <util/macros.hpp>
#include <llpm/exceptions.hpp>

namespace llpm {

class FileSet {
public:
    class File {
        friend class FileSet;
        FileSet& _fileset;
        std::string _name;

        FILE* _file;
        std::ofstream _stream;

        File(FileSet& fs, std::string name) :
            _fileset(fs),
            _name(name),
            _file(NULL),
            _stream()
        {
            _stream.exceptions(std::ofstream::failbit |
                               std::ofstream::badbit);
        }

    public:
        DEF_GET_NP(name);

        std::string baseName() {
            return _name.substr(_name.find_last_of("/")+1);
        }

        FILE* openFile(const char* mode) {
            if (_stream.is_open()) {
                throw InvalidCall("Cannot open FILE* while file has an ofstream open!");
            }
            _file = fopen(_name.c_str(), mode);
            if (_file == NULL)
                throw SysError("creating file '" + _name + "'");
            return _file;
        }

        std::ostream& openStream(std::ios_base::openmode mode = std::ios_base::out) {
            if (_file != NULL) {
                throw InvalidCall("Cannot open stream while file has a FILE* open!");
            }
            _stream.open(_name, mode);
            return _stream;
        }

        void close() {
            if (_file != NULL) {
                fclose(_file);
                _file = NULL;
            }

            if (_stream.is_open()) {
                _stream.close();
            }
        }

        void flush() {
            if (_stream.is_open())
                _stream.flush();
            if (_file != NULL)
                fflush(_file);
        }

        void erase() {
            _fileset.erase(this);
        }

        // Derive a new file with ext
        File* deriveExt(std::string ext) {
            return _fileset.create(this->baseName() + ext);
        }
    };

    std::string _dfltDir;
    std::set<File*> _files;
    bool _keepTemps;

    std::set<std::string> _temporaries;

    std::string createAbsFileName(std::string fn) {
        return _dfltDir + "/" + fn;
    }

public:
    FileSet(bool keepTemps = false,
            std::string dfltDir = "",
            bool create=true) :
        _keepTemps(keepTemps) {
        this->dfltDir(dfltDir, create);
    }

    ~FileSet() {
        for (File* f: _files) {
            f->close();
            delete f;
        }

        // TODO: rm -rf everything in _temporaries
    }

    DEF_GET_NP(dfltDir);
    void dfltDir(std::string dn, bool create=true) {
        if (dn== "") {
            char* cwd = get_current_dir_name();
            _dfltDir = cwd;
            free(cwd);
        } else {
            _dfltDir = dn;
        }

        struct stat s;
        int rc = stat(_dfltDir.c_str(), &s);
        if (rc != 0) {
            if (errno == ENOENT && create) {
                rc = mkdir(_dfltDir.c_str(), 0777);
                if (rc != 0) {
                    throw SysError("creating working directory '" + _dfltDir + "'");
                }
            } else {
                throw SysError("opening working directory '" + _dfltDir + "'");
            }
        } else {
            if (!S_ISDIR(s.st_mode)) {
                throw InvalidCall("Working directory does not appear to be a directory!");
            }
        }
    }

    File* create(std::string fn) {
        File* f = new File(*this, createAbsFileName(fn));
        _files.insert(f);
        return f;
    }

    void flush() {
        for (auto f: _files)
            f->flush();
    }

    std::string tmpdir() {
        char name[_dfltDir.size() + 16];
        sprintf(name, "%s/tmpXXXXXX", _dfltDir.c_str());
        std::string dn = mkdtemp(name);
        _temporaries.insert(dn);
        return dn;
    }

    void erase(File* f) {
        f->close();
        if (!_keepTemps) {
            auto entry = _files.find(f);
            if (entry == _files.end())
                throw InvalidCall("This file does not belong to this fileset!");
            _files.erase(entry);
            unlink(f->name().c_str());
            delete f;
        }
    }

    template<typename Container>
    void erase(Container c) {
        for (auto&& f: c) {
            this->erase(f);
        }
    }

    bool exists(std::string fn) {
        fn = _dfltDir + "/" + fn;
        struct stat s;
        int rc = stat(_dfltDir.c_str(), &s);
        if (rc == ENOENT)
            return false;
        return true;
    }

    File* copy(std::string fn, std::string newName = "") {
        if (newName == "") {
            auto loc = fn.find_last_of("/");
            if (loc == std::string::npos)
                newName = fn;
            else
                newName = fn.substr(loc+1);
        }
        auto dst = _dfltDir + "/" + newName;
        if (exists(newName))
            unlink(dst.c_str());
        int rc = link(fn.c_str(), dst.c_str());
        if (rc != 0)
            throw SysError("Copying file to working set");
        return create(newName);
    }
};

struct Directories {
    static std::string executableFullName() {
        char dest[PATH_MAX+1];
        auto rc = readlink("/proc/self/exe", dest, PATH_MAX);
        if (rc == -1)
            throw SysError("finding executable location");
        dest[rc] = '\0';
        return dest;
    }

    static std::string executableName() {
        std::string full = executableFullName();
        auto pos = full.find_last_of("/");
        return full.substr(pos+1);
    }

    static std::string executablePath() {
        std::string full = executableFullName();
        auto pos = full.find_last_of("/");
        return full.substr(0, pos);
    }
};

};

#endif // __LLPM_UTIL_FILES_HPP__
