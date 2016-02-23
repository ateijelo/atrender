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

#include <fcntl.h>

#include <iostream>
#include <sstream>
#include <boost/filesystem.hpp>
#include <boost/filesystem/fstream.hpp>

#include "directorytilestore.h"

namespace fs = boost::filesystem;
namespace sys = boost::system;

using std::string;
using std::cout;
using std::cerr;
using std::endl;

DirectoryTileStore::DirectoryTileStore(const string &output_dir, int subdirs, bool verbose)
    : output_dir(output_dir), subdirs(subdirs), verbose(verbose)
{
    boost::system::error_code ec;
    fs::create_directories(output_dir + "/links", ec);
    fs::create_directories(output_dir + "/images", ec);
}

bool DirectoryTileStore::alreadyRendered(const tile &t)
{
    if (verbose)
        cout << "DirectoryTileStore::alreadyRendered " << t << endl;
    std::ostringstream o;
    o << output_dir << "/links/" << t.z << "/" << t.x << "/" << t.y << ".png";
    fs::path p(o.str());
    if (verbose)
        cout << "  path: " << p << endl;
    if (fs::is_symlink(p))
    {
        if (verbose) cout <<   "returning true" << endl;
        return true;
    }
    if (verbose) cout <<   "returning false" << endl;
    return false;
}

void DirectoryTileStore::storeTile(const tile &t, std::__cxx11::string &&data)
{
    std::ostringstream o;
    o << output_dir << "/links/" << t.z << "/" << t.x << "/" << t.y << ".png";
    fs::path tilename(o.str());

    sys::error_code ec;
    fs::create_directories(tilename.parent_path(), ec);


    auto hd = md5(data);
    string imgpath = "";
    for (int i=0; i<subdirs; i++) {
        imgpath += hd.substr(i*2, 2) + "/";
    }
    imgpath += hd + ".png";
    auto image = fs::path(output_dir) / "images" / imgpath;
    // / (hexdigest(hash) + ".png");
    //cout << "image: " << image << endl;
    if (subdirs > 0)
        fs::create_directories(image.parent_path());

    int ofd = open(image.c_str(), O_CREAT | O_EXCL | O_WRONLY, 0644);
    if (ofd > 0)
    {
        _unique_tiles++;
        if (verbose)
            cout << "writing image: " << image << " (" << data.size() << " bytes)" << endl;
        size_t b = data.size();
        size_t pos = 0;
        while (b > 0) {
            ssize_t r = write(ofd, data.c_str() + pos, std::min<size_t>(8192, data.size() - pos));
            if (r < 0) {
                perror((string("Error writing to ") + image.string()).c_str());
                break;
            }
            pos += r;
            b -= r;
            if (r == 0) {
                assert(pos == data.size());
                assert(b == 0);
                break;
            }
        }
        ::close(ofd);
    } else if (errno != EEXIST) {
        perror((string("Error opening ") + image.string() + "for writing").c_str());
    } else if (errno == EEXIST) {
        if (verbose)
            cout << "already existed: " << image << endl;
    }

    string target = string("../../../images/") + imgpath;
    if (verbose)
        cout << "creating link: " << tilename << " -> " << target << endl;
    fs::create_symlink(target, tilename, ec);
    if (ec && (ec.default_error_condition() != sys::errc::file_exists))
        cerr << "creating link failed with: " << ec.message() << endl;
}
