from PyShareMemory.ShareMemory import ShareMemory

import numpy as np
import time
import os
import cv2

share = ShareMemory(12111)

i = 0
while True:
    try:
        share.do(lambda data: "success")
    except Exception as e:
        print(e)
        share.destroy_share()
        break
