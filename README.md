# FBS

FBS is a quantified bit-vector formula simplificator that uses BDD approximations from Q3B to attach valuable information to the input formula.

## Instalation

Make sure to clone this repository using `--recursive` to clone all the submodules as well. Then install the dependencies for Q3B by running
```
cd external/q3b
bash contrib/get_deps.sh
``` 

To compile FBS, run
```
mkdir build
cd build
cmake ..
make
```

## Usage

To process the file `file.smt2` in the SMT-LIB v2 format, run
```
./fbs file.smt2
```

Run `./fbs` to see the available command-line options.

