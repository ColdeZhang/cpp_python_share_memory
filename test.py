from PyShareMemory.ShareMemory import ShareMemory

import numpy as np
import time
import os
import cv2

pid = os.getpid()

share = ShareMemory(13, pid)

def some_ai(data):
    # time.sleep(10)
    print("[Python]: some_ai worked it out!!")
    return "success"

share.do(some_ai)


input("[Python]: 程序运行中，按回车退出...\n")
