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

#ifndef MBTILES_H
#define MBTILES_H


#include <queue>
#include <mutex>
#include <thread>
#include <atomic>
#include <list>
#include <unordered_map>
#include <unordered_set>
#include <condition_variable>

#include <sqlite3.h>

#include "tilestore.h"

struct InsertOp {
        InsertOp(const tile& t, std::string&& data, int id, const std::string& hash)
            : t(t), data(std::move(data)), id(id), hash(hash)
        {
            //this->data.swap(data);
        }
//        ~InsertOp() {
//            std::cout << "deleting op " << t << std::endl;
//        }

        const tile t;
        const std::string data;
        const int id;
        const std::string hash;
};

class MBTilesTileStore : public TileStore {
    public:
        MBTilesTileStore(const std::string& mbtiles_file, bool verbose = false);
        ~MBTilesTileStore();
        bool alreadyRendered(const tile &t) override;
        void storeTile(const tile &t, std::__cxx11::string &&data) override;
        int unique_tiles() override { return _unique_tiles; }
        void close() override;
        bool finished() override;
        int queue_size() const;

    private:
        void load_ids();
        void load_rendered_tiles();
        void db_error(const std::string& msg);
        void write_loop();
        void exec(const InsertOp& op);

        std::string mbtiles_file;
        bool verbose;
        bool _finished;
        std::atomic_bool closing { false };
        sqlite3 *db;
        std::unordered_map<std::string,int> idmap;
        std::mutex idmap_mutex;

        std::unordered_set<uint64_t> rendered_tiles;
//        std::mutex rendered_tiles_mutex;

        std::atomic_int _unique_tiles {0};
        int next_tile_id;

        std::queue<InsertOp, std::list<InsertOp>> insert_queue;
        std::mutex insert_queue_mutex;

        std::thread write_thread;
        std::condition_variable write_cond;
        std::mutex write_cond_m;

        sqlite3_stmt *select_id_from_map = nullptr;
        sqlite3_stmt *insert_into_idmap = nullptr;
        sqlite3_stmt *insert_into_map = nullptr;
        sqlite3_stmt *insert_into_images = nullptr;

        size_t _queue_size = 0;
};

#endif // MBTILES_H
