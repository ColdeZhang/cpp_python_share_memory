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


class ShareMemory(object):
    work: callable

    def __init__(self, share_memory_key: int):
        share.create_share(share_memory_key)
        signal.signal(signal.SIGUSR1, self.job)
        pass

    def do(self, job: callable):
        self.work = job

    def job(self):
        self.set_share_working()
        data = self.get_data()
        res = self.work(data)
        self.write_result(res)
        self.set_share_done()


    def get_share_body_size(self):
        return share.get_share_body_size()

    """
    从共享内存中获取数据
    """
    def get_data(self):
        share_body_ptr = share.get_share_body_address()
        py_data_recv = ctypes.cast(share_body_ptr, ctypes.POINTER(ctypes.c_uint8 * self.get_share_body_size())).contents
        return py_data_recv

    """
    向共享内存中写入数据
    """
    def write_result(self, data):
        share.write_result(data.ctypes.data_as(ctypes.c_char_p))

    """
    获取共享内存里图片高
    """
    def get_height(self):
        return share.get_img_rows()

    """
    获取共享内存里图片宽
    """
    def get_width(self):
        return share.get_img_cols()

    """
    设置内存状态为任务完成
    """
    def set_share_done(self):
        share.set_status_done()

    """
    设置状态为处理任务中
    """
    def set_share_working(self):
        share.set_status_working()

    """
    销毁共享内存
    """
    def destroy_share(self):
        share.destroy_share()

    pass
