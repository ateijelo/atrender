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
        InsertOp(const tile& t, std::string&& data)
            : t(t), data(std::move(data))
        {
            //this->data.swap(data);
        }
//        ~InsertOp() {
//            std::cout << "deleting op " << t << std::endl;
//        }

        tile t;
        std::string data;
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

        std::unordered_set<uint64_t> rendered_tiles;
        std::mutex rendered_tiles_mutex;

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
};

#endif // MBTILES_H
