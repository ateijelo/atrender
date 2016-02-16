#include "tilestore.h"

using std::string;

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

string TileStore::md5(const string &data)
{
    unsigned char hash[16];
    //mbedtls_md(mbedtls_md_info_from_type(MBEDTLS_MD_MD5),
    mbedtls_md5(reinterpret_cast<const unsigned char*>(data.c_str()),data.size(),hash);

    return hexdigest(hash);
}

std::ostream &operator<<(std::ostream &o, const tile &t) {
    o << "(" << t.z << "," << t.x << "," << t.y << ")";
    return o;
}
