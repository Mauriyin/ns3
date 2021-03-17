import math
import matplotlib.pyplot as plt
import pandas as pd
import numpy as np
from analytical import *

mcs = 5
tptBianchi5g = []
tptBianchi6g = []
distance20 = []
distance40 = []
distance80 = []
distance160 = []
distance6g = []
num = 0
txPowerDbm5g = 30
txPowerDbm5gSta = 24
channelWidth = 20*(math.pow(2,num))
txPowerDbm6g = 10*math.log10(3.16  * channelWidth)
txPowerDbm6gSta = 10*math.log10(0.8  * channelWidth)
snrs = []
pers = []
dists = []
def cal_snrper(channelWidth, power):
    rxc = path_loss(distance = dis, m_referenceLoss = 46.6777, m_exponent = 2, m_referenceDistance = 1)
    rxPower = power + rxc
    rxPowerW = math.pow(10, (rxPower/10))/1000
    snr = cal_snr(channelWidth, rxPowerW)
    fer = cal_fer(snr)
    return snr, fer

snrAp5g20 = []
perAp5g20 = []
snrSta5g20 = []
perSta5g20 = []
for k in range(30):
    dis = 50*(k+1)
    distance20.append(dis)
    snr, fer = cal_snrper(20, txPowerDbm5g)
    snrAp5g20.append(snr)
    perAp5g20.append(fer)
    snr, fer = cal_snrper(20, txPowerDbm5gSta)
    snrSta5g20.append(snr)
    perSta5g20.append(fer)
snrs.append(snrAp5g20)
pers.append(perAp5g20)
snrs.append(snrSta5g20)
pers.append(perSta5g20)

snrAp5g40 = []
perAp5g40 = []
snrSta5g40 = []
perSta5g40 = []
for k in range(30):
    dis = 38*(k+1)
    distance40.append(dis)
    snr, fer = cal_snrper(40, txPowerDbm5g)
    snrAp5g40.append(snr)
    perAp5g40.append(fer)
    snr, fer = cal_snrper(40, txPowerDbm5gSta)
    snrSta5g40.append(snr)
    perSta5g40.append(fer)
snrs.append(snrAp5g40)
pers.append(perAp5g40)
snrs.append(snrSta5g40)
pers.append(perSta5g40)

snrAp5g80 = []
perAp5g80 = []
snrSta5g80 = []
perSta5g80 = []
for k in range(30):
    dis = 25*(k+1)
    distance80.append(dis)
    snr, fer = cal_snrper(80, txPowerDbm5g)
    snrAp5g80.append(snr)
    perAp5g80.append(fer)
    snr, fer = cal_snrper(80, txPowerDbm5gSta)
    snrSta5g80.append(snr)
    perSta5g80.append(fer)
snrs.append(snrAp5g80)
pers.append(perAp5g80)
snrs.append(snrSta5g80)
pers.append(perSta5g80)

snrAp5g160 = []
perAp5g160 = []
snrSta5g160 = []
perSta5g160 = []
for k in range(30):
    dis = 20*(k+1)
    distance160.append(dis)
    snr, fer = cal_snrper(160, txPowerDbm5g)
    snrAp5g160.append(snr)
    perAp5g160.append(fer)
    snr, fer = cal_snrper(160, txPowerDbm5gSta)
    snrSta5g160.append(snr)
    perSta5g160.append(fer)
snrs.append(snrAp5g160)
pers.append(perAp5g160)
snrs.append(snrSta5g160)
pers.append(perSta5g160)


snrAp6g = []
perAp6g = []
snrSta6g = []
perSta6g = []
for k in range(30):
    dis = 10*(k+1)
    distance6g.append(dis)
    snr, fer = cal_snrper(channelWidth, txPowerDbm6g)
    snrAp6g.append(snr)
    perAp6g.append(fer)
    snr, fer = cal_snrper(channelWidth, txPowerDbm6gSta)
    snrSta6g.append(snr)
    perSta6g.append(fer)
snrs.append(snrAp6g)
pers.append(perAp6g)
snrs.append(snrSta6g)
pers.append(perSta6g)
dists.append(distance20)
dists.append(distance40)
dists.append(distance80)
dists.append(distance160)
dists.append(distance6g)

snrs = np.array(snrs)
pers = np.array(pers)
dists = np.array(dists)

np.save('snr', snrs)
np.save('per', pers)
np.save('dist', dists)

plt.figure(1)
plt.plot(dists[0], snrs[0], label='5g Ap bandwidth=20')
plt.plot(dists[0], snrs[1], label='5g STA bandwidth=20')
plt.plot(dists[1], snrs[2], label='5g Ap bandwidth=40')
plt.plot(dists[1], snrs[3], label='5g STA bandwidth=40')
plt.plot(dists[2], snrs[4], label='5g Ap bandwidth=80')
plt.plot(dists[2], snrs[5], label='5g STA bandwidth=80')
plt.plot(dists[3], snrs[6], label='5g Ap bandwidth=160')
plt.plot(dists[3], snrs[7], label='5g STA bandwidth=160')
plt.plot(dists[4], snrs[8], label='6g Ap')
plt.plot(dists[4], snrs[9], label='6g STA')
plt.xlabel('Distance (m)')
plt.ylabel('SNR (dBm)')
plt.legend()
plt.savefig('snr.png',dpi=300,format='png')
plt.show()

plt.figure(2)
plt.plot(dists[0], pers[0], label='5g Ap bandwidth=20')
plt.plot(dists[0], pers[1], label='5g STA bandwidth=20')
plt.plot(dists[1], pers[2], label='5g Ap bandwidth=40')
plt.plot(dists[1], pers[3], label='5g STA bandwidth=40')
plt.plot(dists[2], pers[4], label='5g Ap bandwidth=80')
plt.plot(dists[2], pers[5], label='5g STA bandwidth=80')
plt.plot(dists[3], pers[6], label='5g Ap bandwidth=160')
plt.plot(dists[3], pers[7], label='5g STA bandwidth=160')
plt.plot(dists[4], pers[8], label='6g Ap')
plt.plot(dists[4], pers[9], label='6g STA')
plt.xlabel('Distance (m)')
plt.ylabel('P_e')
plt.legend()
plt.savefig('per.png',dpi=300,format='png')
plt.show()