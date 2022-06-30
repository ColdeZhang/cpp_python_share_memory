from PyShareMemory.ShareMemory import ShareMemory

import numpy as np
import time
import os
import cv2

share = ShareMemory(12331)

i = 0
while True:
    try:
        if share.get_share_status() == 1:  # 存图结束，允许读图
            # 接受图像
            img = share.get_data()

            share.set_share_can_write()

            # 保存图像 按次序命名
            #cv2.imwrite("./img/{}.jpg".format(str(i)), Py_img)
            #print("save image")
            i += 1

        if share.get_share_status() == 3:
            break
    except Exception as e:
        print(e)
        share.destroy_share()
        break
