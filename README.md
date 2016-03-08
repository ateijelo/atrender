# ATRender

### A fast & simple mapnik tile renderer.

ATRender is a rather simple tile renderer. It uses Mapnik, it is based on a few snippets of code from mod_tile and it tries to use your CPUs efficiently to generate either image files in a directory or an MBTiles file. It was built to generate .mbtiles for Steps.

Usage is rather self explanatory:

```text
ATRender 0.1
Copyright (C) 2016 Andy Teijelo <github.com/ateijelo>
Usage: ./atrender [options]:
  -h [ --help ]             print this help message
  -i arg                    input file with tiles as specified below
  -x arg                    mapnik XML stylesheet
  -n arg (=1)               number of threads
  -p arg                    postprocess tiles with the given command. The command will 
                            receive as its only argument the filename, ending in ".png",
                            of the rendered tile. The command should use the same
                            filename for its result.
  -t arg                    directory for temporary files; these will be created only if
                            postprocessing is enabled.
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

 * It checks for duplicate tiles during generation and does not store them. It uses an indirection layer to share actual image data between equivalent tiles. In directories this means symbolic links; in .mbtiles files it follows MapBox's steps and uses a SQL view.

 * Using `-p`, it can call a command to postprocess a tile. Tiles are in PNG format. See optimize_png.py for an example of a postprocessing command. If you experience a filesystem bottleneck, try using `-t` to save temporary files in a RAM filesystem, e.g. `-t /run/user/1000`.

### License

ATRender is licensed under the GNU General Public License version 3 or later.
