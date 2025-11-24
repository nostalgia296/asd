# Profile 管理

ASD 提供了便捷的配置文件管理功能，可以通过 `asd profile` 命令来创建、查看和删除配置预设。

## 命令语法

```bash
asd profile <command> [arguments]
```

## 可用命令

### `list` / `ls` - 列出所有配置

显示所有已保存的配置预设：

```bash
asd profile list
# 或
asd profile ls
```

输出示例：
```
可用的 Profiles:
  - default
    Repo: nostalgia296/asd
    Mode: Latest Release

  - mirror
    Repo: nostalgia296/asd
    Mode: Latest Release
    
  - specific-tag
    Repo: flutter/flutter
    Tag: 3.3.0
```

### `add` - 添加新配置

交互式地创建一个新的配置预设：

```bash
asd profile add
```

执行该命令后，系统会按顺序询问以下信息：

1. **Profile 名称** - 配置的唯一标识
2. **仓库名称** - 格式为 `owner/repo`，默认为 `nostalgia296/asd`
3. **是否总是使用最新 Release** - 输入 `y` 或 `n`
4. **指定 Tag** - 如果不是使用最新版本，可以指定特定的标签
5. **下载路径** - 文件下载到的目录，留空则使用默认路径
6. **镜像源 URL** - 加速下载的镜像源，留空则不使用
7. **是否强制覆盖文件** - 输入 `y` 或 `n`
8. **下载后执行的命令** - 下载完成后执行的命令，留空则不执行

示例交互：
```
--- 创建新 Profile ---
Profile 名称 (唯一标识): my-config
仓库 (owner/repo): [nostalgia296/asd] flutter/flutter
是否总是使用最新 Release (y/N): y
下载路径 (如果不需要请留空): 
镜像源 URL (如果不需要请留空): https://gh-proxy.com/
是否强制覆盖文件 (y/N): n
下载后执行的命令 (如果不需要请留空): 
成功创建/更新 Profile: my-config
```

### `remove` / `rm` - 删除配置

删除一个指定名称的配置预设：

```bash
asd profile remove <name>
# 或
asd profile rm <name>
```

例如：
```bash
asd profile remove my-config
```

如果配置不存在，会显示错误信息：
```
错误: 找不到名为 "my-config" 的 Profile。
```

## 最佳实践

### 组织不同使用场景的配置

可以为不同的使用场景创建不同的配置预设：

- **开发环境**: 为不同项目创建快速下载配置
- **生产环境**: 使用镜像源加速下载稳定版本
- **测试环境**: 自动覆盖文件配置

### 配置复用

配置预设可以让你在不同环境下快速切换下载参数，而无需每次都输入完整的命令行参数。

例如，你可能有如下配置：
```json
[
  {
    "name": "flutter-latest",
    "repo": "flutter/flutter",
    "latest": true,
    "mirrorUrl": "https://gh-proxy.com/"
  },
  {
    "name": "flutter-specific",
    "repo": "flutter/flutter",
    "chooseTag": "3.3.0",
    "path": "./downloads/flutter"
  }
]
```

然后可以使用这些配置：
```bash
# 使用镜像源下载最新的 Flutter
./asd -p flutter-latest

# 下载特定版本的 Flutter 到指定目录
./asd -p flutter-specific
```