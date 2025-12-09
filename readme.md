## Compile command:

```g++ src/glc_norm_v2.cpp src/io_handling.cpp src/utility.cpp -o glc_norm```

or use CMake to compile with
```cmake -S . -B build```
and executing ```make``` inside the build folder



## How to run

```./glc_norm arquivo.txt cnf log.txt``` for chomsky normal form 
ou
```./glc_norm arquivo.txt gnf log.txt``` for greibach normal form