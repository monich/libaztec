## Aztec encoder

![icon](aztec.png)

Library and command line tools for generating Aztec codes.

**aztec-png**

```console
Usage:
  aztec-png [OPTION...] [TEXT] PNG

Generates Aztec symbol as a PNG file.

Help Options:
  -h, --help                   Show help options

Application Options:
  -s, --scale=SCALE            Scale factor [1]
  -c, --correction=PERCENT     Error correction [23]
  -b, --border=PIXELS          Border around the symbol [1]
  -f, --file=FILE              Encode data from FILE
```

**aztec-svg**

```console
  aztec-svg [OPTION?] [TEXT] SVG

Generates Aztec symbol as an SVG file.

Help Options:
  -h, --help                   Show help options

Application Options:
  -p, --pixel=SIZE             Pixel size [1px]
  -c, --correction=PERCENT     Error correction [23]
  -b, --border=PIXELS          Border around the symbol [1]
  -f, --file=FILE              Encode data from FILE

```
Note that the special filename `-` can be used to specify standard
input or standard output. If no text is provided on the command line,
it's read from stdin.

The library API is described [here](include/aztec_encode.h). Enjoy!
