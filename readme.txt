OS/2-MMPM/2-driver for MikMod (by Stefan Tibus)

This package contains the original MikMod 2.09b-package along with the 
sources of my OS/2-MMPM/2-driver. As the driver uses MCI-function calls it 
should work on any sound-card supported by OS/2 (It was tested on Creative 
Labs SoundBlaster 16 and Turtle Beach Systems Tropez/Tropez Plus soundcards).

This package contains nine files:

README.TXT   - this file
MIKM209B.ZIP - the original MikMod-package
MPLAY2T.EXE  - a compiled version of MikMod using my OS/2-drivers
MIKMOD.C
MIKMOD.H
MTYPES.H     - MikMod-sources I had to do some minor changes on
               You may use these files directly in addition to the original
               MikMod-sources or if you already have another (newer) version
               of MikMod look for the changes I marked with:
               "/* OS/2-support (added by S.T.) */".
               The changes I did were to include the OS/2-driver and the
               OS/2-header files and minor changes to the type-definitions in
               MTYPES.H.
DRV_OS22.C   - The latest version of my OS/2-driver. It uses a 512kB buffer
               consisting of two blocks 256kB-blocks which are updated in
               32kB-blocks (MikMod cannot update blocks >64kB).
DRV_OS21.C   - This was my first version of the driver. It uses a 64kB 
               double-buffer.
               Also the source contains german comments only (sorry but I did
               not think of distribution at that time).
             - I included this one because it works more real-time, the new
               version with the 512kB-buffer is always a bit late, when 
               starting to play or switching to another MOD-file, because 
               there still is one 256kB-half currently playing the old music...
             - Well, both versions of the driver are still in Beta-state and
               their as buggy too :-( though they're quite useable.

Now, some legal stuff:

- There are no warranties at all.
- The author is not responsible for any problems arising by using this module.
- Any commercial use requires a special permission of the author (Stefan Tibus).

- The code and any part of it may not be published without the authors explicit
  permission.
- The code may only be distributed in complete and unmodified form.
  All files included in this package have to be distributed together and in
  unmodified form. You may add files to the package or include this package
  into your distributions.
  Distribution of the code, this package or distributions containing them have
  to be for FREE !



My e-mail address:

FidoNet: Stefan Tibus @ 2:246/8722.20

or p.o.-box: Stefan.Tibus@ThePentagon.com

Any feedback is welcome !



So, that's it, hope it'll work fine at you...

Greetings,

Stefan Tibus
