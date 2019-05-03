tbcomp
------

This repository contains some experimental code to use decision trees to attempt
to improve compression of Syzygy tablebases. It is currently in an unfinished
state and cannot be used to create compressed tablebases.

The source code is in the app/tbcomp directory. It depends on some parts of the
texel code repository, so the full texel repository is included in this
repository for simplicity.


To compile and check that the compiled tbcomp program works as expected:

  cd project_root_directory
  mkdir build
  cd build
  cmake ..
  make -j8
  ./tbcomp test


Re-pair compression/decompression is currently working. To test re-pair
compression, run:

  ./tbcomp repaircomp ../COPYING COPYING.re
  ls -l ../COPYING COPYING.re

To test re-pair decompression, run:

  ./tbcomp repairdecomp COPYING.re COPYING
  diff ../COPYING COPYING


Decision tree computation corresponding to a WDL tablebase file is currently
working, but the computed decision tree is not serialized in any useful format.
The computed decision tree is used to write an uncompressed tablebase file that
can be compressed using "tbcomp repaircomp" to get an estimate for the size a
compressed tablebase file would have.

  export RTBPATH=/path/to/syzygy/files
  ./tbcomp wdldump kqkn
  # Observe the reported decision tree size, in this case 23 leaf nodes.
  ./tbcomp out.bin out.re
  ls -l out.re
  # Observe the compressed size, in this case 184 bytes.

To estimate the size a compressed tablebase file would have, compute:

  184*1.05 + 23*4.1 = 288 bytes

The 1.05 factor corresponds to the estimated size of an index needed to enable
random access for the compressed TB file. The N * 4.1 term is an estimate of the
serialized size of a decision tree with N leaf nodes.


Copyright
---------

The code is currently under a GPLv3 license, but if this would ever evolve into
a usable tablebase solution, the relevant parts would be re-licensed to a more
permissive license.
