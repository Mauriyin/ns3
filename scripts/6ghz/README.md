Steps to reproduce plots found in:
IEEE WLANs in 5 vs 6 GHz: A Comparative Study
Hao Yin, Sumit Roy, Sian Jin
================================

## Table of Contents:

1) [Initial Setup](#initial-setup)
2) [Steps to repoduce plots found in Fig.4](#uplink-case)
3) [Steps to repoduce plots found in Fig.5](#unequal-power-case)

## Initial Setup

1) Create a directory to store all output data from ns-3

```shell
mkdir uplink_data
mkdir uneqaul_data
```

2) Configure ns-3

```shell
./waf -d optimized configure
```

3) Copy files "uplink.cc", "wifi-unequal.cc" and "wifi-error-models-comparison.cc" into the scratch/ subdirectory

4) IMPORTANT NOTE: To produce all the data necessary it is expected that the average server (with at least 4 cores) will take approximately 8Hrs. 

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