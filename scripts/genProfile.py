import json
import os
import sys
from pathlib import Path


def clear_screen():
    os.system('cls' if os.name == 'nt' else 'clear')


def print_header():
    print("=" * 50)
    print("           ASD 配置文件生成工具")
    print("=" * 50)
    print()


def get_profile_data():
    print_header()
    print("请输入配置信息:")
    profile = {}

    while True:
        name = input("名称 (必填): ").strip()
        if name:
            profile["name"] = name
            break
        else:
            print("错误: 名称是必填项，请重新输入！")
            print()

    print()
    mirror_url = input("镜像URL (可选，直接回车跳过): ").strip()
    if mirror_url:
        profile["mirrorUrl"] = mirror_url

    print()
    while True:
        force_overwrite = input("是否强制覆盖? (y/n, 默认: n): ").strip().lower()
        if force_overwrite in ['y', 'yes', '1', 'true', '是', '']:
            profile["forceOverwrite"] = True
            break
        elif force_overwrite in ['n', 'no', '0', 'false', '否']:
            profile["forceOverwrite"] = False
            break
        else:
            print("请输入 'y' 或 'n'")

    print()
    repo = input("仓库 (默认: nostalgia296/asd): ").strip()
    if repo:
        profile["repo"] = repo
    else:
        profile["repo"] = "nostalgia296/asd"

    print()
    choose_tag = input("选择标签 (可选，直接回车跳过): ").strip()
    if choose_tag:
        profile["chooseTag"] = choose_tag

    print()
    path = input("路径 (可选，直接回车跳过): ").strip()
    if path:
        profile["path"] = path

    print()
    while True:
        latest = input("使用最新版本? (y/n, 默认: n): ").strip().lower()
        if latest in ['y', 'yes', '1', 'true', '是']:
            profile["latest"] = True
            break
        elif latest in ['', 'n', 'no', '0', 'false', '否']:
            profile["latest"] = False
            break
        else:
            print("请输入 'y' 或 'n'")

    return profile


def get_config_path():
    config_path = os.environ.get('ASD_CONFIG_PATH')

    if config_path:
        config_file_path = os.path.join(config_path, ".asd_config.json")
        if os.path.exists(config_file_path):
            return config_file_path
        else:
            return config_file_path
    else:
        current_dir = os.getcwd()
        return os.path.join(current_dir, ".asd_config.json")


def add_profile_to_asd_config():
    config_path = get_config_path()

    profiles = []
    if os.path.exists(config_path):
        try:
            with open(config_path, 'r', encoding='utf-8') as f:
                content = f.read().strip()
                if content:
                    profiles = json.loads(content)
                    if not isinstance(profiles, list):
                        print(f"警告: {config_path} 不包含列表，初始化为空列表")
                        profiles = []
        except Exception as e:
            print(f"读取 {config_path} 时出错: {e}")
            return
    else:
        print(f"配置文件 {config_path} 不存在，将创建该文件")

    print(f"\n正在向 {config_path} 添加配置")
    profile = get_profile_data()
    if not profile:
        print("获取配置数据失败。")
        return

    existing_names = [p.get('name') for p in profiles if 'name' in p]
    if profile['name'] in existing_names:
        overwrite = input(f"名称为 '{profile['name']}' 的配置已存在。是否覆盖? (y/n): ").strip().lower()
        if overwrite != 'y':
            print("未添加配置。")
            return
        profiles = [p for p in profiles if p.get('name') != profile['name']]

    profiles.append(profile)

    try:
        os.makedirs(os.path.dirname(config_path), exist_ok=True)

        with open(config_path, 'w', encoding='utf-8') as f:
            json.dump(profiles, f, indent=2, ensure_ascii=False)
        print(f"\n成功向 {config_path} 添加配置！现在共有 {len(profiles)} 个配置。")
        print(f"配置文件位置: {config_path}")
        
        print("\n新添加的配置信息:")
        for key, value in profile.items():
            print(f"  {key}: {value}")
    except Exception as e:
        print(f"写入 {config_path} 时出错: {e}")


def main():
    if len(sys.argv) != 2:
        print(__doc__)
        sys.exit(1)

    command = sys.argv[1].lower()

    if command == "add":
        add_profile_to_asd_config()
    else:
        print("无效命令。请使用 'add'。")
        print(__doc__)
        sys.exit(1)


if __name__ == "__main__":
    main()