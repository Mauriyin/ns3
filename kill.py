import os
import signal
import time
import math

out=os.popen("ps -ef|grep \"./waf --run\"|grep -v grep").read()
for line in out.splitlines():
    fields = line.split() 
              
            # extracting Process ID from the output 
    pid = fields[1]   
    # terminating process  
    os.kill(int(pid), signal.SIGKILL)  

out=os.popen("ps -ef|grep \"wifi-unequal\"|grep -v grep").read()
for line in out.splitlines():
    fields = line.split() 
              
            # extracting Process ID from the output 
    pid = fields[1]   
    # terminating process  
    os.kill(int(pid), signal.SIGKILL)  