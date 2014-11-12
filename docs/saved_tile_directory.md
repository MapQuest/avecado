# Generating a Directory of Tiles
Avecado can be used to pre-render a set of vector tiles with the demo HTTP server.

To do so, a Mapnik XML file is needed. [Mapbox Studio](https://github.com/mapbox/mapbox-studio) will do this, or [Kosmtik](https://github.com/kosmtik/kosmtik) can be used to convert a YAML layer file to Mapnik XML. Assuming a TileMill 2 vector source located in ``$TM2SOURCE`` is being used, the latter can be done with ``nodejs index.js export $TM2SOURCE/data.yml --output $TM2SOURCE/data.xml``.

Avecado needs to be downloaded and [built](https://github.com/MapQuest/avecado#building). When this is done, the HTTP server can be run. Adjust the number of threads to the number of CPU cores.

```sh
./avecado_server $TM2SOURCE/data.xml 4002 --thread-hint 4
```

In another console window, use curl and [GNU Parallel](https://www.gnu.org/software/parallel/) to scrape tiles into a suitable directory structure. The [Slippy Map tile directory structure](http://wiki.openstreetmap.org/wiki/Slippy_map_tilenames) is very simple which allows this. GNU Parallel can be obtained with ``sudo apt-get install parallel`` on Debian based systems.

Use the same number of jobs as avecado_server threads. 
```sh
(for z in $(seq 0 8); do
  for y in $(seq 0 $((2**z - 1))); do
  mkdir -p ${z}/${y}
    for x in $(seq 0 $((2**z - 1))); do
      echo "${z}/${y}/${x}"
    done;
  done;
done;) | parallel --progress –jobs 4 curl -s --retry 5 --retry-delay 25 -o {}.pbf http://localhost:4002/{}.pbf
# Save the tile JSON too
curl -o tile.json http://localhost:4002/tile.json
```

Going to where the files are being saved and counting the number of files gives better view of progress. This can be done with

```
for d in `ls -v`; do echo $d: $(find $d -name '*.pbf' | wc -l)/$((4**d)); done
```

Downloading zoom levels above 8 may take considerable time, as there are a quarter million tiles on zoom 9, one million on zoom 10, and it increases exponentially.
