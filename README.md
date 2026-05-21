# 设备工装管理系统

一个基于 Qt 5 Widgets + SQLite 的本地桌面管理系统，适合在内网环境下管理设备、借还、报修和电缆台账。

## 功能

- 设备台账：查询、新增、编辑、删除
- 借入借出：登记借出、归还、按状态和关键字查询
- 报修管理：登记报修、更新处理状态、查看处理结果
- 电缆管理：
  - 电缆台账字段为 `编号`、`始端`、`终端`
  - 支持几千根电缆的查询、筛选、借出、归还
  - 支持 Excel / CSV 批量导入
  - 支持电缆标签打印
  - 支持扫码借出、扫码归还

## 电缆管理说明

### 主界面搜索

电缆管理页的搜索框支持两个勾选项：

- `回车后清空`：搜索回车后自动清空输入框，减少反复删除文字的操作
- `回车加入缓存`：输入电缆编号后回车，按编号精确匹配并加入右侧借出/归还缓存栏

不勾选 `回车加入缓存` 时，回车仍执行普通模糊搜索，可按 `编号`、`始端`、`终端` 查询。

### 缓存栏登记

先把要操作的电缆加入右侧缓存栏，再填写借用人、部门、日期、备注等信息，最后执行：

- `批量借出`
- `批量归还`

缓存栏支持移除选中项和清空缓存。

### 扫码

扫码枪建议使用键盘输入模式，并在扫描后自动回车。

- `扫码借出`：扫描后确认加入借出缓存
- `扫码归还`：扫描后确认加入归还缓存

当前扫码流程不依赖主搜索框。

## 导入说明

支持以下格式：

- `.csv`
- `.xlsx`
- `.xls`

导入表头必须包含：

- `编号`
- `始端`
- `终端`

说明：

- 空编号行会被跳过
- 编号重复时会更新始端、终端，不重复插入
- `.xlsx` 读取第一张工作表
- `.xls` 读取 Excel 97-2003 格式；大批量导入时建议优先使用 `.xlsx` 或 `.csv`

## 离线部署包

仓库已提供 Win7 x64 离线包：

- `offline-package/CableManagementSystem-Win7-x64.zip`

离线包内包含：

- 程序主文件
- Qt 运行库
- SQLite 驱动
- 打印相关组件
- VC++ 运行库安装包
- Win7 部署说明

## 运行

直接双击离线包目录中的 `启动系统.bat`，或运行 `DapmQt.exe`。

首次启动会自动创建本地数据库文件：

- `data/equipment.db`

## Win7 部署建议

- Windows 7 x64 环境
- 建议使用离线包中的启动脚本
- 如系统缺少 VC++ 运行库，可先安装 `_redist` 目录中的安装包
- 若要打印标签，请确认已安装 Zebra ZT210 对应打印机驱动

## 构建

当前项目使用 Qt 5.15.2 + MSVC 2019 x64。

参考构建方式：

```bat
call "C:\Program Files\Microsoft Visual Studio\2022\Professional\VC\Auxiliary\Build\vcvars64.bat"
cmake -S . -B build-cable-msvc -G "NMake Makefiles" -DCMAKE_BUILD_TYPE=Debug -DCMAKE_PREFIX_PATH="C:\Software\qt5msvc32\5.15.2\msvc2019_64"
cmake --build build-cable-msvc --target DapmQt CableModuleTests
ctest --test-dir build-cable-msvc --output-on-failure -R CableModuleTests
```

## 备注

- 程序使用 SQLite，本地运行不依赖网络
- 电缆编号是唯一标识
- 打印、导入和扫码都围绕电缆编号展开
