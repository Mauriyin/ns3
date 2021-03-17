import os
import signal
import time
import math

bandwidth = 20
dis = 10
freq = 5
Mcs = 5
server_core = 9
# for i in range(1):
#     bandwidth = 20 * math.pow(2, (i))
#     for j in range(1):
#         Mcs = 5 + j
#         for k in range(5):
#             dis = 600 + 50 *(k+1)
#             while (1):
#                 out=os.popen("ps -ef|grep \"./waf --run\"|grep -v grep").read()
#                 count = 0
#                 for line in out.splitlines():
#                     count = count + 1
#                 if count < server_core:
#                     freq = 5
#                     run_command = 'nohup ./waf --run \"scratch/wifi-unequal --standard=11ax  --verbose=0 --duration=200 --maxMpdus=1 --infra=1 --validate=0 --channelWidth={:d}  --phyMode=HeMcs{:d} --frequency={:d} --distance={:d}\" > uneqaul_data/mpdu1_band{:d}_mcs{:d}_fre{:d}_dis{:d} 2>&1 &'.format(int(bandwidth), int(Mcs), int(freq), int(dis), int(bandwidth), int(Mcs), int(freq), int(dis))
#                     os.system(run_command)
#                     freq = 6
#                     run_command = 'nohup ./waf --run \"scratch/wifi-unequal --standard=11ax  --verbose=0 --duration=200 --maxMpdus=1 --infra=1 --validate=0 --channelWidth={:d}  --phyMode=HeMcs{:d} --frequency={:d} --distance={:d}\" > uneqaul_data/mpdu1_band{:d}_mcs{:d}_fre{:d}_dis{:d} 2>&1 &'.format(int(bandwidth), int(Mcs), int(freq), int(dis), int(bandwidth), int(Mcs), int(freq), int(dis))
#                     os.system(run_command)
#                     break
#                 time.sleep(200)
#                 print (time.time())
#                 print (count)

# for i in range(1):
#     bandwidth = 20 * math.pow(2, (i))
#     for j in range(1):
#         Mcs = 5 + j
#         for k in range(10):
#             dis = 580 + 5*(k+1)
#             while (1):
#                 out=os.popen("ps -ef|grep \"./waf --run\"|grep -v grep").read()
#                 count = 0
#                 for line in out.splitlines():
#                     count = count + 1
#                 if count < server_core:
#                     freq = 5
#                     run_command = 'nohup ./waf --run \"scratch/wifi-unequal --standard=11ax  --verbose=0 --duration=200 --maxMpdus=1 --infra=1 --validate=0 --channelWidth={:d}  --phyMode=HeMcs{:d} --frequency={:d} --distance={:d}\" > uneqaul_data/mpdu1_band{:d}_mcs{:d}_fre{:d}_dis{:d} 2>&1 &'.format(int(bandwidth), int(Mcs), int(freq), int(dis), int(bandwidth), int(Mcs), int(freq), int(dis))
#                     os.system(run_command)
#                     freq = 6
#                     run_command = 'nohup ./waf --run \"scratch/wifi-unequal --standard=11ax  --verbose=0 --duration=200 --maxMpdus=1 --infra=1 --validate=0 --channelWidth={:d}  --phyMode=HeMcs{:d} --frequency={:d} --distance={:d}\" > uneqaul_data/mpdu1_band{:d}_mcs{:d}_fre{:d}_dis{:d} 2>&1 &'.format(int(bandwidth), int(Mcs), int(freq), int(dis), int(bandwidth), int(Mcs), int(freq), int(dis))
#                     os.system(run_command)
#                     break
#                 time.sleep(200)
#                 print (time.time())
#                 print (count)


