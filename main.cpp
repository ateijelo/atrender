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

#include <stdio.h>

#include <fstream>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>
#include <string>
#include <atomic>
#include <chrono>
#include <sstream>
#include <iomanip>
#include <iostream>
#include <algorithm>
#include <system_error>

#include <mapnik/map.hpp>
#include <mapnik/image.hpp>
#include <mapnik/load_map.hpp>
#include <mapnik/projection.hpp>
#include <mapnik/image_util.hpp>
#include <mapnik/image_view.hpp>
#include <mapnik/pixel_types.hpp>
#include <mapnik/agg_renderer.hpp>
#include <mapnik/image_view_any.hpp>
#include <mapnik/datasource_cache.hpp>

#include <boost/program_options.hpp>

#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>

#define ATRENDER_VERSION "0.1"

#include "tilestore.h"
#include "directorytilestore.h"
#include "mbtiles.h"

//namespace fs = boost::filesystem;
//namespace sys = boost::system;

using std::cerr;
using std::cout;
using std::endl;
using std::vector;
using std::string;

using mapnik::Map;

#define DEG_TO_RAD (M_PI/180)
#define RAD_TO_DEG (180/M_PI)

#define RENDER_SIZE 256

struct projectionconfig {
    double bound_x0;
    double bound_y0;
    double bound_x1;
    double bound_y1;
    int    aspect_x;
    int    aspect_y;
};

struct projectionconfig * get_projection(const char * srs) {
    struct projectionconfig * prj;

    if (strstr(srs,"+proj=merc +a=6378137 +b=6378137") != NULL) {
        //syslog(LOG_DEBUG, "Using web mercator projection settings");
        prj = (struct projectionconfig *)malloc(sizeof(struct projectionconfig));
        prj->bound_x0 = -20037508.3428;
        prj->bound_x1 =  20037508.3428;
        prj->bound_y0 = -20037508.3428;
        prj->bound_y1 =  20037508.3428;
        prj->aspect_x = 1;
        prj->aspect_y = 1;
    } else if (strcmp(srs, "+proj=eqc +lat_ts=0 +lat_0=0 +lon_0=0 +x_0=0 +y_0=0 +ellps=WGS84 +datum=WGS84 +units=m +no_defs") == 0) {
        //syslog(LOG_DEBUG, "Using plate carree projection settings");
        prj = (struct projectionconfig *)malloc(sizeof(struct projectionconfig));
        prj->bound_x0 = -20037508.3428;
        prj->bound_x1 =  20037508.3428;
        prj->bound_y0 = -10018754.1714;
        prj->bound_y1 =  10018754.1714;
        prj->aspect_x = 2;
        prj->aspect_y = 1;
    } else if (strcmp(srs, "+proj=tmerc +lat_0=49 +lon_0=-2 +k=0.9996012717 +x_0=400000 +y_0=-100000 +ellps=airy +datum=OSGB36 +units=m +no_defs") == 0) {
        //syslog(LOG_DEBUG, "Using bng projection settings");
        prj = (struct projectionconfig *)malloc(sizeof(struct projectionconfig));
        prj->bound_x0 = 0;
        prj->bound_y0 = 0;
        prj->bound_x1 = 700000;
        prj->bound_y1 = 1400000;
        prj->aspect_x = 1;
        prj->aspect_y = 2;
    } else {
        //syslog(LOG_WARNING, "Unknown projection string, using web mercator as never the less. %s", srs);
        prj = (struct projectionconfig *)malloc(sizeof(struct projectionconfig));
        prj->bound_x0 = -20037508.3428;
        prj->bound_x1 =  20037508.3428;
        prj->bound_y0 = -20037508.3428;
        prj->bound_y1 =  20037508.3428;
        prj->aspect_x = 1;
        prj->aspect_y = 1;
    }

    return prj;
}

mapnik::box2d<double> tile2prjbounds(struct projectionconfig * prj, int x, int y, int z)
{
    double p0x = prj->bound_x0 + (prj->bound_x1 - prj->bound_x0)* ((double)x / (double)(prj->aspect_x * 1<<z));
    double p0y = (prj->bound_y1 - (prj->bound_y1 - prj->bound_y0)* (((double)y + 1) / (double)(prj->aspect_y * 1<<z)));
    double p1x = prj->bound_x0 + (prj->bound_x1 - prj->bound_x0)* (((double)x + 1) / (double)(prj->aspect_x * 1<<z));
    double p1y = (prj->bound_y1 - (prj->bound_y1 - prj->bound_y0)* ((double)y / (double)(prj->aspect_y * 1<<z)));

    mapnik::box2d<double> bbox(p0x, p0y, p1x,p1y);
    return  bbox;
}

