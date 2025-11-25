import { defineConfig } from 'vitepress'

export default defineConfig({
  title: 'ASD - GitHub Release 下载工具',
  description: '一个用 Dart 编写的命令行工具，用于从 GitHub Releases 下载文件',
  base: '/asd/',
  ignoreDeadLinks: true,
  themeConfig: {
    nav: [
      { text: '首页', link: '/' },
      { text: '快速开始', link: '/guide/quick-start' },
      { text: '配置', link: '/guide/configuration' },
      { text: '命令行参数', link: '/guide/arguments' },
      { text: '高级功能', link: '/guide/advanced' }
    ],

    sidebar: [
      {
        text: '指南',
        items: [
          { text: '介绍', link: '/' },
          { text: '快速开始', link: '/guide/quick-start' },
          { text: '命令行参数', link: '/guide/arguments' },
          { text: '配置文件', link: '/guide/configuration' },
          { text: 'Profile 管理', link: '/guide/profiles' },
          { text: '镜像加速', link: '/guide/mirrors' },
          { text: '动作执行', link: '/guide/actions' }
        ]
      }
    ],

    socialLinks: [
      { icon: 'github', link: 'https://github.com/nostalgia296/asd' }
    ],

    footer: {
      message: 'Released under the MIT License.',
      copyright: 'Copyright © 2025-present nostalgia296'
    }
  }
})