# for i in range(1):
#     bandwidth = 20 * math.pow(2, (1))
#     for j in range(1):
#         Mcs = 5 + j
#         for k in range(5):
#             dis = 450 + 50*(k+1)
#             while (1):
#                 out=os.popen("ps -ef|grep \"./waf --run\"|grep -v grep").read()
#                 count = 0
#                 for line in out.splitlines():
#                     count = count + 1
#                 if count < server_core:
#                     freq = 5
#                     run_command = 'nohup ./waf --run \"scratch/wifi-unequal --standard=11ax  --verbose=0 --duration=200 --maxMpdus=1 --infra=1 --validate=0 --channelWidth={:d}  --phyMode=HeMcs{:d} --frequency={:d} --distance={:d}\" > uneqaul_data/mpdu1_band{:d}_mcs{:d}_fre{:d}_dis{:d} 2>&1 &'.format(int(bandwidth), int(Mcs), int(freq), int(dis), int(bandwidth), int(Mcs), int(freq), int(dis))
#                     os.system(run_command)
#                     # freq = 6
#                     # run_command = 'nohup ./waf --run \"scratch/wifi-unequal --standard=11ax  --verbose=0 --duration=200 --maxMpdus=1 --infra=1 --validate=0 --channelWidth={:d}  --phyMode=HeMcs{:d} --frequency={:d} --distance={:d}\" > uneqaul_data/mpdu1_band{:d}_mcs{:d}_fre{:d}_dis{:d} 2>&1 &'.format(int(bandwidth), int(Mcs), int(freq), int(dis), int(bandwidth), int(Mcs), int(freq), int(dis))
#                     # os.system(run_command)
#                     break
#                 time.sleep(200)
#                 print (time.time())
#                 print (count)

for i in range(1):
    bandwidth = 20 * math.pow(2, (1))
    for j in range(1):
        Mcs = 5 + j
        for k in range(3):
            dis = 700 + 50*(k+1)
            while (1):
                out=os.popen("ps -ef|grep \"./waf --run\"|grep -v grep").read()
                count = 0
                for line in out.splitlines():
                    count = count + 1
                if count < server_core:
                    freq = 5
                    run_command = 'nohup ./waf --run \"scratch/wifi-unequal --standard=11ax  --verbose=0 --duration=200 --maxMpdus=1 --infra=1 --validate=0 --channelWidth={:d}  --phyMode=HeMcs{:d} --frequency={:d} --distance={:d}\" > uneqaul_data/mpdu1_band{:d}_mcs{:d}_fre{:d}_dis{:d} 2>&1 &'.format(int(bandwidth), int(Mcs), int(freq), int(dis), int(bandwidth), int(Mcs), int(freq), int(dis))
                    os.system(run_command)
                    # freq = 6
                    # run_command = 'nohup ./waf --run \"scratch/wifi-unequal --standard=11ax  --verbose=0 --duration=200 --maxMpdus=1 --infra=1 --validate=0 --channelWidth={:d}  --phyMode=HeMcs{:d} --frequency={:d} --distance={:d}\" > uneqaul_data/mpdu1_band{:d}_mcs{:d}_fre{:d}_dis{:d} 2>&1 &'.format(int(bandwidth), int(Mcs), int(freq), int(dis), int(bandwidth), int(Mcs), int(freq), int(dis))
                    # os.system(run_command)
                    break
                time.sleep(200)
                print (time.time())
                print (count)

# for i in range(1):
#     bandwidth = 20 * math.pow(2, (1))
#     for j in range(1):
#         Mcs = 5 + j
#         for k in range(10):
#             dis = 100 + 5*(k+1)
#             while (1):
#                 out=os.popen("ps -ef|grep \"./waf --run\"|grep -v grep").read()
#                 count = 0
#                 for line in out.splitlines():
#                     count = count + 1
#                 if count < server_core:
#                     freq = 5
#                     run_command = 'nohup ./waf --run \"scratch/wifi-unequal --standard=11ax  --verbose=0 --duration=200 --maxMpdus=1 --infra=1 --validate=0 --channelWidth={:d}  --phyMode=HeMcs{:d} --frequency={:d} --distance={:d}\" > uneqaul_data/mpdu1_band{:d}_mcs{:d}_fre{:d}_dis{:d} 2>&1 &'.format(int(bandwidth), int(Mcs), int(freq), int(dis), int(bandwidth), int(Mcs), int(freq), int(dis))
#                     os.system(run_command)
#                     freq = 6
#                     run_command = 'nohup ./waf --run \"scratch/wifi-unequal --standard=11ax  --verbose=0 --duration=200 --maxMpdus=1 --infra=1 --validate=0 --channelWidth={:d}  --phyMode=HeMcs{:d} --frequency={:d} --distance={:d}\" > uneqaul_data/mpdu1_band{:d}_mcs{:d}_fre{:d}_dis{:d} 2>&1 &'.format(int(bandwidth), int(Mcs), int(freq), int(dis), int(bandwidth), int(Mcs), int(freq), int(dis))
#                     os.system(run_command)
#                     break
#                 time.sleep(200)
#                 print (time.time())
#                 print (count)

