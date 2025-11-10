# GitHub Release 下载工具

一个用 Dart 编写的命令行工具，用于从 GitHub Releases 下载文件，支持并发下载、进度显示和gh-proxy加速源。

#### 参数选项

| 参数 | 说明 | 示例 |
|------|------|------|
| `-u <URL>` | 指定 GitHub 仓库 URL | `-u https://github.com/owner/repo` |
| `-m <URL>` | 使用镜像源 | `-m https://mirror.example.com/` |
| `-f` | 强制覆盖已存在的文件 | `-f` |
| `-t` | 快速指定tagname | `-t tagname` |
| `-c` | 自定义下载路径 | `-c path` |
| `-l` | 快速切换到latest Release | `-l` |
#### 注意：-l 与 -t 是冲突的
#### -u支持的链接写法
  1. https://github.com/owner/repo.git
  2. https://github.com/owner/repo
  3. git@github.com:owner/repo.git
  4. github.com/owner/repo
  5. owner/repo
