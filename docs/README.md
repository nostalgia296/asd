# ASD 文档

这是 ASD (GitHub Release 下载工具) 的官方文档网站。

## 开发

### 安装依赖

```bash
npm install
```

### 本地开发

```bash
npm run docs:dev
```

开发服务器将在 http://localhost:5173 上运行。

### 构建文档

```bash
npm run docs:build
```

### 本地预览构建结果

```bash
npm run docs:serve
```

## 文档结构

- `index.md` - 首页
- `/guide/` - 用户指南
  - `quick-start.md` - 快速开始
  - `arguments.md` - 命令行参数
  - `configuration.md` - 配置文件
  - `profiles.md` - Profile 管理
  - `mirrors.md` - 镜像加速
  - `actions.md` - 动作执行

## 贡献

修改文档后，请确保本地预览无误后再提交。