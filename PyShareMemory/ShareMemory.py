import copy
import ctypes
import os
import time
import numpy as np
import signal

print("[Python]: 开始加载共享内存动态库...")
libLoad = ctypes.cdll.LoadLibrary
try:
    module_root_path = os.path.dirname(__file__)
    share = libLoad(os.path.join(module_root_path, "libsharememory.so"))
    print("[Python]: 加载共享内存动态库成功！")
    share.get_share_body_address.restype = ctypes.POINTER(ctypes.c_uint8)
except Exception as e:
    print("[Python]: 加载共享内存动态库失败！可能是由于没有将 libsharememory.so 放置在PyShareMemory模块目录下！")
    print("[Python]: 详细的错误信息-" + str(e))
    exit(1)


def blank_work():
    return "[来自Python的信息]: Null，没有传入工作函数！"


class ShareMemory(object):
    work = blank_work()

    def __init__(self, share_memory_key: int, slave_pid: int):
        share.create_share(share_memory_key, slave_pid)
        signal.signal(signal.SIGUSR1, self.job)
        pass

    def do(self, work_to_do):
        self.work = work_to_do

    def job(self, signum, frame):
        share.set_status_working()
        print('[Python]: 接收到传输完成的信号，开始处理...')
        data = self.get_data()
        res = self.work(data, self.get_width(), self.get_height())
        print('[Python]: 处理完成，开始向内存写入结果')
        share.write_result(ctypes.c_char_p(res.encode('utf-8')))
        share.set_status_done()
        print('[Python]: 写入结束，内存标志位已修改为JOB_DONE')
        print('[Python]: 本次任务结束。')

    # 获取数据总大小
    def get_share_body_size(self):
        return share.get_share_body_size()

    # 从共享内存中获取数据
    def get_data(self):
        share_body_ptr = share.get_share_body_address()
        py_data_recv = ctypes.cast(share_body_ptr, ctypes.POINTER(ctypes.c_uint8 * self.get_share_body_size())).contents
        return py_data_recv

    # 获取图片的高
    def get_height(self):
        return share.get_img_rows()

    # 获取图片的宽
    def get_width(self):
        return share.get_img_cols()

    # 获取图片通道
    def get_chanel(self):
        return share.get_img_channels()

    pass
