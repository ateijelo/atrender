#include <iostream>

#include "mbtiles.h"

using std::string;
using std::cout;
using std::cerr;
using std::endl;

using std::mutex;
using std::lock_guard;
using std::unique_lock;

/* NOTE: This class will fail if zoom levels higher than 28 are used
 * because of the optimization used by alreadyRendered to store the set
 * of rendered tiles.
 */
MBTilesTileStore::MBTilesTileStore(const string &mbtiles_file, bool verbose)
    : mbtiles_file(mbtiles_file), verbose(verbose)
{
    int rc;
    rc = sqlite3_open(mbtiles_file.c_str(), &db);
    if (rc != 0)
    {
        sqlite3_close(db);
        throw std::runtime_error("Error opening database");
    }
    char *errmsg;
    rc = sqlite3_exec(db,
        R"sql(
        CREATE TABLE IF NOT EXISTS metadata (
            name TEXT,
            value TEXT,
            PRIMARY KEY (name)
        );
        CREATE TABLE IF NOT EXISTS map (
            zoom INTEGER,
            col INTEGER,
            row INTEGER,
            tile_id INTEGER,
            PRIMARY KEY (zoom, col, row)
        ) WITHOUT ROWID;
        CREATE TABLE IF NOT EXISTS images (
            tile_id INTEGER PRIMARY KEY,
            tile_data BLOB
        );
        CREATE VIEW IF NOT EXISTS tiles AS
            SELECT
                map.zoom AS zoom_level,
                map.col AS tile_column,
                map.row AS tile_row,
                images.tile_data AS tile_data
            FROM map
            JOIN images ON images.tile_id = map.tile_id;
        CREATE TABLE IF NOT EXISTS idmap (
            md5 TEXT,
            tile_id INTEGER
        );
        )sql",
        nullptr,
        nullptr,
        &errmsg
    );
    if (rc != 0)
    {
        cerr << "Error while creating db: " << errmsg << endl;
        //cout << "sqlite3_exec returned " << rc << endl;
        throw std::runtime_error("Error initializing database");
    }
    load_ids();
    load_rendered_tiles();

    std::thread t {[this]() {
        write_loop();
    }};

    write_thread = std::move(t);
}

MBTilesTileStore::~MBTilesTileStore()
{
    close();
}

void MBTilesTileStore::load_ids()
{
    sqlite3_stmt *stmt;
    if (sqlite3_prepare_v2(db, "SELECT md5, tile_id FROM idmap;", -1, &stmt, nullptr) != SQLITE_OK)
        db_error("error preparing select from idmap query");

    if (verbose) cout << "Loading tile id's from database... ";
    int rc;
    next_tile_id = -1;
    while (true) {
        rc = sqlite3_step(stmt);
        if (rc == SQLITE_ROW) {
            const char *hash = (const char *)sqlite3_column_text(stmt, 0);
            int tile_id = sqlite3_column_int(stmt, 1);
            idmap.insert({hash, tile_id});
            next_tile_id = std::max(next_tile_id, tile_id);
        } else if (rc == SQLITE_DONE) {
            break;
        }
        else {
            db_error("error stepping through idmap table");
        }
    }

    next_tile_id++;
    if (verbose) cout << "done." << endl;

//    for (const auto& pair : idmap) {
//        cout << "  " << pair.first << ", " << pair.second << endl;
//    }

//    std::chrono::milliseconds d(2000);
//    std::this_thread::sleep_for(d);

    sqlite3_finalize(stmt);
}

void MBTilesTileStore::load_rendered_tiles()
{
    sqlite3_stmt *stmt;
    if (sqlite3_prepare_v2(db, "SELECT zoom, col, row FROM map;", -1, &stmt, nullptr) != SQLITE_OK)
        db_error("error preparing select from idmap query");

    if (verbose) cout << "Loading rendered tiles from database... ";
    int rc;
    while (true) {
        rc = sqlite3_step(stmt);
        if (rc == SQLITE_ROW) {
            uint64_t z = sqlite3_column_int(stmt, 0);
            uint64_t x = sqlite3_column_int(stmt, 1);
            uint64_t y = sqlite3_column_int(stmt, 2);
            rendered_tiles.insert((z << 58)|(x << 29)|y);
        } else if (rc == SQLITE_DONE) {
            break;
        } else {
            db_error("error stepping through map table");
        }
    }

    if (verbose) cout << "done." << endl;
//    for (uint64_t t : rendered_tiles) {
//        int z = (int)(t >> 58);
//        int x = (int)((t >> 29) & 0x1fffffff);
//        int y = (int)(t & 0x1fffffff);
//        cout << "already rendered: " << t
//             << "(" << z << "/" << x << "/" << y << ")" << endl;
//    }
    sqlite3_finalize(stmt);
}

