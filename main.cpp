#include <stdio.h>
#include <iostream>
#include <vector>
#include <thread>
#include <string>
#include <sstream>
#include <atomic>
#include <chrono>

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


void makedir(string path)
{
    //cout << "creating dir " << path << endl;
    int r = mkdir(path.c_str(), 0777);
    if (r != 0 && errno != EEXIST)
        throw std::runtime_error("Error creating:" + path);
}

void render(Map &m, projectionconfig *prj, const string& outputdir, int x, int y, int z)
{
    m.resize(256,256);
    m.zoom_to_box(tile2prjbounds(prj,x,y,z));

    if (m.buffer_size() == 0) { // Only set buffer size if the buffer size isn't explicitly set in the mapnik stylesheet.
        m.set_buffer_size(128);
    }

    mapnik::image_rgba8 buf(RENDER_SIZE, RENDER_SIZE);
    mapnik::agg_renderer<mapnik::image_rgba8> ren(m,buf);
    ren.apply(); // <-- Here's where the map is rendered

    std::ostringstream o;
    o << outputdir << "/" << z;
    makedir(o.str());
    o << "/" << x;
    makedir(o.str());
    o << "/" << y << ".png";

    mapnik::image_view<mapnik::image_rgba8> v1(0, 0, 256, 256, buf);
    struct mapnik::image_view_any view(v1);
    mapnik::save_to_file(view, o.str(), "png256");
}

struct tile {
    int x;
    int y;
    int z;
};

std::atomic_int tilecount;
std::atomic_int finished_threads;

void render_thread(const vector<tile>& tiles, const string& outputdir, const string& xml) {
    Map m;
    mapnik::load_map(m,xml);

    for (const auto& t: tiles) {
        try {
            render(m,get_projection(m.srs().c_str()),outputdir,t.x,t.y,t.z);
        } catch (std::exception e) {
            cerr << "rendering " << outputdir
                 << "/" << t.z
                 << "/" << t.x
                 << "/" << t.y << ".png failed with:" << endl;
            cerr << e.what() << endl;
        }
        tilecount++;
    }
    finished_threads++;
}

struct Args
{
    string xml;
    int threads;
    string output_dir;
};

Args args;

namespace po = boost::program_options;

int parse_args(int argc, char *argv[], Args *args) {
    string usage = "";
    usage += "atrender " ATRENDER_VERSION "\n"
             "Usage: " + string(argv[0]) + " [options]";
    po::options_description desc(usage, 90);
    desc.add_options()
            ("help,h", "print this help message")
            (",x", po::value<string>(&args->xml),
                    "mapnik XML file")
            (",o", po::value<string>(&args->output_dir)->default_value(".","current directory"),
                    "directory to store rendered tiles")
            (",n", po::value<int>(&args->threads)->default_value(1),
                    "number of threads")
            ;
    po::positional_options_description pod;
    pod.add("input", -1);

    po::variables_map vm;
    po::store(po::command_line_parser(argc, argv).
              options(desc).positional(pod).run(), vm);
    po::notify(vm);

    if (vm.count("help")) {
        cout << desc << endl;
        cout << "Tiles are read from STDIN in the following format:" << endl;
        cout << endl;
        cout << "  Z/X/Y" << endl;
        cout << "  Z/X/Y" << endl;
        cout << "  ..." << endl;
        cout << endl;
        cout << "This is the output format of tilestache-list" << endl;
        return 1;
    }

    if (vm.count("-x") == 0) {
        cout << "You must supply a mapnik xml file with -x" << endl;
        cout << "See " << argv[0] << " -h" << endl;
        return 1;
    }

    return 0;
}

int main(int argc, char *argv[])
{
    int r = parse_args(argc, argv, &args);
    if (r != 0)
        return r;

    const char *plugins_dir = "/usr/lib/mapnik/3.0/input";
    mapnik::datasource_cache::instance().register_datasources(plugins_dir);

    string tiles_dir = args.output_dir + "/tiles";
    makedir(args.output_dir + "/tiles");
    //makedir(args.output_dir + "/images");

    string line;
    vector<tile> tiles;
    while (true) {
        std::getline(std::cin, line);
        if (std::cin.eof())
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

    tilecount = 0;
    finished_threads = 0;

    int thread_count = args.threads;
    std::thread threads[thread_count];

    for (int i=0; i<thread_count; i++) {
        auto from = tiles.begin() + i * tiles.size() / thread_count;
        auto to = tiles.begin() + (i + 1) * tiles.size() / thread_count;

        if (i == thread_count - 1) {
            to = tiles.end();
        }

        threads[i] = std::thread { render_thread, vector<tile>(from, to), tiles_dir, args.xml };
    }

    std::chrono::milliseconds d(500);
    while (true)
    {
        std::this_thread::sleep_for(d);
        printf("Rendered %d tiles  \r", int(tilecount));
        fflush(stdout);
        if (finished_threads >= thread_count)
            break;
    }
    cout << endl;

    for (auto& t: threads)
        t.join();

    return 0;
}

