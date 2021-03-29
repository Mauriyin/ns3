#!/bin/bash
source spatial-reuse-functions.sh
cd ../examples

#obssPdLevel=-82
#export NS_LOG=DynamicObssPdAlgorithm=logic

for i in 1; do #Iteration(seed)
for j in 1; do #Number of STAs
RngRunpar=${i}
npar=${j}
Res=Sim

#ns-3 Parameters
export AUTO_DELETE_SPATIAL_REUSE_OUTPUT_FILES=1 #Auto delete result files (ON:1/OFF:0)
export enablePcap=0 #Wireshark Capture (ON:1/OFF:0)
export enableAscii=0
export rxSensitivity=-90
export standard=11ac #IEEE Standard
export RngRun=${RngRunpar} #Iteration
export duration=10 #Simulation duration [s]
export enableFrameCapture=true #PHY Capture (ON:true/OFF:false)
export powerBackoff=0
export txStartOffset=50
export txGain=0
export rxGain=0
export txRange=102
export performTgaxTimingChecks=0
export sigma=0.0
export bianchi=1
export useIdealWifiManager=0
# OBSS-PD : Dynamic Carrier Sensing (11ax function)
export obssPdAlgorithm=ConstantObssPdAlgorithm
export enableObssPd=0
export obssPdThreshold=-74

#NetWork Parameters
export antennas=1 #Number of Antennas
export maxSupportedRxSpatialStreams=1 #MIMO Setting (DownLink)
export maxSupportedTxSpatialStreams=1 #MIMO Setting (UpLink)
export enableRts=0 #RTS/CTS (ON:1/OFF:0)
export bw=20 #BandWidth [Mhz]
export scenario=logdistance #Loss-function
export payloadSizeDownlink=0 #MSDU Size [Octet] (DownLink)
export payloadSizeUplink=1472 #MSDU Size [Octet] (UpLink)
export downlink=0 #Offered Load [Mbps] (DounLink)
export uplink=400 #Offered Load [Mbps] (UpLink) => saturation condition
export powSta=20 #STA TX Power [dBm]
export powAp=20 #AP TX Power [dBm]

#Topology Parameters
export nodePositionsFile=NONE
export nBss=2 #Number of BSSs
export d=5
export n=${npar}
export r1=8 #BSS1 Radius
export r2=8 #BSS2 Radius
export r3=8 #BSS3 Radius
export r4=8 #BSS4 Radius
export r5=8 #BSS5 Radius
export r6=8 #BSS6 Radius

#BSS Parameters
export MCS1=8 #BSS1 MCS 
export MCS2=8 #BSS2 MCS
export MCS3=8 #BSS3 MCS 
export MCS4=8 #BSS4 MCS
export MCS5=8 #BSS5 MCS 
export MCS6=8 #BSS6 MCS

export ccaTrSta1=-82 #CCA-ED_Threshold(BSS1_STA) CCA_CA Setting -> spatial-reuse.cc
export ccaTrAp1=-82 
export ccaTrSta2=-82 #CCA-ED_Threshold(BSS2_STA)
export ccaTrAp2=-82
export ccaTrSta3=-82 #CCA-ED_Threshold(BSS3_STA)
export ccaTrAp3=-82 
export ccaTrSta4=-82 #CCA-ED_Threshold(BSS4_STA)
export ccaTrAp4=-82 
export ccaTrSta5=-82 #CCA-ED_Threshold(BSS5_STA)
export ccaTrAp5=-82 
export ccaTrSta6=-82 #CCA-ED_Threshold(BSS6_STA)
export ccaTrAp6=-82  

#Max A-MPDU Size [Octet]
export maxAmpduSizeBss1=65535
export maxAmpduSizeBss2=65535
export maxAmpduSizeBss3=65535
export maxAmpduSizeBss4=65535
export maxAmpduSizeBss5=65535
export maxAmpduSizeBss6=65535
export maxAmpduSizeBss7=65535

export test="${Res}_Ite${RngRun}_STA${n}"
run_one 

wait
done
done

