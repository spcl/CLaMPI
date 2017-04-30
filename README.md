# CLaMPI: A caching layer for MPI
Authors: Salvatore Di Girolamo, Flavio Vella, Torsten Hoefler

CLaMPI is the framework described by Di Girolamo et al. [1] to enable low-overhead caching of RMA get operations.
It is implemented on top of MPI-3 RMA as a proof-of-concept.

All published work using the simulator should cite [1].

[1] Di Girolamo, Salvatore, Flavio Vella, and Torsten Hoefler. "Transparent Caching for RMA Systems." 
(https://spcl.inf.ethz.ch/Publications/.pdf/CLaMPI.pdf)

##Compile
```
```

###On Piz Daint
``` 
    module switch PrgEnv-cray PrgEnv-gnu
    export CRAYPE_LINK_TYPE="dynamic"
    autoreconf -if
    CC=cc ./configure --prefix=$(pwd)/build/ --with-fompi=<fompi_path> --with-dmapp=/opt/cray/dmapp/default/include --enable-adaptive --with-liblsb=<liblsb with MPI path>
    make
    make install
```
###On x86
```
    autoreconf -if
    CC=mpicc ./configure --prefix=$(pwd)/build/ --enable-adaptive --with-liblsb=<liblsb with MPI path>
    make
    make install
```

###Notes:
    - libLSB is optional in configure. If not specified, the perf_hash test will not be compiled
    - `--enable-adaptive` enables the adaptive scheme

