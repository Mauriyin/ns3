import os
import re
import numpy as np
import math
import matplotlib.pyplot as plt
import pandas as pd

def path_loss(distance, m_referenceLoss, m_exponent = 2, m_referenceDistance = 1):
    pathLossDb = 10 * m_exponent * math.log10 (distance / m_referenceDistance)
    rxc = -m_referenceLoss - pathLossDb
    return rxc

def cal_snr(channelWidth, rxPowerW):
    BOLTZMANN = 1.3803e-23
    Nt = BOLTZMANN * 290 * channelWidth * 1e6
    m_noiseFigure = 5.01187
    noiseFloor = m_noiseFigure * Nt
    noise = noiseFloor
    snr = 10*math.log10(rxPowerW/noise) 
    return snr
def print_result(bianchi_result):
    print("{\"HeMcs3\", {")
    for  i in range (len(bianchi_result)):
        print("{%d, %.4f}," % (5*(i+1), bianchi_result[i]))
    print("}},")

def str_result(bianchi_result, mcs):
    str_bianchi = '{' + '\"HeMcs{:d}\"'.format(mcs) + ', {\n'
    for  i in range (len(bianchi_result)):
        str_tmp = '\t\t{' + '{:d}, {:.4f}'.format(5*(i+1), bianchi_result[i]) +'},\n'
        str_bianchi  = str_bianchi + str_tmp
    str_bianchi = str_bianchi + "}},\n"
    print(str_bianchi)
    return str_bianchi

def data_analysis(data_dir):
    f = open(data_dir,'r')
    #out = open('output.txt','w')
    lines = f.readlines()
    throughput = []
    for line in lines:
        if "Total throughput:" in line:
            tpt = re.findall("\d+\.\d+", line)
            tpts = re.findall("\d+", line)
            tpt.append(tpts[0])
            #print(tpt)
            throughput.append(float(tpt[0]))
            #out.write(line)

    #print(throughput)
    return throughput

