# 镜像加速

ASD 支持使用镜像源来加速 GitHub Release 文件的下载，特别适用于网络访问 GitHub 比较慢的地区。

## 基本概念

镜像加速功能允许你通过 `-m` 参数或配置文件中的 `mirrorUrl` 字段指定一个镜像源地址。下载工具会将这个镜像源地址作为前缀添加到原始的 GitHub 下载链接前，从而通过镜像服务器下载文件。

## 使用方法

### 通过命令行参数

使用 `-m` 参数指定镜像源：

```bash
./asd -u https://github.com/owner/repo -l -m https://gh-proxy.com/
```

### 通过配置文件

在配置文件中设置 `mirrorUrl` 字段：

```json
[
  {
    "name": "mirror-download",
    "repo": "owner/repo",
    "latest": true,
    "mirrorUrl": "https://gh-proxy.com/"
  }
]
```

然后使用该配置：

```bash
./asd -p mirror-download
```

## 支持的镜像源

以下是一些常用的 GitHub 镜像源：

- **gh-proxy.com**: `https://gh-proxy.com/`
- **proxy.pipers.cn**: `https://proxy.pipers.cn/`

你也可以使用其他公开的或自建的 GitHub 镜像服务。

## 镜像源格式

镜像源 URL 应该以 `/` 结尾。如果未提供结尾的 `/`，工具会自动添加。

例如：
- 输入: `https://gh-proxy.com` → 实际使用: `https://gh-proxy.com/`
- 输入: `https://gh-proxy.com/` → 实际使用: `https://gh-proxy.com/`

## 工作原理

当启用镜像加速时，工具会将原始的 GitHub 下载链接转换为镜像源链接：

原始链接: `https://github.com/owner/repo/releases/download/v1.0.0/file.zip`
镜像链接: `https://gh-proxy.com/https://github.com/owner/repo/releases/download/v1.0.0/file.zip`

## 注意事项

1. **镜像源可靠性**: 请确保使用的镜像源是可靠的，不受信任的镜像源可能会影响下载文件的完整性。
2. **协议一致性**: 大多数镜像源支持 HTTPS 协议，请在镜像 URL 中使用 HTTPS。
3. **速度差异**: 不同镜像源在不同地区的访问速度可能有差异，可以尝试多个镜像源找到最适合的。
4. **更新延迟**: 某些镜像源可能存在缓存，新发布的文件可能需要一些时间才能通过镜像源访问。
5. **配置优先级**: 如果同时在命令行和配置文件中指定了镜像源，命令行参数会优先使用。

## 配置示例

### 基础镜像配置

```json
[
  {
    "name": "fast-download",
    "repo": "flutter/flutter",
    "latest": true,
    "mirrorUrl": "https://gh-proxy.com/"
  }
]
```

### 带自定义路径的镜像配置

```json
[
  {
    "name": "complete-config",
    "repo": "flutter/flutter",
    "chooseTag": "3.3.0",
    "mirrorUrl": "https://gh-proxy.com/",
    "path": "./flutter-sdk",
    "forceOverwrite": false
  }
]
```

## 性能优化建议

1. **选择合适的镜像源**: 根据你的地理位置选择访问速度最快的镜像源。
2. **结合并发下载**: 镜像加速与工具的并发下载功能结合使用，可以进一步提升下载效率。
3. **定期检查镜像源**: 定期检查并更新配置文件中的镜像源，以确保其可用性。