bool MBTilesTileStore::alreadyRendered(const tile &t)
{
    uint64_t z = t.z;
    uint64_t x = t.x;
    uint64_t y = t.y;
    auto it = rendered_tiles.find((z << 58)|(x << 29)|y);

    if (it == rendered_tiles.end())
    {
        return false;
    }
    return true;

//    if (select_id_from_map == nullptr) {
//        if (sqlite3_prepare_v2(db, "SELECT tile_id FROM map WHERE "
//                               "zoom = ? AND col = ? AND row = ?;",
//                               -1, &select_id_from_map, nullptr) != SQLITE_OK)
//            db_error("error preparing select from map query");
//    } else {
//        if (sqlite3_reset(select_id_from_map) != SQLITE_OK)
//            db_error("error resetting select from map query");
//    }

//    if (sqlite3_bind_int(select_id_from_map, 1, t.z) != SQLITE_OK)
//        db_error("error binding insert into map query");
//    if (sqlite3_bind_int(select_id_from_map, 2, t.x) != SQLITE_OK)
//        db_error("error binding insert into map query");
//    if (sqlite3_bind_int(select_id_from_map, 3, t.y) != SQLITE_OK)
//        db_error("error binding insert into map query");

//    int rc = sqlite3_step(select_id_from_map);
//    if (!(rc == SQLITE_ROW || rc == SQLITE_DONE))
//        db_error("error stepping through query");

//    bool result = false;
//    if (rc == SQLITE_ROW)
//       result = true;

//    return result;
}

void MBTilesTileStore::db_error(const string& msg)
{
    cerr << msg << ": " << sqlite3_errmsg(db) << endl;
    throw std::runtime_error("database error");
}

void MBTilesTileStore::write_loop()
{
    unique_lock<mutex> lock(write_cond_m);

    while (true) {
        sqlite3_exec(db, "BEGIN", nullptr, nullptr, nullptr);
        while (!insert_queue.empty()) {
            _finished = false;
            const InsertOp& op = insert_queue.front();
            exec(op);
            insert_queue_mutex.lock();
            insert_queue.pop();
            insert_queue_mutex.unlock();
        }

        sqlite3_exec(db, "COMMIT", nullptr, nullptr, nullptr);
        _finished = true;

        // ran out of tiles to write
        // I'll wait until someone wakes me up
        if (closing)
            break;
        else
            write_cond.wait(lock);
    }
}