def bianchi_ax(data_rate, ack_rate, k, difs, fer = 0.0):
       # Parameters for 11ax
    nA = [5]
    CWmin = 15 
    CWmax = 1023 
    L_DATA = 1500 * 8 # data size in bits
    L_ACK = 14 * 8    # ACK size in bits
    B = 1/(CWmin+1) 
    B=0 
    EP = L_DATA/(1-B)
    T_GI = 800e-9                  # guard interval in seconds
    T_SYMBOL_ACK = 4e-6            # symbol duration in seconds (for ACK)
    T_SYMBOL_DATA = 12.8e-6 + T_GI # symbol duration in seconds (for DATA)
    T_PHY_ACK = 20e-6              # PHY preamble & header duration in seconds (for ACK)
    T_PHY_DATA = 44e-6             # PHY preamble & header duration in seconds (for DATA)
    L_SERVICE = 16                 # service field length in bits
    L_TAIL = 6                     # tail lengthh in bits
    L_MAC = (30) * 8               # MAC header size in bits
    L_APP_HDR = 8 * 8              # bits added by the upper layer(s)
    T_SIFS = 16e-6 
    T_DIFS = 34e-6 
    T_SLOT = 9e-6 
    delta = 1e-7 
    L_PADD = 2 * 8
    Aggregation_Type = 'A_MPDU'   #A_MPDU or A_MSDU (HYBRID not fully supported)
    K_MSDU = 1 
    K_MPDU = k 
    L_MPDU_HEADER = 4 * 8 
    L_MSDU_HEADER = 14 * 8 
    
    if (k <= 1):
        Aggregation_Type = 'NONE' 

    N_DBPS = data_rate * T_SYMBOL_DATA   # number of data bits per OFDM symbol

    if (Aggregation_Type == 'NONE'):
        N_SYMBOLS = math.ceil((L_SERVICE + (L_MAC + L_DATA + L_APP_HDR) + L_TAIL)/N_DBPS)
        T_DATA = T_PHY_DATA + (T_SYMBOL_DATA * N_SYMBOLS)
        K_MPDU = 1
        K_MSDU = 1 
        #print((L_SERVICE + (L_MAC + L_DATA + L_APP_HDR) + L_TAIL)/8)
    if (Aggregation_Type == 'A_MSDU'):
        N_SYMBOLS = math.ceil((L_SERVICE + K_MPDU*(L_MAC + L_MPDU_HEADER + K_MSDU*(L_MSDU_HEADER + L_DATA + L_APP_HDR)) + L_TAIL)/N_DBPS)
        T_DATA = T_PHY_DATA + (T_SYMBOL_DATA * N_SYMBOLS)

    if (Aggregation_Type == 'A_MPDU'):
        N_SYMBOLS = math.ceil((L_SERVICE + K_MPDU*(L_MAC + L_MPDU_HEADER + L_DATA + L_APP_HDR) + (K_MPDU - 2)*L_PADD + L_TAIL)/N_DBPS)
        T_DATA = T_PHY_DATA + (T_SYMBOL_DATA * N_SYMBOLS)
        #print(K_MPDU*(L_MAC + L_MPDU_HEADER + L_DATA + L_APP_HDR)/8 )
        L_ACK = 32 * 8
    #Calculate ACK Duration
    N_DBPS = ack_rate * T_SYMBOL_ACK   # number of data bits per OFDM symbol
    N_SYMBOLS = math.ceil((L_SERVICE + L_ACK + L_TAIL)/N_DBPS)
    T_ACK = T_PHY_ACK + (T_SYMBOL_ACK * N_SYMBOLS)

    T_s = T_DATA + T_SIFS + T_ACK + T_DIFS 
    if difs == 1: #DIFS
        T_C = T_DATA + T_DIFS 
    else:
        T_s = T_DATA + T_SIFS + T_ACK + T_DIFS + delta 
        T_C = T_DATA + T_DIFS + T_SIFS + T_ACK + delta  + T_SLOT
    # print(T_DATA, T_ACK)
    T_S = T_s/(1-B) + T_SLOT 
    T_E = T_DATA + T_DIFS + T_SIFS + T_ACK + delta  + T_SLOT
    #T_E=T_C
    S_bianchi = np.zeros(len(nA)) 
    for j in range(len(nA)):
        N = nA[j]*1
        m = math.log2((CWmax + 1)/(CWmin + 1))   
        n = nA[j]*1 
        W = CWmin + 1 
        m = math.log2((CWmax + 1)/(CWmin + 1)) 
        tau1 = np.linspace(0, 0.1, 100000)  
        p = 1 - np.power((1 - tau1),(n - 1))
        ps = p*0 
        peq = p + fer - fer*p
        for i in range(int(m)):
            ps = ps + np.power(2*peq, i) 

        taup = 2./(1 + W + peq*W*ps) 
        b = np.argmin(np.abs(tau1 - taup))
        tau = taup[b] 
        # print(tau)
        Ptr = 1 - math.pow((1 - tau), int(n))
        Ps = n*tau*math.pow((1 - tau), int(n-1))/Ptr

        S_bianchi[j] = K_MSDU*K_MPDU*Ps*Ptr*EP*(1-fer)/((1-Ptr)*T_SLOT+Ptr*Ps*T_S*(1-fer)+Ptr*(1-Ps)*T_C+Ptr*Ps*fer*T_E)/1e6 

    bianchi_result = S_bianchi
    return bianchi_result

def cal_fer(snr):
    run_command =  './waf --run \"scratch/wifi-error-models-comparison --frameFormat=He --beginMcs=5 --endMcs=6 --stepMcs=1 --snrInput={:f}\"'.format(float(snr))
    process = os.popen(run_command) # return file
    output = process.read()
    process.close()
    fer = 0.0
    for line in output.splitlines():
        if "fer" in line:
            fer = re.findall("\d+\.\d+", line)
            fer = float(fer[0])
    return fer





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

