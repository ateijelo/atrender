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

#ifndef TILESTORE_H
#define TILESTORE_H

#include <memory>
#include <iostream>
#include <string>
#include <mbedtls/md5.h>

struct tile {
    int x;
    int y;
    int z;
};

std::ostream& operator<<(std::ostream& o, const tile& t);

class TileStore {
    public:
        virtual bool alreadyRendered(const tile& t) = 0;
        virtual void storeTile(const tile& t, std::string&& data) = 0;
        virtual void close() {}
        virtual int unique_tiles() = 0;
        virtual bool finished() { return true; }
        std::string md5(const std::string& data);
};

#endif // TILESTORE_H
