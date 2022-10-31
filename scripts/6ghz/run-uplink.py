#   Copyright (c) 2022 University of Washington

#   This program is free software; you can redistribute it and/or modify
#   it under the terms of the GNU General Public License version 2 as
#   published by the Free Software Foundation;

#   This program is distributed in the hope that it will be useful,
#   but WITHOUT ANY WARRANTY; without even the implied warranty of
#   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#   GNU General Public License for more details.

#   You should have received a copy of the GNU General Public License
#   along with this program; if not, write to the Free Software
#   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
import os
import signal
import time
import math

bandwidth = 20
dis = 0
freq = 5
Mcs = 5
server_core = 4


def ns3_run(band, distance):
    dis = distance
    while (1):
        out=os.popen("ps -ef|grep \"./waf --run\"|grep -v grep").read()
        count = 0
        for line in out.splitlines():
            count = count + 1
        if count < server_core:
            freq = 5
            run_command = 'nohup ./waf --run \"scratch/uplink --standard=11ax  --verbose=0 --duration=200 --maxMpdus=1 --infra=1 --validate=0 --channelWidth={:d}  --phyMode=HeMcs{:d} --frequency={:d} --distance={:d}\" > uplink_data/mpdu1_band{:d}_mcs{:d}_fre{:d}_dis{:d} 2>&1 &'.format(int(bandwidth), int(Mcs), int(freq), int(dis), int(bandwidth), int(Mcs), int(freq), int(dis))
            os.system(run_command)
            time.sleep(12)
            freq = 6
            run_command = 'nohup ./waf --run \"scratch/uplink --standard=11ax  --verbose=0 --duration=200 --maxMpdus=1 --infra=1 --validate=0 --channelWidth={:d}  --phyMode=HeMcs{:d} --frequency={:d} --distance={:d}\" > uplink_data/mpdu1_band{:d}_mcs{:d}_fre{:d}_dis{:d} 2>&1 &'.format(int(bandwidth), int(Mcs), int(freq), int(dis), int(bandwidth), int(Mcs), int(freq), int(dis))
            os.system(run_command)
            break
        time.sleep(75)


# 20 Mhz Bandwidth
bandwidth = 20
for i in range(0,700,50):
    ns3_run(bandwidth,i)

# 40 Mhz Bandwidth
bandwidth = 40
for i in range(0,700,50):
    ns3_run(bandwidth,i)


# 80 Mhz Bandwidth
bandwidth = 80
for i in range(0,700,50):
    ns3_run(bandwidth,i)


# 160 Mhz Bandwidth
bandwidth = 160
for i in range(0,700,50):
    ns3_run(bandwidth,i)