for i in range(1):
    bandwidth = 20 * math.pow(2, (1))
    for j in range(1):
        Mcs = 5 + j
        for k in range(4):
            dis = 300+ 10*(k+1)
            while (1):
                out=os.popen("ps -ef|grep \"./waf --run\"|grep -v grep").read()
                count = 0
                for line in out.splitlines():
                    count = count + 1
                if count < server_core:
                    freq = 5
                    run_command = 'nohup ./waf --run \"scratch/wifi-unequal --standard=11ax  --verbose=0 --duration=200 --maxMpdus=1 --infra=1 --validate=0 --channelWidth={:d}  --phyMode=HeMcs{:d} --frequency={:d} --distance={:d}\" > uneqaul_data/mpdu1_band{:d}_mcs{:d}_fre{:d}_dis{:d} 2>&1 &'.format(int(bandwidth), int(Mcs), int(freq), int(dis), int(bandwidth), int(Mcs), int(freq), int(dis))
                    os.system(run_command)
                    # freq = 6
                    # run_command = 'nohup ./waf --run \"scratch/wifi-unequal --standard=11ax  --verbose=0 --duration=200 --maxMpdus=1 --infra=1 --validate=0 --channelWidth={:d}  --phyMode=HeMcs{:d} --frequency={:d} --distance={:d}\" > uneqaul_data/mpdu1_band{:d}_mcs{:d}_fre{:d}_dis{:d} 2>&1 &'.format(int(bandwidth), int(Mcs), int(freq), int(dis), int(bandwidth), int(Mcs), int(freq), int(dis))
                    # os.system(run_command)
                    break
                time.sleep(200)
                print (time.time())
                print (count)


# for i in range(1):
#     bandwidth = 20 * math.pow(2, (2))
#     for j in range(1):
#         Mcs = 5 + j
#         for k in range(5):
#             dis = 300 + 50*(k+1)
#             while (1):
#                 out=os.popen("ps -ef|grep \"./waf --run\"|grep -v grep").read()
#                 count = 0
#                 for line in out.splitlines():
#                     count = count + 1
#                 if count < server_core:
#                     freq = 5
#                     run_command = 'nohup ./waf --run \"scratch/wifi-unequal --standard=11ax  --verbose=0 --duration=200 --maxMpdus=1 --infra=1 --validate=0 --channelWidth={:d}  --phyMode=HeMcs{:d} --frequency={:d} --distance={:d}\" > uneqaul_data/mpdu1_band{:d}_mcs{:d}_fre{:d}_dis{:d} 2>&1 &'.format(int(bandwidth), int(Mcs), int(freq), int(dis), int(bandwidth), int(Mcs), int(freq), int(dis))
#                     os.system(run_command)
#                     freq = 6
#                     run_command = 'nohup ./waf --run \"scratch/wifi-unequal --standard=11ax  --verbose=0 --duration=200 --maxMpdus=1 --infra=1 --validate=0 --channelWidth={:d}  --phyMode=HeMcs{:d} --frequency={:d} --distance={:d}\" > uneqaul_data/mpdu1_band{:d}_mcs{:d}_fre{:d}_dis{:d} 2>&1 &'.format(int(bandwidth), int(Mcs), int(freq), int(dis), int(bandwidth), int(Mcs), int(freq), int(dis))
#                     os.system(run_command)
#                     break
#                 time.sleep(200)
#                 print (time.time())
#                 print (count)

