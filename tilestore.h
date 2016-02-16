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
