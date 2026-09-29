import os

def rename_all_to_lower():
    cwd = os.getcwd()
    for name in os.listdir(cwd):
        old_path = os.path.join(cwd, name)

        if os.path.isdir(old_path):
            continue
        lower_name = name.lower()
        if lower_name == name:
            continue
        new_path = os.path.join(cwd, lower_name)

        os.rename(old_path, new_path)
        print(f"{name} -> {lower_name}")

if __name__ == "__main__":
    rename_all_to_lower()