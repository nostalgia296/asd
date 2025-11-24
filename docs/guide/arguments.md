# 命令行参数

ASD 提供了多种命令行参数来控制下载行为。所有参数都以 `-` 开头。

## 参数说明

| 参数 | 说明 | 示例 |
|------|------|------|
| `-u <URL>` | 指定 GitHub 仓库 URL | `-u https://github.com/owner/repo` |
| `-m <URL>` | 使用镜像源 | `-m https://mirror.example.com/` |
| `-f` | 强制覆盖已存在的文件 | `-f` |
| `-t <tagname>` | 快速指定标签名 | `-t v1.0.0` |
| `-c <path>` | 自定义下载路径 | `-c ./downloads` |
| `-l` | 快速切换到最新 Release | `-l` |
| `-p <name>` | 指定预设名称 | `-p name` |

## 详细说明

### `-u <URL>` - 指定仓库 URL

使用 `-u` 参数指定要下载 Release 的 GitHub 仓库。支持多种 URL 格式：

- `https://github.com/owner/repo.git`
- `https://github.com/owner/repo`
- `git@github.com:owner/repo.git`
- `github.com/owner/repo`
- `owner/repo`

示例：
```bash
./asd -u https://github.com/flutter/flutter
```

### `-m <URL>` - 镜像源

使用 `-m` 参数指定镜像源以加速下载。镜像源会作为前缀添加到下载链接前。

示例：
```bash
./asd -u https://github.com/flutter/flutter -l -m https://gh-proxy.com/
```

### `-f` - 强制覆盖

使用 `-f` 参数强制覆盖已存在的文件。默认情况下，工具会跳过已存在的文件。

示例：
```bash
./asd -u https://github.com/flutter/flutter -l -f
```

### `-t <tagname>` - 指定标签

使用 `-t` 参数直接指定要下载的标签名，无需交互式选择。

示例：
```bash
./asd -u https://github.com/flutter/flutter -t 3.3.0
```

### `-c <path>` - 自定义下载路径

使用 `-c` 参数指定文件下载到的目录路径。

示例：
```bash
./asd -u https://github.com/flutter/flutter -l -c ./my_downloads
```

### `-l` - 最新 Release

使用 `-l` 参数自动下载最新的 Release 版本，无需交互式选择。

示例：
```bash
./asd -u https://github.com/flutter/flutter -l
```

### `-p <name>` - 预设配置

使用 `-p` 参数指定一个预设配置，可以从配置文件中加载预设参数。

示例：
```bash
./asd -p my_config
```

## 参数冲突

注意：`-l`（最新 Release）和 `-t`（指定标签）参数不能同时使用，这样做会导致错误。

```bash
# ❌ 错误：这两个参数不能同时使用
./asd -u https://github.com/flutter/flutter -l -t 3.3.0
```