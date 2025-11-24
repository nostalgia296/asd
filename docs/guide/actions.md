# 动作执行

ASD 支持在文件下载成功后执行自定义命令。这个功能通过配置文件中的 `action` 字段实现，可以在下载完成后自动执行后续操作。

## 基本概念

动作执行功能允许你在所有文件成功下载后运行一个自定义命令。这个命令只会在下载完全成功时执行，如果下载过程中有任何失败，命令将不会执行。

## 使用场景

动作执行功能在以下场景中非常有用：

- **自动解压**: 下载压缩包后自动解压
- **自动安装**: 下载安装包后自动安装
- **文件校验**: 对下载的文件进行校验
- **脚本执行**: 执行后续处理脚本

## 配置方法

### 在配置文件中设置

在配置文件的 profile 中添加 `action` 字段：

```json
{
  "name": "auto-extract",
  "repo": "owner/repo",
  "latest": true,
  "action": "tar -xzf $FileName"
}
```

### 通过命令行参数设置

在命令行中使用配置文件中的预设，或通过配置文件中的 action 字段来实现动态操作。

## `$FileName` 占位符

在动作命令中，你可以使用 `$FileName` 占位符来引用当前下载的文件名。这个占位符会在执行命令时被实际的文件名替换。

### 单文件处理

如果下载单个文件，`$FileName` 将被替换为该文件的名称。

### 多文件处理

当下载多个文件时，每个文件下载完成后都会执行配置中的动作，`$FileName` 会被替换为当前处理的文件名。

示例：
```json
{
  "name": "extract-each",
  "repo": "owner/repo",
  "latest": true,
  "action": "echo 'Downloaded file: $FileName' && tar -xzf $FileName"
}
```

## 执行环境

动作命令会在下载目录（由 `path` 字段指定，如果没有指定则为当前目录）中执行。

## 实际应用示例

### 示例 1: 自动解压下载的压缩包

```json
[
  {
    "name": "auto-unzip",
    "repo": "user/project",
    "latest": true,
    "path": "./downloads",
    "action": "unzip $FileName -d ./extracted"
  }
]
```

### 示例 2: 验证下载文件的校验和

```json
[
  {
    "name": "verify-download",
    "repo": "user/project",
    "latest": true,
    "path": "./downloads",
    "action": "sha256sum $FileName && echo 'File integrity verified'"
  }
]
```

### 示例 3: 安装下载的软件包

```json
[
  {
    "name": "auto-install",
    "repo": "user/app",
    "latest": true,
    "path": "./packages",
    "action": "sudo dpkg -i $FileName && sudo apt-get install -f"
  }
]
```

## 重要注意事项

1. **仅在成功时执行**: 动作命令只在所有文件都下载成功时执行
2. **执行路径**: 命令在下载目录中执行，而不是当前工作目录
3. **安全性**: 请确保动作命令是安全的，因为它会在下载后自动执行
4. **错误处理**: 如果动作命令执行失败，工具会报告错误但不会影响已下载的文件
5. **占位符使用**: 如果动作命令不包含 `$FileName`，则只执行一次，而不是为每个文件执行
6. **命令格式**: 确保命令格式正确，特别是特殊字符需要适当转义