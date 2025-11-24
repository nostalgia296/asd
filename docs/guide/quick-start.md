# 快速开始

## 安装

### 从源码编译

1. 确保已安装 Dart SDK
2. 克隆仓库并编译

```bash
git clone https://github.com/nostalgia296/asd.git
cd asd
make
```

或者直接编译：

```bash
git clone https://github.com/nostalgia296/asd.git
cd asd
dart pub get && dart compile exe bin/downloader.dart -o asd
```

## 基本使用

### 下载最新版本

```bash
./asd -u https://github.com/owner/repo -l
```

### 下载指定标签

```bash
./asd -u https://github.com/owner/repo -t v1.0.0
```

### 使用镜像源加速

```bash
./asd -u https://github.com/owner/repo -l -m https://gh-proxy.com/
```

### 交互式选择

直接运行命令不带标签参数，可以交互式选择标签和文件：

```bash
./asd -u https://github.com/owner/repo
```

## 配置预设

创建一个配置文件 `.asd_config.json` 来管理多个预设：

```json
[
  {
    "name": "default",
    "mirrorUrl": null,
    "forceOverwrite": false,
    "repo": "nostalgia296/asd",
    "chooseTag": null,
    "path": null,
    "latest": false
  },
  {
    "name": "latest",
    "mirrorUrl": null,
    "forceOverwrite": false,
    "repo": "nostalgia296/asd",
    "chooseTag": null,
    "path": null,
    "latest": true
  }
]
```

使用预设：

```bash
./asd -p latest
```

## 管理配置预设

创建新的配置预设：

```bash
./asd profile add
```

列出所有配置预设：

```bash
./asd profile list
```

删除指定配置预设：

```bash
./asd profile remove <name>
```

现在你已经了解了基本的使用方法！继续阅读其他文档以深入了解高级功能。