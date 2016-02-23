# ATRender

### A fast & simple mapnik tile renderer.

ATRender is a rather simple tile renderer. It uses Mapnik, it is based on a few snippets of code from mod_tile and it tries to use your CPUs efficiently to generate either image files in a directory or an MBTiles file. It was built to generate .mbtiles for Steps.

Usage is rather self explanatory:

```text
atrender 0.1
Usage: ./atrender [options]:
  -h [ --help ]             print this help message
  -i arg                    input file with tiles as specified below
  -x arg                    mapnik XML file
  -n arg (=1)               number of threads
  -d arg                    save tiles to given directory
  -s [ --subdirs ] arg (=0) when using an output directory (-d) to store tiles avoid too
                            many images per folder by spreading files in subdirectories
                            based on prefixes of the file names; each subdir uses two
                            characters; using -s 2 does this:
                                abcdefgh.png -> ab/cd/abcdefgh.png
  -m [ --mbtiles ] arg      save tiles as an MBTiles file
  -v                        be verbose

Input tiles file must be in the following format:

  Z/X/Y
  Z/X/Y
  ...

This is the output format of tilestache-list
```

### Features

 * ATRender was designed to be able to resume an interrupted generation process. It will skip already generated tiles.

 * It checks for duplicate tiles during generation and does not store them. It uses an indirection layer to share actual image data between equivalent tiles. In directories this means symbolic links; in .mbtiles files it uses a SQL view.

### License

ATRender is licensed under the GNU General Public License
