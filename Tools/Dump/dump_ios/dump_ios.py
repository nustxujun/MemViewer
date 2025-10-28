import os
import sys
import platform
import subprocess
import zipfile
import shutil
import requests
import argparse
import colorama
import configparser
import tidevice_wrap
from colorama import Fore, Back, Style

colorama.init()

def error(msg):
    print(f"{Fore.RED}{msg}{Style.RESET_ALL}")

def notice(msg):
    print(f"{Fore.YELLOW}{msg}{Style.RESET_ALL}")

def info(msg):
    print(f"{Fore.GREEN}{msg}{Style.RESET_ALL}")

def warn(msg):
    print(f"{Fore.YELLOW}{msg}{Style.RESET_ALL}")

upload_api = "http://9.134.132.170:8080/upload"

def get_exec_path():
    if getattr(sys, 'frozen', False):
        return os.path.dirname(os.path.abspath(sys.executable))
    else:
        return os.path.dirname(os.path.abspath(__file__))
    
workspace = get_exec_path()

def get_platform_info():
    OS = f"{platform.system()}{platform.release()}"

    result = subprocess.run(
        ['wmic', 'path', 'win32_VideoController', 'get', 'name'],
        capture_output=True, text=True
    )

    VC = ""
    for line in result.stdout.splitlines():
        if "NVIDIA" in line or "AMD" in line:
            VC = line.replace(" ","")
            break
    return VC, OS

def upload_trace_file(file_path):
    try:
        with open(file_path, 'rb') as file:
            files = {'file': file}
            response = requests.post(upload_api, files=files)
            response.raise_for_status()  # Raise an exception for HTTP errors
            print("trace 上传成功")
            return True
    except requests.exceptions.RequestException as e:
        print(f"trace 上传失败: {e}")
        return False



def dump_tracefiles(device, bundleid, workspace, output_path,symbol_path, tag_name):
    if not (tidevice_wrap.is_file_exist(device, bundleid,"/Documents/MemTrace")):
        warn("no trace files found")
        return
    
    def get_version(trace_dir):
        version_path = os.path.join(trace_dir, "versions")
        branch = "unknown"
        ver = "0.0.0"
        with open(version_path, "r") as f:
            branch = f.readline().strip()
            ver = f.readline().strip()
        return branch, ver
    

    product_type, product_version = tidevice_wrap.get_device_type_and_os_version(device)
    computer_name = os.uname().nodename
    
    dir = tidevice_wrap.list_dir(device, bundleid,"/Documents/MemTrace")

    if len(dir) == 0:
        warn("no trace files found")
        return

    traces_dir = os.path.join(output_path, "trace")
    os.makedirs(traces_dir, exist_ok=True)

    for folder in dir:
        temp_path= os.path.join(output_path,"temp")
        shutil.rmtree(temp_path,ignore_errors=True)
        os.makedirs(temp_path,exist_ok=True)

        files = tidevice_wrap.list_dir(device, bundleid,f"/Documents/MemTrace/{folder}")
        print(f"begin copy {folder}")
        for file_name in files:
            tidevice_wrap.pull_file(device, bundleid,f"/Documents/MemTrace/{folder}/{file_name}",os.path.join(temp_path, file_name))

        branch, version = get_version(temp_path)

        trace_name = f"{folder}_{branch}_{version}_{product_type}_{product_version}_{computer_name}_{tag_name}.memtrace"
        trace_path = os.path.join(output_path,"trace", trace_name)
        parsing_cmd = f'{os.path.join(workspace, "MemoryTraceParser")} {temp_path} {symbol_path} -o={trace_path} -q -e={tidevice_wrap.get_executable_name(device, bundleid)}'
        print(parsing_cmd)
        os.system(parsing_cmd)
        if os.path.exists(trace_path):
            print (f'begin zip {trace_name}')
            with zipfile.ZipFile(trace_path + ".zip", 'w', zipfile.ZIP_DEFLATED) as zipf:
                zipf.write(trace_path, trace_name)

            os.remove(trace_path)
        else:
            print("解析失败")
            os._exit(1)

        if os.path.exists(trace_path + ".zip"):
            upload_trace_file(trace_path + ".zip")
            tidevice_wrap.rmtree(device ,bundleid, f"/Documents/MemTrace/{folder}")
        else:
            print("未找到trace文件")

