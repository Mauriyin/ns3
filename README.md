# 11ac Multi-BSS Simulatin on latest ns-3-dev 

Location of script used for simulation
→./ns3/src/wifi/examples

used script "PIMRC_sim.sh" in ./ns3/src/wifi/scripts

reference simulator:https://github.com/kajihara-ry/ns3-11ac

[ns-3 installation procedure(Ubuntu 16.04)]

1. Clone this repo.

2. Build ns-3 under the 'ns3' folder.

   command : ./waf configure --enable-examples  --build-profile=optimized

3. Compile ns-3.

   command : ./waf
   
4. After compiling ns-3, it will be possible to run simulations.

# Issues
### Compile issues:
There are some compile issues thanks to the update of the mainline ns-3, and I fixed them through this [commit](https://github.com/Mauriyin/ns3/commit/8cad2d6d26d4880da6aa79ef85153a6c3ab79afd). I commented on the parts that are not used and when I put them back to your 11ac [repo](https://github.com/kajihara-ry/ns3-11ac), it had the same results. So I am confident these modifications are right to do.
### Throughput issue:
If I run on this version above, the UDP server only sends one packet. I've noticed that this is because the variable is set to true:
```
filterOutNonAddbaEstablished = true
```
I make some changes in this [commit](https://github.com/Mauriyin/ns3/commit/c5353d6de06750369ea96ad53455adcb360e44c5) and then the packets are sending successfully. However, the throughput is low and unequal compared with the one in your [repo](https://github.com/kajihara-ry/ns3-11ac).  
Throughput from this repo, 5 STAs per AP:
```
Uplink [Mbps]: 400
Downlink [Mbps]: 0
Uplink Efficiency   9.18057 [%]
Downlink Efficiency 0 [%]
Throughput,  AP1 Uplink   [Mbps] : 36.7223
Throughput,  AP1 Downlink [Mbps] : 0
Throughput,  AP2 Uplink   [Mbps] : 16.4063
Throughput,  AP2 Downlink [Mbps] : 0
Total Throughput Uplink   [Mbps] : 53.1286
Total Throughput Downlink [Mbps] : 0
```   
Throughput from the original [repo](https://github.com/kajihara-ry/ns3-11ac), 5 STAs per AP:
```
Uplink [Mbps]: 400
Downlink [Mbps]: 0
Uplink Efficiency   7.26772 [%]
Downlink Efficiency 0 [%]
Throughput,  AP1 Uplink   [Mbps] : 29.0709
Throughput,  AP1 Downlink [Mbps] : 0
Throughput,  AP2 Uplink   [Mbps] : 27.9243
Throughput,  AP2 Downlink [Mbps] : 0
Total Throughput Uplink   [Mbps] : 56.9952
Total Throughput Downlink [Mbps] : 0
```