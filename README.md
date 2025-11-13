# GitHub Release 下载工具

一个用 Dart 编写的命令行工具，用于从 GitHub Releases 下载文件，支持配置预设，并发下载、进度显示和gh-proxy加速源。

#### 参数选项

| 参数 | 说明 | 示例 |
|------|------|------|
| `-u <URL>` | 指定 GitHub 仓库 URL | `-u https://github.com/owner/repo` |
| `-m <URL>` | 使用镜像源 | `-m https://mirror.example.com/` |
| `-f` | 强制覆盖已存在的文件 | `-f` |
| `-t` | 快速指定tagname | `-t tagname` |
| `-c` | 自定义下载路径 | `-c path` |
| `-l` | 快速切换到latest Release | `-l` |
| `-p` | 指定预设名称 | `-p name` |

#### 注意
1. `-l` 与 `-t` 参数冲突不可同时使用
2. `-u` 支持的链接写法：
   - https://github.com/owner/repo.git
   - https://github.com/owner/repo
   - git@github.com:owner/repo.git
   - github.com/owner/repo
   - owner/repo
   
#### 配置文件
###### 快速生成和管理配置文件
通过 asd profile 相关参数可以管理配置

```bash
asd profile add #新增
asd profile remove/rm name #删除
asd profile list/ls #列出已有配置
```

###### 文件位置
1. 如果定义了环境变量 `ASD_CONFIG_PATH`，则优先使用  
   `$ASD_CONFIG_PATH/.asd_config.json`
2. 否则使用**当前工作目录**下的  
   `.asd_config.json`

###### 文件格式
JSON **数组**，每个元素是一个预设（profile）。  
示例：

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
  },
  {
    "name": "overwrite",
    "mirrorUrl": null,
    "forceOverwrite": true,
    "repo": "nostalgia296/asd",
    "chooseTag": null,
    "path": null,
    "latest": false
  },
  {
    "name": "mirror",
    "mirrorUrl": "https://gh-proxy.com/",
    "forceOverwrite": false,
    "repo": "nostalgia296/asd",
    "chooseTag": null,
    "path": "./downloads",
    "latest": true
  }
]
```

###### 字段说明
| 字段 | 类型 | 说明 |
|---|---|---|
| `name` | string | **必填**，profile 的唯一标识 |
| `mirrorUrl` | string? | 镜像加速前缀，如 `https://gh-proxy.com/(可以不写'/')` |
| `forceOverwrite` | bool | 是否强制覆盖已存在文件，默认 `false` |
| `repo` | string | 仓库名称，写法可以跟-u一样，默认 `nostalgia296/asd` |
| `chooseTag` | string? | 指定 tag，与 `latest` 互斥 |
| `path` | string? | 下载到哪个目录（目前不支持支持 `~`） |
| `latest` | bool | 是否总是取最新 Release，默认 `false` |
| `action` | string? | 下载成功后执行的命令（仅在所有文件下载成功时执行） |

###### 动作执行 (Action)
配置文件支持 `action` 字段，可以在下载成功后执行指定的命令。该命令只会在所有文件都成功下载后执行,另外，如果你的action需要对当前你下载的文件进行操作时，你可以用$FileName来代指当前文件名字。
注意：action中命令执行的路径就是你的文件下载的文件那个路径(path字段)，而不是当前路径(当然如果没指定path也是当前路径)

---

###### 使用方式
如果预设有指定，不会覆盖参数指定的

命令行通过 `-p <name>`加载预设：

```bash
# 使用名为 fast 的预设
asd -p fast
```

#### 编译可执行文件
1. 确保安装了Dart SDK 和 make 
2. 然后执行make
```bash
git clone https://github.com/nostalgia296/asd.git
cd asd
make
```
或者直接编译:
```bash
git clone https://github.com/nostalgia296/asd.git
cd asd
dart pub get && dart compile exe bin/downloader.dart -o asd
```
