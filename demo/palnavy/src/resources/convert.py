import os

REQUIRED_FILES = [
    "RNG.MKF", "SSS.MKF", "WORD.DAT", "M.MSG",
    "WOR16.FON", "WOR16.ASC", "MAP.MKF", "DATA.MKF",
    "ABC.MKF", "MGO.MKF", "FBP.MKF", "FIRE.MKF",
    "BALL.MKF", "GOP.MKF", "PAT.MKF", "F.MKF", "RGM.MKF"
]

def sanitize_name(filename):
    return filename.replace('.', '_').upper()

def generate_c_resources():
    current_dir_files = {f.upper(): f for f in os.listdir('.')}
    
    c_content = [
        '#include "game_resources.h"\n\n',
    ]
    
    h_content = [
        '#ifndef GAME_RESOURCES_H\n',
        '#define GAME_RESOURCES_H\n\n',
        '#include <stdint.h>\n',
        '#include <stddef.h>\n\n',
    ]

    missing_files = []

    for req_file in REQUIRED_FILES:
        if req_file.upper() not in current_dir_files:
            missing_files.append(req_file)
            continue
            
        actual_filename = current_dir_files[req_file.upper()]
        var_suffix = sanitize_name(req_file)
        
        print(f"actual_filename: {actual_filename} -> var_suffix: {var_suffix}...")
        
        with open(actual_filename, 'rb') as f:
            data = f.read()
            
        file_len = len(data)
        
        h_content.append(f"extern const uint8_t g_res_{var_suffix}[];\n")
        h_content.append(f"extern const size_t g_res_{var_suffix}_len;\n\n")
        
        c_content.append(f"/* actual_filename: {actual_filename} */\n")
        c_content.append(f"const size_t g_res_{var_suffix}_len = {file_len};\n")
        c_content.append(f"const uint8_t g_res_{var_suffix}[] = {{\n")
        
        bytes_str_list = []
        for i in range(0, file_len, 16):
            chunk = data[i:i+16]
            line = ", ".join(f"0x{b:02X}" for b in chunk)
            bytes_str_list.append("    " + line)
            
        c_content.append(",\n".join(bytes_str_list))
        c_content.append("\n};\n\n")

    h_content.append("#endif /* GAME_RESOURCES_H */\n")

    if missing_files:
        print("\nerror：missing files:")
        for mf in missing_files:
            print(f"  - {mf}")
        return

    # 写入物理文件
    with open("game_resources.c", "w", encoding="utf-8") as f_c:
        f_c.writelines(c_content)
        
    with open("game_resources.h", "w", encoding="utf-8") as f_h:
        f_h.writelines(h_content)

    print("\ndone")

if __name__ == "__main__":
    generate_c_resources()