struct Args
{
    string input;
    string xml;
    int threads;
    string output_dir;
    string mbtiles;
    bool verbose;
    int subdirs;
};

Args args;

//std::atomic_int unique_tiles;
std::atomic_int rendered_tiles;


void render(Map &m, projectionconfig *prj, TileStore& store, const tile& t)
{
    if (store.alreadyRendered(t))
        return;

    m.resize(256,256);
    m.zoom_to_box(tile2prjbounds(prj,t.x,t.y,t.z));

    if (m.buffer_size() == 0) { // Only set buffer size if the buffer size isn't explicitly set in the mapnik stylesheet.
        m.set_buffer_size(128);
    }

    mapnik::image_rgba8 buf(RENDER_SIZE, RENDER_SIZE);
    mapnik::agg_renderer<mapnik::image_rgba8> ren(m,buf);
    ren.apply(); // <-- Here's where the map is rendered
    rendered_tiles++;

    mapnik::image_view<mapnik::image_rgba8> v1(0, 0, 256, 256, buf);
    struct mapnik::image_view_any view(v1);
    string data = mapnik::save_to_string(view, "png256");
    //cout << "first rendered byte is at " << (void*)(&(data.at(0))) << endl;

    store.storeTile(t, std::move(data));
}

std::atomic_int tilecount;
std::atomic_int finished_threads;

vector<tile> tiles;
vector<tile>::iterator next_tile;
std::mutex next_tile_mutex;

vector<tile>::iterator get_next_tile() {
    std::lock_guard<std::mutex> lock(next_tile_mutex);

    //cout << "next_tile - tiles.begin(): " << next_tile - tiles.begin() << endl;
    if (next_tile == tiles.end())
        return tiles.end();
    auto r = next_tile;
    ++next_tile;
    return r;
}

void render_thread(const std::shared_ptr<TileStore> store, const string& xml) {
    Map m;
    mapnik::load_map(m,xml);

    while (true) {
        auto i = get_next_tile();
        if (i == tiles.end())
            break;
        const tile& t = *i;
        //cout << "thread " << std::this_thread::get_id() << " ";
        //cout << "rendering " << outputdir
        //     << "/" << t.z
        //     << "/" << t.x
        //     << "/" << t.y << ".png" << endl;
        //cout << "store.use_count(): " << store.use_count() << endl;
        try {
            render(m, get_projection(m.srs().c_str()), *store, t);
        } catch (std::exception e) {
            cerr << "rendering tile " << t << "failed with:" << endl;
            cerr << e.what() << endl;
        }
        tilecount++;
    }
    finished_threads++;
}

namespace po = boost::program_options;

int parse_args(int argc, char *argv[], Args *args) {
    string usage = "";
    usage += "atrender " ATRENDER_VERSION "\n"
             "Usage: " + string(argv[0]) + " [options]";
    po::options_description desc(usage, 90);
    desc.add_options()
            ("help,h", "print this help message")
            (",i", po::value<string>(&args->input),
                    "input file with tiles as specified below")
            (",x", po::value<string>(&args->xml),
                    "mapnik XML stylesheet")
            (",n", po::value<int>(&args->threads)->default_value(1),
                    "number of threads")
            (",d", po::value<string>(&args->output_dir),
                    "save tiles to given directory")
            ("subdirs,s", po::value<int>(&args->subdirs)->default_value(0),
                    "when using an output directory (-d) to store tiles avoid too "
                    "many images per folder by spreading files in subdirectories "
                    "based on prefixes of the file names; each subdir uses two "
                    "characters; using -s 2 does this:\n"
                    "    abcdefgh.png -> ab/cd/abcdefgh.png")
            ("mbtiles,m", po::value<string>(&args->mbtiles),
                    "save tiles as an MBTiles file")
            (",v", po::bool_switch(&args->verbose)->default_value(false),
                    "be verbose")

            ;
    po::positional_options_description pod;
    pod.add("-i", 1);

    po::variables_map vm;
    po::store(po::command_line_parser(argc, argv).
              options(desc).positional(pod).run(), vm);
    po::notify(vm);

    if (args->subdirs > 16)
        args->subdirs = 16;

    if (vm.count("help")) {
        cout << desc << endl;
        cout << "Input tiles file must be in the following format:" << endl;
        cout << endl;
        cout << "  Z/X/Y" << endl;
        cout << "  Z/X/Y" << endl;
        cout << "  ..." << endl;
        cout << endl;
        cout << "This is the output format of tilestache-list" << endl;
        return 1;
    }

    if (vm.count("-i") == 0) {
        cout << "Input tiles file is required (one per line in Z/X/Y format)." << endl;
        cout << "See " << argv[0] << " -h" << endl;
        return 1;
    }

    if (vm.count("-x") == 0) {
        cout << "You must supply a mapnik xml stylesheet with -x" << endl;
        cout << "See " << argv[0] << " -h" << endl;
        return 1;
    }

    if (vm.count("-m") > 0 && vm.count("-d") > 0) {
        cout << "Options -m and -d are exclusive" << endl;
        cout << "See " << argv[0] << " -h" << endl;
        return 1;
    }

    return 0;
}

