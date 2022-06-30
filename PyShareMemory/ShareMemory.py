import copy
import ctypes
import os
import time
import numpy as np

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
    def __init__(self, share_memory_key: int):
        share.create_share(share_memory_key)
        pass

    def get_share_body_size(self):
        return share.get_share_body_size()

    """
    从共享内存中获取数据
    """
    def get_data(self):
        share_body_ptr = share.get_share_body_address()
        py_data_recv = ctypes.cast(share_body_ptr, ctypes.POINTER(ctypes.c_uint8 * self.get_share_body_size())).contents
        self.set_share_can_write()
        # 数据转化成numpy数组
        # return np.array(py_data_recv, dtype=np.uint8)
        return py_data_recv

    """
    向共享内存中写入数据
    """
    def put_data(self, data):
        share.put_body(data.ctypes.data_as(ctypes.c_char_p))
        # 读取完成
        share.set_flag_can_write()

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
    设置内存状态为可写入
    """
    def set_share_can_write(self):
        share.set_flag_can_write()

    """
    获取内存状态
    """
    def get_share_status(self):
        return share.flag_status()

    """
    销毁共享内存
    """
    def destroy_share(self):
        share.destroy_share()
    pass
