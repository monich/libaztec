## Aztec encoder

![icon](aztec.png)

Library and command line tool for generating Aztec codes.

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

Note that the special filename `-` can be used to specify standard
input or standard output.

Library API is described [here](include/aztec_encode.h). Enjoy!
