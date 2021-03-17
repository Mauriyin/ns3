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
def bianchi_ax_ap(data_rate, ack_rate, k, difs, fer = 0.0):
       # Parameters for 11ax
    nA = [2]
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
        #print(m)
        n = nA[j]*1 
        W = CWmin + 1 
        m = math.log2((CWmax + 1)/(CWmin + 1)) 
        tau1 = 2/1024  
        p = 1 - np.power((1 - tau1),(n - 1))
        ps = p*0 
        peq = p + fer - fer*p
        for i in range(int(m)):
            ps = ps + np.power(2*peq, i) 

        taup = 2./(1 + W + peq*W*ps) 
        b = np.argmin(np.abs(tau1 - taup))
        tau = taup 
        #print(tau)
        Ptr = 1 - math.pow((1 - tau1), int(n-1))*(1-tau)
        Ps = tau*math.pow((1 - tau1), int(n-1))/Ptr

        S_bianchi[j] = K_MSDU*K_MPDU*Ps*Ptr*EP*(1-fer)/((1-Ptr)*T_SLOT+Ptr*Ps*T_S*(1-fer)+Ptr*(1-Ps)*T_C+Ptr*Ps*fer*T_E)/1e6 

    bianchi_result = S_bianchi
    return bianchi_result
def bianchi_11(data_rate, ack_rate, k, difs, fer = 0.0):
       # Parameters for 11ax
    nA = [5]
    CWmin = 15 
    CWmax = 1023 
    L_DATA = 1500 * 8 # data size in bits
    L_ACK = 14 * 8    # ACK size in bits
    B = 1/(CWmin+1) 
    #B=0 
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
    j = 0
    m = math.log2((CWmax + 1)/(CWmin + 1))   
    n = nA[j]*1 
    W = CWmin + 1 
    m = math.log2((CWmax + 1)/(CWmin + 1)) 
    
    # pw_sta = np.linspace(0, 0.1, 100)
    # ps = pw_sta*0 
    # for i in range(int(m)):
    #     ps = ps + np.power(2*pw_sta, i) 
    # tau_sta = 2./(1 + W + pw_sta*W*ps) 
    
    # peq = tau_sta
    # ps = tau_sta*0 
    # for i in range(int(m)):
    #     ps = ps + np.power(2*peq, i) 
    # tau_ap = 2./(1 + W + peq*W*ps)

    # pe_sta = (1. - tau_ap)/(1. - pw_sta)
    # pe_sta = 1 - pe_sta
    tau1 = np.linspace(0, 0.1, 100)  
    p = tau1
    ps = p*0 
    peq = p + fer - fer*p
    for i in range(int(m)):
        ps = ps + np.power(2*peq, i) 
    taup = 2./(1 + W + peq*W*ps) 
    tau_sta = taup

    p = 1 - np.power((1 -tau_sta), (n-1))
    ps = p*0 
    peq = p
    for i in range(int(m)):
        ps = ps + np.power(2*peq, i) 
    tau_ap = 2./(1 + W + peq*W*ps) 
    pc_sta = 1 - (1 - tau_ap)*np.power((1 -tau_sta), (n-2))
    b = np.argmin(np.abs(tau1 - pc_sta))
    tau_ap = tau_ap[b]
    tau_sta = tau_sta[b]

    Ptr = 1 - (1 - tau_ap)*np.power((1 -tau_sta), (n-1))
    Ps_ap = (tau_ap*np.power((1 -tau_sta), (n-1)))/Ptr
    Ps_sta = (n-1)*(tau_sta*(1-tau_ap)*np.power((1 -tau_sta), (n-2)))/Ptr

    bianchi_result = K_MSDU*K_MPDU*Ps_sta*Ptr*EP*(1-fer)/((1-Ptr)*T_SLOT+Ptr*Ps_sta*T_S*(1-fer)+Ptr*(1-Ps_sta)*T_C+Ptr*Ps_sta*fer*T_E)/1e6 + K_MSDU*K_MPDU*Ps_ap*Ptr*EP*(1-0)/((1-0)*T_SLOT+Ptr*Ps_ap*T_S*(1-0)+Ptr*(1-Ps_ap)*T_C+Ptr*Ps_ap*0*T_E)/1e6
    return bianchi_result, tau_ap, tau_sta

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

