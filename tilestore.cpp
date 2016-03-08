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

#include <boost/filesystem.hpp>
#include <sstream>
#include <fstream>

#include "tilestore.h"

namespace fs = boost::filesystem;

using std::string;
using std::ofstream;
using std::ifstream;
using std::ios;
using std::cout;
using std::cerr;
using std::endl;

string hexdigest(const unsigned char *digest)
{
    string r;
    r.reserve(32);
    for (int i=0; i<16; i++)
    {
        unsigned char low_nibble = digest[i] & 15;
        unsigned char high_nibble = digest[i] >> 4;
        r += "0123456789abcdef"[high_nibble];
        r += "0123456789abcdef"[low_nibble];
    }
    return r;
}

void TileStore::postprocess(const std::__cxx11::string &command)
{
    postprocess_command = command;
}

void TileStore::tempdir(const std::__cxx11::string &tmpdir)
{
    if (tmpdir.empty())
        _tempdir = fs::temp_directory_path();
    else
        _tempdir = tmpdir;
}

string TileStore::md5(const string &data)
{
    unsigned char hash[16];
    //mbedtls_md(mbedtls_md_info_from_type(MBEDTLS_MD_MD5),
    mbedtls_md5(reinterpret_cast<const unsigned char*>(data.c_str()),data.size(),hash);

    return hexdigest(hash);
}

string TileStore::do_postprocess(const string &data, const string &filename)
{
    fs::path fn = _tempdir / filename;

    ofstream o(fn.string(), ios::binary);
    o.write(data.c_str(), data.size());
    o.close();

    string cmd = postprocess_command + ' ' + fn.string();
    int rc = system(cmd.c_str());
    if (rc != 0) {
        cerr << "Command: " << cmd << " failed with status " << rc << endl;
        return string();
    }

    ifstream i(fn.string(), ios::binary);
    std::ostringstream oss;
    oss << i.rdbuf();

    i.close();
    fs::remove(fn);

    return oss.str();
}

std::ostream &operator<<(std::ostream &o, const tile &t) {
    o << "(" << t.z << "," << t.x << "," << t.y << ")";
    return o;
}
