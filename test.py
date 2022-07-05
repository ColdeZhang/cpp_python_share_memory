from PyShareMemory.ShareMemory import ShareMemory

import numpy as np
import time
import os
import cv2

pid = os.getpid()

share = ShareMemory(2615, pid)


def some_ai(data, w, h):
    print("[AI Module]: w - %d, h - %d" % (w, h))
    size = (share.get_height(), share.get_width(), share.get_chanel())
    npdtata = np.array(data, dtype=np.uint8)
    img_data = np.reshape(npdtata, size)
    # 按顺序命名保存图片到本地
    cv2.imwrite('./img/'+str(time.time()) + '.jpg', img_data)
    # time.sleep(1)
    print("[AI Module]: some_ai worked it out!!")
    return "[AI Module]: i did it success !!!"

share.do(some_ai)


while(True):
    print("[Python]: ++++++++++心跳包，表明程序活着++++++++++", time.time())
    time.sleep(5)
