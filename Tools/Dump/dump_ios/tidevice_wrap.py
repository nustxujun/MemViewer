from tidevice import Usbmux
from tidevice import Device
import os

product_type_map = {
    "iPhone1,1": "iPhone",
    "iPhone1,2": "iPhone3G",
    "iPhone2,1": "iPhone3GS",
    "iPhone3,1": "iPhone4",
    "iPhone3,2": "iPhone4",
    "iPhone3,3": "iPhone4",
    "iPhone4,1": "iPhone4S",
    "iPhone5,1": "iPhone5",
    "iPhone5,2": "iPhone5",
    "iPhone5,3": "iPhone5c",
    "iPhone5,4": "iPhone5c",
    "iPhone6,1": "iPhone5s",
    "iPhone6,2": "iPhone5s",
    "iPhone7,2": "iPhone6",
    "iPhone7,1": "iPhone6Plus",
    "iPhone8,1": "iPhone6s",
    "iPhone8,2": "iPhone6sPlus",
    "iPhone8,4": "iPhoneSE_1Gen",
    "iPhone9,1": "iPhone7",
    "iPhone9,3": "iPhone7",
    "iPhone9,2": "iPhone7Plus",
    "iPhone9,4": "iPhone7Plus",
    "iPhone10,1": "iPhone8",
    "iPhone10,4": "iPhone8",
    "iPhone10,2": "iPhone8Plus",
    "iPhone10,5": "iPhone8Plus",
    "iPhone10,3": "iPhoneX",
    "iPhone10,6": "iPhoneX",
    "iPhone11,2": "iPhoneXS",
    "iPhone11,4": "iPhoneXSMax",
    "iPhone11,6": "iPhoneXSMax",
    "iPhone11,8": "iPhoneXR",
    "iPhone12,1": "iPhone11",
    "iPhone12,3": "iPhone11Pro",
    "iPhone12,5": "iPhone11ProMax",
    "iPhone12,8": "iPhoneSE_2Gen",
    "iPhone13,1": "iPhone12mini",
    "iPhone13,2": "iPhone12",
    "iPhone13,3": "iPhone12Pro",
    "iPhone13,4": "iPhone12ProMax",
    "iPhone14,4": "iPhone13mini",
    "iPhone14,5": "iPhone13",
    "iPhone14,2": "iPhone13Pro",
    "iPhone14,3": "iPhone13ProMax",
    "iPhone14,6": "iPhoneSE_3Gen",
    "iPhone14,7": "iPhone14",
    "iPhone14,8": "iPhone14Plus",
    "iPhone15,2": "iPhone14Pro",
    "iPhone15,3": "iPhone14ProMax",
    "iPhone15,4": "iPhone15",
    "iPhone15,5": "iPhone15Plus",
    "iPhone16,1": "iPhone15Pro",
    "iPhone16,2": "iPhone15ProMax",
    "iPhone17,3": "iPhone16",
    "iPhone17,4": "iPhone16Plus",
    "iPhone17,1": "iPhone16Pro",
    "iPhone17,2": "iPhone16ProMax",
    "iPhone17,5": "iPhone16e",

    "iPad4,1": "iPadAir",
    "iPad4,2": "iPadAir",
    "iPad4,3": "iPadAir",
    "iPad4,4": "iPadMini2",
    "iPad4,5": "iPadMini2",
    "iPad4,6": "iPadMini2",
    "iPad4,7": "iPadMini3",
    "iPad4,8": "iPadMini3",
    "iPad4,9": "iPadMini3",
    "iPad5,1": "iPadMini4",
    "iPad5,2": "iPadMini4",
    "iPhone5,3": "iPadAir2",
    "iPad5,4": "iPadAir2",
    
    "iPad6,3": "iPadPro9_7inch",
    "iPad6,4": "iPadPro9_7inch",
    "iPad6,7": "iPadPro12_9inch",
    "iPad6,8": "iPadPro12_9inch",
    "iPad6,11": "iPad_5Gen",
    "iPad6,12": "iPad_5Gen",

    "iPad7,1": "iPadPro12_9inch_2Gen",
    "iPad7,2": "iPadPro12_9inch_2Gen",
    "iPad7,3": "iPadPro10_5inch",
    "iPad7,4": "iPadPro10_5inch",
    "iPad7,5": "iPad_6Gen",
    "iPad7,6": "iPad_6Gen",
    "iPad7,11": "iPad_7Gen",
    "iPad7,12": "iPad_7Gen",

    "iPad8,1": "iPadPro11inch",
    "iPad8,2": "iPadPro11inch",
    "iPad8,3": "iPadPro11inch",
    "iPad8,4": "iPadPro11inch",
    "iPad8,5": "iPadPro12_9inch_3Gen",
    "iPad8,6": "iPadPro12_9inch_3Gen",
    "iPad8,7": "iPadPro12_9inch_3Gen",
    "iPad8,8": "iPadPro12_9inch_3Gen",
    "iPad8,9": "iPadPro11inch_2Gen",
    "iPad8,10": "iPadPro11inch_2Gen",
    "iPad8,11": "iPadPro12_9inch_4Gen",
    "iPad8,12": "iPadPro12_9inch_4Gen",

    "iPad11,1": "iPadmini5_5Gen",
    "iPad11,2": "iPadmini5_5Gen",
    "iPad11,3": "iPadAir_3Gen",
    "iPad11,4": "iPadAir_3Gen",
    "iPad11,6": "iPad_8Gen",
    "iPad11,7": "iPad_8Gen",

    "iPad12,1": "iPad_9Gen",
    "iPad12,2": "iPad_9Gen",


    "iPad13,1": "iPadAir_4Gen",
    "iPad13,2": "iPadAir_4Gen",
    "iPad13,4": "iPadPro11inch_3Gen",
    "iPad13,5": "iPadPro11inch_3Gen",
    "iPad13,6": "iPadPro11inch_3Gen",
    "iPad13,7": "iPadPro11inch_3Gen",
    "iPad13,8": "iPadPro12_9inch_5Gen",
    "iPad13,9": "iPadPro12_9inch_5Gen",
    "iPad13,10": "iPadPro12_9inch_5Gen",
    "iPad13,11": "iPadPro12_9inch_5Gen",

    "iPad13,16": "iPadAir_5Gen",
    "iPad13,17": "iPadAir_5Gen",

    "iPad13,19": "iPad_10Gen",

    "iPad14,1": "iPadmini_6Gen",
    "iPad14,2": "iPadmini_6Gen",
    "iPad14,3": "iPadPro11inch_4Gen",
    "iPad14,4": "iPadPro11inch_4Gen",
    "iPad14,5": "iPadPro12_9inch_6Gen",
    "iPad14,6": "iPadPro12_9inch_6Gen",

    "iPad14,8": "iPadAir11inchM2",
    "iPad14,9": "iPadAir11inchM2",
    "iPad14,10": "iPadAir13inchM2",
    "iPad14,11": "iPadAir13inchM2",

    "iPad15,3": "iPadAir11inchM3",
    "iPad15,4": "iPadAir11inchM3",
    "iPad15,5": "iPadAir13inchM3",
    "iPad15,6": "iPadAir13inchM3",
    "iPad15,7": "iPadA16",
    "iPad15,8": "iPadA16",
    
    "iPad16,1": "iPadminiA17Pro",
    "iPad16,2": "iPadminiA17Pro",
    "iPad16,3": "iPadPro11inchM4",
    "iPad16,4": "iPadPro11inchM4",
    "iPad16,5": "iPadPro13inchM4",
    "iPad16,6": "iPadPro13inchM4",
}