# # for i in range(1):
# #     bandwidth = 20 * math.pow(2, (2))
# #     for j in range(1):
# #         Mcs = 5 + j
# #         for k in range(10):
# #             dis = 100 + 5*(k+1)
# #             while (1):
# #                 out=os.popen("ps -ef|grep \"./waf --run\"|grep -v grep").read()
# #                 count = 0
# #                 for line in out.splitlines():
# #                     count = count + 1
# #                 if count < server_core:
# #                     freq = 5
# #                     run_command = 'nohup ./waf --run \"scratch/wifi-unequal --standard=11ax  --verbose=0 --duration=200 --maxMpdus=1 --infra=1 --validate=0 --channelWidth={:d}  --phyMode=HeMcs{:d} --frequency={:d} --distance={:d}\" > uneqaul_data/mpdu1_band{:d}_mcs{:d}_fre{:d}_dis{:d} 2>&1 &'.format(int(bandwidth), int(Mcs), int(freq), int(dis), int(bandwidth), int(Mcs), int(freq), int(dis))
# #                     os.system(run_command)
# #                     freq = 6
# #                     run_command = 'nohup ./waf --run \"scratch/wifi-unequal --standard=11ax  --verbose=0 --duration=200 --maxMpdus=1 --infra=1 --validate=0 --channelWidth={:d}  --phyMode=HeMcs{:d} --frequency={:d} --distance={:d}\" > uneqaul_data/mpdu1_band{:d}_mcs{:d}_fre{:d}_dis{:d} 2>&1 &'.format(int(bandwidth), int(Mcs), int(freq), int(dis), int(bandwidth), int(Mcs), int(freq), int(dis))
# #                     os.system(run_command)
# #                     break
# #                 time.sleep(200)
# #                 print (time.time())
# #                 print (count)

# # for i in range(1):
# #     bandwidth = 20 * math.pow(2, (2))
# #     for j in range(1):
# #         Mcs = 5 + j
# #         for k in range(10):
# #             dis = 250 + 5*(k+1)
# #             while (1):
# #                 out=os.popen("ps -ef|grep \"./waf --run\"|grep -v grep").read()
# #                 count = 0
# #                 for line in out.splitlines():
# #                     count = count + 1
# #                 if count < server_core:
# #                     freq = 5
# #                     run_command = 'nohup ./waf --run \"scratch/wifi-unequal --standard=11ax  --verbose=0 --duration=200 --maxMpdus=1 --infra=1 --validate=0 --channelWidth={:d}  --phyMode=HeMcs{:d} --frequency={:d} --distance={:d}\" > uneqaul_data/mpdu1_band{:d}_mcs{:d}_fre{:d}_dis{:d} 2>&1 &'.format(int(bandwidth), int(Mcs), int(freq), int(dis), int(bandwidth), int(Mcs), int(freq), int(dis))
# #                     os.system(run_command)
# #                     freq = 6
# #                     run_command = 'nohup ./waf --run \"scratch/wifi-unequal --standard=11ax  --verbose=0 --duration=200 --maxMpdus=1 --infra=1 --validate=0 --channelWidth={:d}  --phyMode=HeMcs{:d} --frequency={:d} --distance={:d}\" > uneqaul_data/mpdu1_band{:d}_mcs{:d}_fre{:d}_dis{:d} 2>&1 &'.format(int(bandwidth), int(Mcs), int(freq), int(dis), int(bandwidth), int(Mcs), int(freq), int(dis))
# #                     os.system(run_command)
# #                     break
# #                 time.sleep(200)
# #                 print (time.time())
# #                 print (count)

# for i in range(1):
#     bandwidth = 20 * math.pow(2, (3))
#     for j in range(1):
#         Mcs = 5 + j
#         for k in range(5):
#             dis = 250 + 50*(k+1)
#             while (1):
#                 out=os.popen("ps -ef|grep \"./waf --run\"|grep -v grep").read()
#                 count = 0
#                 for line in out.splitlines():
#                     count = count + 1
#                 if count < server_core:
#                     freq = 5
#                     run_command = 'nohup ./waf --run \"scratch/wifi-unequal --standard=11ax  --verbose=0 --duration=200 --maxMpdus=1 --infra=1 --validate=0 --channelWidth={:d}  --phyMode=HeMcs{:d} --frequency={:d} --distance={:d}\" > uneqaul_data/mpdu1_band{:d}_mcs{:d}_fre{:d}_dis{:d} 2>&1 &'.format(int(bandwidth), int(Mcs), int(freq), int(dis), int(bandwidth), int(Mcs), int(freq), int(dis))
#                     os.system(run_command)
#                     freq = 6
#                     run_command = 'nohup ./waf --run \"scratch/wifi-unequal --standard=11ax  --verbose=0 --duration=200 --maxMpdus=1 --infra=1 --validate=0 --channelWidth={:d}  --phyMode=HeMcs{:d} --frequency={:d} --distance={:d}\" > uneqaul_data/mpdu1_band{:d}_mcs{:d}_fre{:d}_dis{:d} 2>&1 &'.format(int(bandwidth), int(Mcs), int(freq), int(dis), int(bandwidth), int(Mcs), int(freq), int(dis))
#                     os.system(run_command)
#                     break
#                 time.sleep(200)
#                 print (time.time())
#                 print (count)

