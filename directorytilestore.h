// This file is part of ATRender, a fast & simple mapnik tile render
// Copyright (C) 2016  Andy Teijelo <github.com/ateijelo>

// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.

// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.

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