using std::flush;
using std::setw;
using std::left;
using std::setfill;

string pretty(double seconds)
{
    if (seconds < 0)
        return "--:--:--";
    int secs = int(seconds) % 60;
    int mins = (int(seconds) / 60) % 60;
    int hours = int(seconds) / 3600;
    std::ostringstream o;
    o << setw(2) << setfill('0') << hours;
    o << ":" << setw(2) << setfill('0') << mins;
    o << ":" << setw(2) << setfill('0') << secs;
    return o.str();
}

int main(int argc, char *argv[])
{
    int r = parse_args(argc, argv, &args);
    if (r != 0)
        return r;

    const char *plugins_dir = "/usr/lib/mapnik/3.0/input";
    mapnik::datasource_cache::instance().register_datasources(plugins_dir);

    std::shared_ptr<TileStore> store;

    if (!args.mbtiles.empty()) {
        store = std::make_shared<MBTilesTileStore>(
                args.mbtiles, args.verbose
        );
    }
    if (!args.output_dir.empty()) {
        store = std::make_shared<DirectoryTileStore>(
                args.output_dir, args.subdirs, args.verbose
        );
    }
    if (!store) {
        cout << "You must specify a place to save tiles to (-m or -d)" << endl;
        cout << "See " << argv[0] << " -h" << endl;
        return 1;
    }

    string line;
    std::ifstream input(args.input);
    while (true) {
        std::getline(input, line);
        if (input.eof())
            break;
        std::istringstream linestream(line);
        int x,y,z; char c,d;
        linestream >> z >> c >> x >> d >> y;
        if (linestream.fail() || c != '/' || d != '/') {
            cerr << "error parsing line: " << line << endl;
            cerr << "input lines must be in Z/X/Y format" << endl;
            return 1;
        }
        tiles.push_back({x,y,z});
    }

    // this makes the ETA more stable
    // and we intentionally won't seed it, so that
    // if you interrupt a run, the next one will
    // skip all the already rendered tiles first,
    // since the random order will be the same
    std::random_shuffle(tiles.begin(), tiles.end());

    tilecount = 0;
    finished_threads = 0;

    rendered_tiles = 0;
    next_tile = tiles.begin();

    int thread_count = args.threads;
    std::thread threads[thread_count];

    for (int i=0; i<thread_count; i++) {
        threads[i] = std::thread { render_thread, store, args.xml };
    }

    std::chrono::milliseconds d(1000);
    auto start = std::chrono::system_clock::now();

    int total_tiles = tiles.size();
    int moveup = 1; bool first = true;
    while (true)
    {
        std::this_thread::sleep_for(d);
        auto now = std::chrono::system_clock::now();
        std::chrono::duration<double> elapsed = now - start;

        if (!first) {
            for (int i=0; i<moveup; i++)
                printf("\033[A");
            printf("\r");
        }
        first = false;

        double speed = rendered_tiles / elapsed.count();
        double eta = -1;
        if (speed != 0)
            eta = 1 + (total_tiles - tilecount) / speed;

        printf("Total: %d  Processed: %d  Rendered: %d  Unique: %d\n",
               total_tiles, int(tilecount),
               int(rendered_tiles), store->unique_tiles());

        printf("Speed: %.1f  ", speed);
        cout << "Elapsed: " << pretty(elapsed.count()) << "  "
             << "ETA: " << pretty(eta);

        if (!args.mbtiles.empty()) {
            const MBTilesTileStore* s = static_cast<MBTilesTileStore*>(store.get());
            printf("\nWrite queue: %d tiles", s->queue_size());
            moveup = 2;
        }

        if (finished_threads >= thread_count && store->finished())
            break;
    }
    cout << endl;

    for (auto& t: threads)
        t.join();

    return 0;
}