def main():

    config = configparser.ConfigParser()


    parser = argparse.ArgumentParser(description="Dump windows trace files")
    parser.add_argument('--bundleid', help='app bundle id')
    parser.add_argument('--device_udid', help='device id')
    parser.add_argument('--mach_obj_path', help='mach-o file for symbol generation')
    parser.add_argument('-n', help='tag name')
    parser.add_argument('-u', help='upload path')
    parser.add_argument('-w', help='workspace')
    parser.add_argument('-s', help='force generate symbols')

    args = parser.parse_args()

    computer_name = os.uname().nodename

    global workspace
    if args.w:
        workspace = os.path.abspath(args.w)

    bundleid = args.bundleid
    tag_name = args.n or ""
    exec_path = None
    force_generate_symbols = args.s
    device_udid = args.device_udid
    mach_obj_path = args.mach_obj_path

    if (config.read(os.path.join(workspace,'config.ini')) and config.has_section("dump")):
        global info
        info(f"config is found")
        if config.has_option("dump",'bundleid'):
            bundleid=config.get("dump",'bundleid')
        if config.has_option("dump","device_udid"):
            device_udid=config.get("dump",'device_udid')
        if config.has_option("dump",'mach_obj_path'):
            mach_obj_path=config.get("dump",'mach_obj_path')
        if config.has_option("dump",'tag_name'):
            tag_name=config.get("dump",'tag_name')
        if config.has_option("dump",'force_generate_symbols'):
            force_generate_symbols=config.get("dump",'force_generate_symbols')
        if config .has_option("dump",'launch_app'):
            launch_app=config.get("dump",'launch_app')
        
        if config.has_option("dump",'upload_api'):
            global upload_api
            upload_api=config.get("dump",'upload_api')
        pass
    else:
        warn(f"config not found, use commandline args")



    device_list = tidevice_wrap.list_devices()
    if len(device_list) == 0:
        error("no device connected")
        os._exit(1)
    
    if device_udid == None:
        print("\nconnected devices:")
        idx = 0
        for info in device_list:
            print(f"    {idx}: {info[0]} IOS{info[1]} {info[2]}")
            idx += 1
        idx = int(input("\nplease select device:"))
        if idx < 0 or idx >= len(device_list):
            error("invalid device index")
            os._exit(1)
        device_udid = device_list[idx][2]

    device = tidevice_wrap.get_device(device_udid)
    device_type, os_ver = tidevice_wrap.get_device_type_and_os_version(device)
    print(f"working in {workspace}, computer name: {computer_name}, device: {device_type}, OS: {os_ver}")


    if args.u:
        upload_api = args.u

    if (bundleid == None):
        error("bundleid not found")
        os._exit(1)

    if not tidevice_wrap.is_app_installed(device, bundleid):
        error(f"app {bundleid} not installed")
        os._exit(1)


    if launch_app:
        print(f"platform : {platform.system()}")
        if platform.system() == 'Darwin':
            envs = '{"UE4_FORCE_MALLOC_ANSI":"1"}'
            cmd = f"xcrun devicectl device process launch --device {device_udid} -e '{envs}' --console --terminate-existing -q {bundleid}"
            print(cmd)
            os.system(cmd)
        else:
            error("launch app only supported on macOS")
            os._exit(1)


    info(f"upload path: {upload_api}")



    symbols_path = os.path.join(workspace, "symbols")
    if not os.path.exists(symbols_path) or force_generate_symbols:
        if mach_obj_path == None:
            error("mach-o path not found")
            os._exit(1)
        print ("generating symbols ...")
        os.system(f'nm -an --demangle {mach_obj_path} | grep " [T,t] " | sed -E "s/(.+) [T,t] (.+)/0x\\1 \\2/" > {symbols_path}')
    else:
        notice("symbols already exists, skip generating")

    if not os.path.exists(symbols_path):
        error("failed to generate symbols")
        os._exit(1)

    info(f"symbols path: {symbols_path}")

    output_dir = os.path.join(workspace, "output")
    if not os.path.exists(output_dir):
        os.makedirs(output_dir)

    print("dumping ...")
     
    dump_tracefiles(device, bundleid,workspace, output_dir,symbols_path,tag_name)


    print("done")
    pass

if __name__ == '__main__':
    main()