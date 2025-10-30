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
        os._exit(1)
        return False

def main():

    config = configparser.ConfigParser()


    parser = argparse.ArgumentParser(description="Dump windows trace files")
    parser.add_argument('-g', help='game path')
    parser.add_argument('-n', help='tag name')
    parser.add_argument('-u', help='upload path')
    parser.add_argument('-w', help='workspace')
    parser.add_argument('-s', help='force generate symbols')

    args = parser.parse_args()

    computer_name = os.environ["COMPUTERNAME"]
    VC, OS = get_platform_info()

    global workspace
    if args.w:
        workspace = os.path.abspath(args.w)


    print(f"working in {workspace}, computer name: {computer_name}, video adapter: {VC}, OS: {OS}")

    game_path = args.g
    tag_name = args.n or ""
    exec_path = None
    force_generate_symbols = args.s
    exec_name_part = None

    if args.u:
        global upload_api
        upload_api = args.u

    if (config.read(os.path.join(workspace,'config.ini')) and config.has_section("dump")):
        if config.has_option("dump",'game_path'):
            game_path=config.get("dump",'game_path')
        if config.has_option("dump",'tag_name'):
            tag_name=config.get("dump",'tag_name')
        if config.has_option("dump",'force_generate_symbols'):
            force_generate_symbols=config.get("dump",'force_generate_symbols') == "True"
        if config.has_option("dump","executale"):
            exec_name_part =config.get("dump","executale")
        
        if config.has_option("dump",'upload_api'):
            upload_api=config.get("dump",'upload_api')
        pass

    
    if exec_name_part == None:
        exec_name_part = "Win64"

    if (game_path == None):
        error("game path not found")
        os._exit(1)


    # find executable path
    for dir in os.listdir(game_path):
        path = os.path.join(game_path, dir)
        if dir == "Engine" or os.path.isfile(path):
            continue
        
        bin_path = os.path.join(path, "Binaries")
        if not os.path.exists(bin_path):
            continue

        game_path = path
        trace_path = os.path.join(game_path, "MemTrace")
        if not os.path.exists(trace_path):
            error ("no trace files")
            continue

        temp = os.path.join(bin_path, "Win64")

        for root, dirs, files in os.walk(temp):
            for file in files:
                if file.endswith(".exe") and file.find(exec_name_part) != -1:
                    exec_path = os.path.join(bin_path, "Win64", file)
                    exec_name = file[:-4]

    if exec_path == None:
        error("exec files path not found")
        os._exit(1)

    info(f"executable path found: {exec_path}")
    info(f"upload path: {upload_api}")

    output_dir = os.path.join(workspace, "output")
    if not os.path.exists(output_dir):
        os.makedirs(output_dir)

    symbols_path = os.path.join(workspace, "symbols")
    if not os.path.exists(symbols_path) or force_generate_symbols:
        if os.path.exists(symbols_path):
            os.remove(symbols_path)
        print ("generating symbols ...")
        os.system(f"{os.path.join(workspace, 'gensym.exe')} {exec_path} {symbols_path}")
    else:
        notice("symbols already exists, skip generating")

    if not os.path.exists(symbols_path):
        error("failed to generate symbols")
        # os._exit(1)

    info(f"symbols path: {symbols_path}")

    print("dumping ...")
    for root, dirs, files in os.walk(trace_path):
        for dir in dirs:
            trace_dir = os.path.join(root, dir)
            
            output_name = f"{dir}_master_0_{VC}_{OS}_{computer_name}{tag_name}.memtrace"
            output_path = os.path.join(output_dir, output_name)
            parsing_cmd = f'{os.path.join(workspace, "MemoryTraceParser")} {trace_dir} {symbols_path} -o={output_path} -e={exec_name}'
            print(parsing_cmd)
            os.system(parsing_cmd)

            
            if os.path.exists(output_path):
                print(f'begin zip {output_path}')
                with zipfile.ZipFile(output_path + '.zip', 'w', zipfile.ZIP_DEFLATED) as zipf:
                    zipf.write(output_path, output_name)

                os.remove(output_path)
            else:
                print("Parsing failed.")
                os._exit(1)

            if os.path.exists(output_path + '.zip'):
                if upload_api : 
                    upload_trace_file(output_path + '.zip')
                shutil.rmtree(trace_dir)

    print("done")
    pass

if __name__ == '__main__':
    main()