usbmux = Usbmux()


def list_devices():
 
    devices = usbmux.device_list()
    available_devices = []
    

    idx = 0
    for info in devices:
        device = Device(info.udid, usbmux)
        info_list = []
        type , os_ver = get_device_type_and_os_version(device)

        info_list.append(type)
        info_list.append(os_ver)
        info_list.append(info.udid)

        available_devices.append(info_list)
        idx += 1

    return available_devices

def get_device(device_udid):
    devices = usbmux.device_list()
    for info in devices:
        if device_udid == info.udid:
            return Device(info.udid, usbmux)
    return None

def is_app_installed(device,bundle_id):
    for info in device.installation.iter_installed(app_type="User"):
        if info["CFBundleIdentifier"] == bundle_id:
            return True
    return False

def is_file_exist(device,bundle_id, path):
    sync = device.app_sync(bundle_id)
    return sync.exists(path)

def list_dir(device,bundle_id, path):
    sync = device.app_sync(bundle_id)
    list = []
    for finfo in sync.listdir_info(path):
        filename = finfo.st_name
        list.append(filename)

    return list

def rmtree(device,bundle_id, path):
    sync = device.app_sync(bundle_id)
    sync.rmtree(path)
    print(f"delete {path}")

def push_data(device,bundle_id, dst_path, data):
    sync = device.app_sync(bundle_id)
    sync.push_content( dst_path,data)
    print(f"import file {dst_path}")

def pull_file(device, bundle_id, src_path, dst_path):
    sync = device.app_sync(bundle_id)
    sync.pull(src_path, dst_path)
    print(f"export file {src_path}")

def get_device_info(device):
    return device.get_value()

def get_file_stat(device, bundle_id, path):
    sync = device.app_sync(bundle_id)
    return sync.stat(path)

def get_device_type_and_os_version(device):
    info = get_device_info(device)
    product_type = info.get("ProductType", "")
    os_version = info.get("ProductVersion", "")
    return product_type_map.get(product_type, "Unknown"), os_version

def get_executable_name(device, bundle_id):
    return device.installation.lookup(bundle_id).get("CFBundleExecutable")