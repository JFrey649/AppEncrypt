# AppEncrypt

Windows 第三方 exe 加密工具：**右键加密、双击需密码启动、卸载自动还原**。

技术栈：C++17 · Qt6 Widgets · CMake · SQLite · Windows CNG (BCrypt) · Inno Setup

---

## 功能概览

| 能力 | 说明 |
|------|------|
| 加密 | 将任意 exe 替换为 Stub，原程序加密为同目录 `.appencrypt` 文件 |
| 启动 | 双击加密 exe → 输入密码 → 解密到临时文件并启动（保留同目录 DLL） |
| 解密 / 改密 | 右键或主界面操作，恢复原始 exe 或更换密码 |
| 右键菜单 | 注册到 `exefile` Shell，支持 HKCU / HKLM |
| 卸载还原 | 卸载时自动解密全部已加密程序，移除右键菜单 |

---

## 架构简述

```
用户双击 MyApp.exe (Stub)
    → AppEncryptStub 调用 AppEncrypt.exe unlock
        → 验证密码 → 解密 vault
        → 同目录写入 {uuid}.appencrypt.run.exe
        → CreateProcess 启动宿主 → 退出后删除临时文件
```

加密后文件：

```
MyApp.exe                 ← Stub（尾部含 UUID、密文路径、launcher 路径）
MyApp.exe.appencrypt      ← AES-256-GCM 密文
appencrypt.db             ← 元数据（salt、verifier、wrapped DEK 等）
```

详细设计见 **[项目实现说明](docs/项目实现说明.md)**。

---

## 依赖

- Windows 10 1903+ / Windows 11（x64）
- CMake 3.16+
- Qt 6（Core, Widgets，MSVC 2022 64-bit）
- Visual Studio 2022 或 MinGW
- Inno Setup 6（打包时）
- curl / tar（`fetch_sqlite.bat`，Windows 10+ 自带）

---

## 快速构建

```bat
REM 1. 拉取 SQLite（首次）
scripts\fetch_sqlite.bat

REM 2. 配置 + 编译
cmake -B build -G "Visual Studio 17 2022" -A x64 -DCMAKE_PREFIX_PATH="D:/Qt/6.10.2/msvc2022_64"
cmake --build build --config Release

REM 3. 打包安装程序（可选）
scripts\build_installer.bat "D:\Qt\6.10.2\msvc2022_64"
REM 输出: dist\AppEncrypt-Setup-1.0.0.exe
```

> `build\Release` 下直接运行可能缺 Qt DLL，请用 `windeployqt` 或安装包测试。

---

## 使用

### 安装后

安装程序自动注册 exe 右键菜单。在任意 `.exe` 上右键：

- 使用 AppEncrypt 加密
- 使用 AppEncrypt 解密
- 更改 AppEncrypt 密码

### 命令行

```text
AppEncrypt.exe                          打开主界面
AppEncrypt.exe encrypt   "<exe_path>"
AppEncrypt.exe decrypt   "<exe_path>"
AppEncrypt.exe changepw  "<exe_path>"
AppEncrypt.exe unlock    "<stub_path>" [host_args...]
AppEncrypt.exe register-shell   [--system]
AppEncrypt.exe unregister-shell [--system]
AppEncrypt.exe uninstall-restore
```

### 辅助脚本

```bat
scripts\register_shell.bat "C:\Program Files\AppEncrypt"
scripts\unregister_shell.bat "C:\Program Files\AppEncrypt"
scripts\install_sendto.bat "C:\Program Files\AppEncrypt"
```

---

## 工程结构

```
src/
  main.cpp                 入口 / CLI 分发
  cli/                     命令行解析
  core/                    AppConfig, Crypto, Database, EncryptService, ...
  ui/                      MainWindow, PasswordDialog, ProgressDialog
stub/stub_main.cpp         轻量 Stub（无 Qt）
installer/AppEncrypt.iss   Inno Setup
scripts/*.bat              构建与部署脚本
third_party/sqlite/        SQLite amalgamation
```

---

## 文档

| 文档 | 内容 |
|------|------|
| **[项目实现说明](docs/项目实现说明.md)** | 架构、模块、数据格式、流程（**开发者必读**） |
| [构建与部署手册](docs/构建与部署手册.md) | 编译、打包、安装、卸载、排错 |
| [需求说明书](docs/需求说明书.md) | 功能与非功能需求 |
| [技术确认纪要](docs/技术确认纪要.md) | 技术决策与 MVP 范围 |

---

## 配置

首次运行生成 `{InstallDir}\config.ini`，并同步 `MainExe` 到 `%ProgramData%\AppEncrypt\config.ini`（供 Stub 备用）。

数据库默认：`{InstallDir}\appencrypt.db`
