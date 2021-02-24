import os
import re
import numpy as np
import math
import matplotlib.pyplot as plt
import pandas as pd
from analytical import *

num = 0 # 0 20 MHz; 1 40 MHz;
len5g = 64
elen5g = 38

# num = 1 # 0 20 MHz; 1 40 MHz;
# len5g = 45
# elen5g = 27

# num = 2 # 0 20 MHz; 1 40 MHz;
# len5g = 30
# elen5g = 21

# num = 3 # 0 20 MHz; 1 40 MHz;
# len5g = 21
# elen5g = 14

mcs = 5
tptBianchi5g = []
tptBianchi6g = []
distance5g = []
distance6g = []

txPowerDbm5g = 30
txPowerDbm5gSta = 24
channelWidth = 20*(math.pow(2,num))
txPowerDbm6g = 10*math.log10(3.16  * channelWidth)
txPowerDbm6gSta = 10*math.log10(0.8  * channelWidth)


data_rates = [
    [8.603e6, 17.206e6, 25.8e6, 34.4e6, 51.6e6, 68.8e6, 77.4e6, 86e6, 103.2e6, 114.7e6, 129e6, 143.4e6], 
    [17.2e6, 34.4e6, 51.6e6, 68.8e6, 103.2e6, 137.6e6, 154.9e6, 172.1e6, 206.5e6, 229.4e6, 258.1e6, 286.8e6],
    [36e6, 72.1e6, 108.1e6, 144.1e6, 216.2e6, 288.2e6, 324.3e6, 360.3e6, 432.4e6, 480.4e6, 540.4e6, 600.5e6],
    [72.1e6, 144.1e6, 216.2e6, 288.2e6, 432.4e6, 576.5e6, 648.5e6, 720.6e6, 864.7e6, 960.8e6, 1080.9e6, 1201e6]
]

ack_rates = [
    [6e6, 12e6, 12e6, 24e6, 24e6, 24e6, 24e6, 24e6, 24e6, 24e6, 24e6, 24e6],
    [6e6, 12e6, 12e6, 24e6, 24e6, 24e6, 24e6, 24e6, 24e6, 24e6, 24e6, 24e6],
    [6e6, 12e6, 12e6, 24e6, 24e6, 24e6, 24e6, 24e6, 24e6, 24e6, 24e6, 24e6],
    [6e6, 12e6, 12e6, 24e6, 24e6, 24e6, 24e6, 24e6, 24e6, 24e6, 24e6, 24e6],
]
distanceSim5g = []
distanceSim6g = []
path = "uneqaul_data" 
files= os.listdir(path) 

for flie in files: 
     if not os.path.isdir(flie): 
        dir5g = "mpdu1_band" + str(int(channelWidth)) + "_mcs5_fre5_dis"
        dir6g = "mpdu1_band" + str(int(channelWidth)) + "_mcs5_fre6_dis"
        if dir5g in flie:
            nums = re.findall("\d+", flie)
            # print(nums)
            distanceSim5g.append(int(nums[4]))
        if dir6g in flie:
            nums = re.findall("\d+", flie)
            # print(nums)
            distanceSim6g.append(int(nums[4]))
distanceSim5g.sort()
distanceSim6g.sort()
tpt5g = []
tpt6g = []

for i in range(len(distanceSim5g)):
    data_dir_5 =  "uneqaul_data/mpdu1_band" + str(int(channelWidth)) + "_mcs5_fre5_dis"+ str(int(distanceSim5g[i]))
    tpt_5 = np.array(data_analysis(data_dir_5))
    tpt5g.append(tpt_5[0])
for i in range(len(distanceSim6g)):
    data_dir_6 =  "uneqaul_data/mpdu1_band" + str(int(channelWidth)) + "_mcs5_fre6_dis"+ str(int(distanceSim6g[i]))
    tpt_6 = np.array(data_analysis(data_dir_6))
    tpt6g.append(tpt_6[0]) 

for k in range(23):
    dis = 5*(k+1)
    distance6g.append(dis)
    rxc = path_loss(distance = dis, m_referenceLoss = 49.013, m_exponent = 2, m_referenceDistance = 1)
    rxPower = txPowerDbm6gSta + rxc
    rxPowerW = math.pow(10, (rxPower/10))/1000
    snr = cal_snr(channelWidth, rxPowerW)
    fer = cal_fer(snr)
    bianchi_result, _, _ = bianchi_11(data_rates[num][mcs], ack_rates[num][mcs], 1, 0, fer)
    tptBianchi6g.append(bianchi_result)

for k in range(len5g):
    dis = 10*(k+1)
    distance5g.append(dis)
    rxc = path_loss(distance = dis, m_referenceLoss = 46.6777, m_exponent = 2, m_referenceDistance = 1)
    rxPower = txPowerDbm5gSta + rxc
    rxPowerW = math.pow(10, (rxPower/10))/1000
    snr = cal_snr(channelWidth, rxPowerW)
    fer = cal_fer(snr)
    bianchi_result, _, _ = bianchi_11(data_rates[num][mcs], ack_rates[num][mcs], 1, 0, fer)
    tptBianchi5g.append(bianchi_result)

for k in range(25):
    dis = 130 + 5*(k)
    distance6g.append(dis)
    rxc = path_loss(distance = dis, m_referenceLoss = 49.013, m_exponent = 2, m_referenceDistance = 1)
    rxPower = txPowerDbm6g + rxc
    rxPowerW = math.pow(10, (rxPower/10))/1000
    snr = cal_snr(channelWidth, rxPowerW)
    fer = cal_fer(snr)
    bianchi_result = bianchi_ax_ap(data_rates[num][mcs], ack_rates[num][mcs], 1, 0, fer)
    tptBianchi6g.append(bianchi_result[0]*0.93)


for k in range(elen5g):
    dis = distance5g[-1] + 15
    distance5g.append(dis)
    rxc = path_loss(distance = dis, m_referenceLoss = 46.6777, m_exponent = 2, m_referenceDistance = 1)
    rxPower = txPowerDbm5g + rxc
    rxPowerW = math.pow(10, (rxPower/10))/1000
    snr = cal_snr(channelWidth, rxPowerW)
    fer = cal_fer(snr)
    bianchi_result = bianchi_ax_ap(data_rates[num][mcs], ack_rates[num][mcs], 1, 0, fer)
    tptBianchi5g.append(bianchi_result[0]*0.93)
distance6g.append(distance5g[-1])
tptBianchi6g.append(0)

plt.plot(distance5g, tptBianchi5g, color='g', label='5 GHz Analysis')
plt.plot(distance6g, tptBianchi6g, color='b', label='6 GHz Analysis')
plt.scatter(distanceSim6g,tpt6g, label='6 GHz ns-3', marker='*',color='black')
plt.scatter(distanceSim5g,tpt5g, label='5 GHz ns-3', marker='x',color='black')
filename = 'mcs' + str(int(mcs)) +'_unequal_test' + str(int(channelWidth)) + '.png'
plt.xlabel('Distance (m)')
plt.ylabel('Aggregated Throughput (Mbps)')
plt.legend()
plt.savefig(filename,dpi=300,format='png')
plt.show()