void MBTilesTileStore::exec(const InsertOp &op)
{
    const tile& t = op.t;
    const string& data = op.data;

    int rc;

    auto hash = md5(data);
    auto it = idmap.find(hash);
    int tile_id;
    if (it == idmap.end()) {
        tile_id = next_tile_id;
        next_tile_id++;
        _unique_tiles++;

        idmap.insert({hash, tile_id});

        if (insert_into_idmap == nullptr) {
            if (sqlite3_prepare_v2(db,
                    "INSERT INTO idmap VALUES(?, ?);", -1,
                    &insert_into_idmap, nullptr
                ) != SQLITE_OK) {
                db_error("error preparing 'insert into idmap' query");
            }
        } else {
            if (sqlite3_reset(insert_into_idmap) != SQLITE_OK)
                db_error("error resetting 'insert into idmap' query");
        }
        if (sqlite3_bind_text(insert_into_idmap, 1, hash.c_str(), hash.size(), SQLITE_STATIC) != SQLITE_OK)
            db_error("error binding insert into idmap query");
        if (sqlite3_bind_int(insert_into_idmap, 2, tile_id) != SQLITE_OK)
            db_error("error binding insert into idmap query");

        rc = sqlite3_step(insert_into_idmap);
        if (rc != SQLITE_DONE)
            db_error("error stepping through 'insert into idmap' query");


        if (insert_into_images == nullptr) {
            if (sqlite3_prepare_v2(db,
                    "INSERT INTO images VALUES(?, ?);", -1,
                    &insert_into_images, nullptr
                ) != SQLITE_OK) {
                db_error("error preparing 'insert into images' query");
            }
        } else {
            if (sqlite3_reset(insert_into_images) != SQLITE_OK)
                db_error("error resetting 'insert into images' query");
        }
        if (sqlite3_bind_int(insert_into_images, 1, tile_id) != SQLITE_OK)
            db_error("error binding insert into images query");
        if (sqlite3_bind_blob(insert_into_images, 2, data.c_str(), data.size(), SQLITE_STATIC) != SQLITE_OK)
            db_error("error binding insert into images query");

        rc = sqlite3_step(insert_into_images);
        if (rc != SQLITE_DONE)
            db_error("error stepping through 'insert into images' query");

    } else {
        tile_id = it->second;
    }

//    sqlite3_stmt *stmt;
//    if (sqlite3_prepare_v2(db, "SELECT tile_id FROM idmap WHERE md5 = ?;",
//            -1, &stmt, nullptr) != SQLITE_OK)
//        db_error("error preparing select from idmap query");

//    if (sqlite3_bind_text(stmt, 1, hash.c_str(), hash.size(), SQLITE_STATIC)
//            != SQLITE_OK)
//        db_error("error binding select from idmap query");

//    rc = sqlite3_step(stmt);
//    if (!(rc == SQLITE_ROW || rc == SQLITE_DONE))
//        db_error("error stepping through query");

//    int tile_id = -1;
//    if (rc == SQLITE_ROW)
//       tile_id = sqlite3_column_int(stmt, 0);
//    sqlite3_finalize(stmt);

    //cout << "idmap[" << hash << "] = " << tile_id << endl;
    if (insert_into_map == nullptr) {
        if (sqlite3_prepare_v2(db,
                "INSERT INTO map VALUES(?,?,?,?);", -1,
                &insert_into_map, nullptr
            ) != SQLITE_OK) {
                db_error("error preparing 'insert into map' query");
            }
    } else {
        if (sqlite3_reset(insert_into_map) != SQLITE_OK)
            db_error("error resetting 'insert into map' query");
    }
    if (sqlite3_bind_int(insert_into_map, 1, t.z) != SQLITE_OK)
        db_error("error binding insert into map query");
    if (sqlite3_bind_int(insert_into_map, 2, t.x) != SQLITE_OK)
        db_error("error binding insert into map query");
    if (sqlite3_bind_int(insert_into_map, 3, t.y) != SQLITE_OK)
        db_error("error binding insert into map query");
    if (sqlite3_bind_int(insert_into_map, 4, tile_id) != SQLITE_OK)
        db_error("error binding insert into map query");
    rc = sqlite3_step(insert_into_map);
    if (rc != SQLITE_DONE)
        db_error("error stepping through 'insert into map' query");

//    auto it = idmap.find(hash);
//    if (it == idmap.end())
//        return StoreResult::Duplicate;
}

void MBTilesTileStore::storeTile(const tile &t, std::__cxx11::string &&data)
{
    {
        lock_guard<mutex> guard { insert_queue_mutex };
        //cout << "data.length = " << data.length() << endl;
        //cout << "before inserting data.c_str() was " << (void*)(&(data.at(0))) << endl;
        //auto op = InsertOp(t, data);
        //cout << "after constructing op, first byte is at " << (void*)(&(op.data.at(0))) << endl;
        insert_queue.emplace(t, std::move(data));
        //cout << "after inserting, insert_queue.back().data.at(0) is " << (void*)(&(insert_queue.back().data.at(0))) << endl;
        //cout << (void*)(data.c_str()) << endl;
        //cout << data.empty() << endl;
        //lock_guard<mutex> write_cond_guard { write_cond_m };
    }
    write_cond.notify_one();
}

void MBTilesTileStore::close()
{
    closing = true;
    write_cond.notify_one();
    write_thread.join();
    if (verbose)
        cout << "Closing database." << endl;
    sqlite3_close(db);
}

bool MBTilesTileStore::finished()
{
    return _finished;
}

int MBTilesTileStore::queue_size() const
{
    return insert_queue.size();
}