# # for i in range(1):
# #     bandwidth = 20 * math.pow(2, (3))
# #     for j in range(1):
# #         Mcs = 5 + j
# #         for k in range(10):
# #             dis = 100 + 5*(k+1)
# #             while (1):
# #                 out=os.popen("ps -ef|grep \"./waf --run\"|grep -v grep").read()
# #                 count = 0
# #                 for line in out.splitlines():
# #                     count = count + 1
# #                 if count < server_core:
# #                     freq = 5
# #                     run_command = 'nohup ./waf --run \"scratch/wifi-unequal --standard=11ax  --verbose=0 --duration=200 --maxMpdus=1 --infra=1 --validate=0 --channelWidth={:d}  --phyMode=HeMcs{:d} --frequency={:d} --distance={:d}\" > uneqaul_data/mpdu1_band{:d}_mcs{:d}_fre{:d}_dis{:d} 2>&1 &'.format(int(bandwidth), int(Mcs), int(freq), int(dis), int(bandwidth), int(Mcs), int(freq), int(dis))
# #                     os.system(run_command)
# #                     freq = 6
# #                     run_command = 'nohup ./waf --run \"scratch/wifi-unequal --standard=11ax  --verbose=0 --duration=200 --maxMpdus=1 --infra=1 --validate=0 --channelWidth={:d}  --phyMode=HeMcs{:d} --frequency={:d} --distance={:d}\" > uneqaul_data/mpdu1_band{:d}_mcs{:d}_fre{:d}_dis{:d} 2>&1 &'.format(int(bandwidth), int(Mcs), int(freq), int(dis), int(bandwidth), int(Mcs), int(freq), int(dis))
# #                     os.system(run_command)
# #                     break
# #                 time.sleep(200)
# #                 print (time.time())
# #                 print (count)

# # for i in range(1):
# #     bandwidth = 20 * math.pow(2, (3))
# #     for j in range(1):
# #         Mcs = 5 + j
# #         for k in range(10):
# #             dis = 200 + 5*(k+1)
# #             while (1):
# #                 out=os.popen("ps -ef|grep \"./waf --run\"|grep -v grep").read()
# #                 count = 0
# #                 for line in out.splitlines():
# #                     count = count + 1
# #                 if count < server_core:
# #                     freq = 5
# #                     run_command = 'nohup ./waf --run \"scratch/wifi-unequal --standard=11ax  --verbose=0 --duration=200 --maxMpdus=1 --infra=1 --validate=0 --channelWidth={:d}  --phyMode=HeMcs{:d} --frequency={:d} --distance={:d}\" > uneqaul_data/mpdu1_band{:d}_mcs{:d}_fre{:d}_dis{:d} 2>&1 &'.format(int(bandwidth), int(Mcs), int(freq), int(dis), int(bandwidth), int(Mcs), int(freq), int(dis))
# #                     os.system(run_command)
# #                     freq = 6
# #                     run_command = 'nohup ./waf --run \"scratch/wifi-unequal --standard=11ax  --verbose=0 --duration=200 --maxMpdus=1 --infra=1 --validate=0 --channelWidth={:d}  --phyMode=HeMcs{:d} --frequency={:d} --distance={:d}\" > uneqaul_data/mpdu1_band{:d}_mcs{:d}_fre{:d}_dis{:d} 2>&1 &'.format(int(bandwidth), int(Mcs), int(freq), int(dis), int(bandwidth), int(Mcs), int(freq), int(dis))
# #                     os.system(run_command)
# #                     break
# #                 time.sleep(200)
# #                 print (time.time())
# #                 print (count)