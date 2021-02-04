from py_interface import *
from ctypes import *
from argparse import ArgumentParser
import time


class DcfRlEnv(Structure):
    # _pack_ = 1
    _fields_ = [
        ('time', c_int64),
        ('power', c_double),
        ('idx', c_uint32),
    ]


class DcfRlAct(Structure):
    _fields_ = [
        ('ccaBusy', c_bool),
    ]


parser = ArgumentParser()
parser.add_argument('--rl', type=int, default=1)
arg = parser.parse_args()
print(arg)
# sett = dict(UseRl=arg.rl)
exp = Experiment(1234, 4096, "dcf-wifi", '../../')
rl = Ns3AIRL(1357, DcfRlEnv, DcfRlAct)
for _ in range(1):
    exp.reset()
    exp.run(show_output=True)
    # exp.run(setting=sett, show_output=True)
    while not rl.isFinish():
        with rl as data:
            if not data:
                break
            # print(data.env.idx, data.env.power)
            data.act.ccaBusy = c_bool(data.env.idx!=1)
            # data.act.ccaBusy = c_bool(0)
    time.sleep(5)
    exp.kill()
del exp