mcs = 5
for i in range(4):
    tptBianchi5g = []
    tptBianchi6g = []
    tpt5g = []
    tpt6g = []
    distance = [5]
    txPowerDbm5g = 24
    channelWidth = 20*(math.pow(2,i))
    txPowerDbm6g = 10*math.log10(0.8 * channelWidth)
    bianchi_result = bianchi_ax(data_rates[i][mcs], ack_rates[i][mcs], 1, 0, 0.0)
    tptBianchi5g.append(bianchi_result[0])
    tptBianchi6g.append(bianchi_result[0])

    data_dir_5 =  "uplink_data/mpdu1_band" + str(int(channelWidth)) + "_mcs5_fre5_dis"+ str(int(5))
    data_dir_6 =  "uplink_data/mpdu1_band" + str(int(channelWidth)) + "_mcs5_fre6_dis"+ str(int(5))
    tpt_5 = np.array(data_analysis(data_dir_5))
    tpt_6 = np.array(data_analysis(data_dir_6))
    tpt5g.append(tpt_5[0])
    tpt6g.append(tpt_6[0])
    
    for k in range(12):
        dis = 50*(k+1)
        distance.append(dis)
        rxc = path_loss(distance = dis, m_referenceLoss = 46.6777, m_exponent = 2, m_referenceDistance = 1)
        rxPower = txPowerDbm5g + rxc
        rxPowerW = math.pow(10, (rxPower/10))/1000
        snr = cal_snr(channelWidth, rxPowerW)
        fer = cal_fer(snr)
        bianchi_result = bianchi_ax(data_rates[i][mcs], ack_rates[i][mcs], 1, 0, fer)
        tptBianchi5g.append(bianchi_result[0])
        
        rxc = path_loss(distance = dis, m_referenceLoss = 49.013, m_exponent = 2, m_referenceDistance = 1)
        rxPower = txPowerDbm6g + rxc
        rxPowerW = math.pow(10, (rxPower/10))/1000
        snr = cal_snr(channelWidth, rxPowerW)
        fer = cal_fer(snr)
        bianchi_result = bianchi_ax(data_rates[i][mcs], ack_rates[i][mcs], 1, 0, fer)
        tptBianchi6g.append(bianchi_result[0])

        data_dir_5 =  "uplink_data/mpdu1_band" + str(int(channelWidth)) + "_mcs5_fre5_dis"+ str(int(50*(k+1)))
        data_dir_6 =  "uplink_data/mpdu1_band" + str(int(channelWidth)) + "_mcs5_fre6_dis"+ str(int(50*(k+1)))
        tpt_5 = np.array(data_analysis(data_dir_5))
        tpt_6 = np.array(data_analysis(data_dir_6))
        tpt5g.append(tpt_5[0])
        tpt6g.append(tpt_6[0])

    distance.append(650)
    tpt5g.append(0)
    tpt6g.append(0)
    tptBianchi6g.append(0)
    tptBianchi5g.append(0)
    print(distance, tpt5g, tptBianchi6g)
    plt.figure(i)
    plt.plot(distance,tpt5g,label='5 GHz ns-3',marker='*',color='blue', linestyle='-.',markersize=4)
    plt.plot(distance,tptBianchi5g,label='5 GHz Bianchi',marker='*',color='g', linestyle='--',markersize=4)
    plt.plot(distance,tpt6g,label='6 GHz',marker='x',color='red', linestyle='-',markersize=4)
    plt.plot(distance,tptBianchi6g,label='6 GHz Bianchi',marker='*',color='y', linestyle='--',markersize=4)
    plt.xlabel('Distance')
    plt.ylabel('Throughput (Mbps)')
    plt.legend()
    filename = 'mcs' + str(int(mcs)) +'_uplink' + str(int(channelWidth)) + '.png'
    plt.savefig(filename,dpi=300,format='png')
    plt.show()


