## P1stream build environment

This repository contains the Mac OS X source plugins for [P1stream].

It contains three plugins:

 - **AudioQueue**: Uses Audio Queue Services in Core Audio's Audio Toolbox
   framework to expose system audio sources as P1stream audio sources.

 - **DisplayStream**: Uses display streams from Quartz Display Services in the
   Core Graphics framework to expose displays as P1stream video sources.

 - **DisplayLink**: Uses display links from the Core Video framework to expose
   display vertical-sync events as P1stream video clocks.

 [P1stream]: https://github.com/p1stream/p1stream

### License

[GPLv3](LICENSE)

© 2014 — Stéphan Kochen
