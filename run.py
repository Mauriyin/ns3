import os
import signal
import time
import math

bandwidth = 20
dis = 10
freq = 5
Mcs = 5

for i in range(1):
    bandwidth = 20 * math.pow(2, i)
    for j in range(1):
        Mcs = 5 + j
        for k in range(10):
            dis = 185 + 20 * k 
            while (1):
                out=os.popen("ps -ef|grep \"./waf --run\"|grep -v grep").read()
                count = 0
                for line in out.splitlines():
                    count = count + 1
                if count < 4:
                    freq = 5
                    run_command = 'nohup time ./waf --run \"scratch/uplink --standard=11ax  --verbose=0 --duration=200 --maxMpdus=5 --infra=1 --validate=0 --channelWidth={:d}  --phyMode=HeMcs{:d} --frequency={:d} --distance={:d}\" > uplink_data/band{:d}_mcs{:d}_fre{:d}_dis{:d} 2>&1 &'.format(int(bandwidth), int(Mcs), int(freq), int(dis), int(bandwidth), int(Mcs), int(freq), int(dis))
                    os.system(run_command)
                    freq = 6
                    run_command = 'nohup time ./waf --run \"scratch/uplink --standard=11ax  --verbose=0 --duration=200 --maxMpdus=5 --infra=1 --validate=0 --channelWidth={:d}  --phyMode=HeMcs{:d} --frequency={:d} --distance={:d}\" > uplink_data/band{:d}_mcs{:d}_fre{:d}_dis{:d} 2>&1 &'.format(int(bandwidth), int(Mcs), int(freq), int(dis), int(bandwidth), int(Mcs), int(freq), int(dis))
                    os.system(run_command)
                    break
                time.sleep(300)
                print (time.time())
                print (count)

