# 配置文件

## 配置文件位置

ASD 使用 JSON 格式的配置文件 `.asd_config.json` 来管理多个预设配置。配置文件的位置取决于环境变量的设置：

1. 如果定义了环境变量 `ASD_CONFIG_PATH`，则优先使用 `$ASD_CONFIG_PATH/.asd_config.json`
2. 否则使用**当前工作目录**下的 `.asd_config.json`

## 配置文件格式

配置文件必须是 JSON **数组**，每个元素是一个预设（profile）。

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

## 字段说明

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

## 字段详细说明

### `name` - 配置名称

每个配置都必须有一个唯一的名称，用于在命令行中通过 `-p <name>` 参数引用该配置。

### `mirrorUrl` - 镜像源 URL

指定镜像源 URL，用于加速下载。该 URL 会作为前缀添加到下载链接前。

### `forceOverwrite` - 强制覆盖

设置为 `true` 时，将强制覆盖已存在的文件。默认值为 `false`。

### `repo` - 仓库名称

指定要下载的 GitHub 仓库，格式为 `owner/repo`。

### `chooseTag` - 指定标签

指定要下载的标签。此字段与 `latest` 互斥，不能同时设置。

### `path` - 下载路径

指定文件下载到的目录路径。

### `latest` - 最新版本

设置为 `true` 时，将自动下载最新的 Release 版本。

### `action` - 下载后执行的命令

指定在所有文件下载成功后执行的命令。在命令中可以使用 `$FileName` 占位符来引用当前下载的文件名。

## 配置优先级

当使用 `-p <name>` 参数加载预设时：

- 如果命令行参数中有设定，不会被预设中的参数覆盖
- 预设中设置的参数只在命令行没有指定相同参数时生效