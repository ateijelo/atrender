#ifndef DIRECTORYTILESTORE_H
#define DIRECTORYTILESTORE_H

#include <atomic>

#include "tilestore.h"

class DirectoryTileStore : public TileStore {
    public:
        DirectoryTileStore(const std::string& output_dir, int subdirs = 0, bool verbose = false);
        bool alreadyRendered(const tile &t) override;
        void storeTile(const tile &t, std::string&& data) override;
        int unique_tiles() override { return _unique_tiles; }

    private:
        std::atomic_int _unique_tiles {0};
        std::string output_dir;
        int subdirs;
        bool verbose;
};

#endif // DIRECTORYTILESTORE_H
