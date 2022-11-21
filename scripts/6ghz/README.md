Steps to reproduce plots found in:
IEEE WLANs in 5 vs 6 GHz: A Comparative Study
================================
Hao Yin, Sumit Roy, and Sian Jin. 2022. IEEE WLANs in 5 vs 6 GHz: A Comparative Study. In Proceedings of the 2022 Workshop on ns-3 (WNS3 '22). Association for Computing Machinery, New York, NY, USA, 25–32. https://doi.org/10.1145/3532577.3532580
## Table of Contents:

1) [Initial Setup](#initial-setup)
2) [Steps to repoduce plots found in Fig.4](#uplink-case)
3) [Steps to repoduce plots found in Fig.5](#unequal-power-case)

## Initial Setup
**The codes for the experiments are under `scripts/6ghz` subdirectory.**  
1) Clone the current code and checkout the unequal branch
```shell
git clone https://github.com/Mauriyin/ns3.git
cd ns3
git checkout -b unequal origin/unequal
```
Under the `ns3` root directory:  

2) Create a directory to store all output data from ns-3

```shell
mkdir uplink_data
mkdir uneqaul_data
```

3) Configure ns-3

```shell
./waf -d optimized configure
```

4) Copy files `uplink.cc`, `wifi-unequal.cc` and `wifi-error-models-comparison.cc` into the `scratch/` subdirectory

4) Build ns-3 `./waf build`

6) Copy the python scripts `run-uplink.py`, `result-uplink.py`, `run-unequal.py` and `result-unequal.py` to the `ns3` root directory

**IMPORTANT NOTE: To produce all the data necessary it is expected that the average server (with at least 4 cores) will take approximately 8Hrs.**   

## Uplink Case

1) Obtain data by executing ns-3 code and storing it in the uplink_data directory.

```shell
python3 run-uplink.py
```

2) Process the output data and create plots found in Fig. 4

```shell
python3 result-uplink.py
```

## Unequal Power Case

1) Obtain data by executing ns-3 code and storing it in the unuqual_data directory.

```shell
python3 run-unequal.py
```

2) Process the output data and create plots found in Fig. 5

```shell
python3 result-unequal.py
```
