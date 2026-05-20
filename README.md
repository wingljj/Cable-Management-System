# 设备工装管理系统

这是一个基于 Qt 5 Widgets + SQLite 的本地桌面管理系统，适合做设备/工装台账、借入借出、报修和统计分析。

## 功能

- 设备台账：查询、新增、编辑、删除设备/工装信息。
- 借入借出：登记借出、登记归还、按状态和关键字查询流转记录。
- 报修管理：登记报修、更新维修状态、记录处理人和处理结果。
- 统计分析：设备总数、在库、借出、维修中、待处理报修、逾期未还，以及按状态/类别汇总。

## 运行

发布版入口：

```text
E:\dapm_qt\dist\DapmQt\DapmQt.exe
```

程序首次启动会自动创建本地数据库：

```text
E:\dapm_qt\dist\DapmQt\data\equipment.db
```

如果需要清空演示数据，关闭程序后删除 `equipment.db`，下次启动会重新创建示例数据。

## 重新构建

当前工程使用 Qt 5.15.0 msvc2019_64，Qt 路径为：

```text
D:\Software\QT\content\5.15.0\msvc2019_64
```

在 VS 2019 x64 编译环境下执行：

```bat
call "D:\Software\visual2019\content\VC\Auxiliary\Build\vcvars64.bat"
"D:\Software\QT\content\Tools\CMake_64\bin\cmake.exe" -S "E:\dapm_qt" -B "E:\dapm_qt\build" -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH="D:\Software\QT\content\5.15.0\msvc2019_64" -DCMAKE_MAKE_PROGRAM="D:\Software\QT\content\Tools\Ninja\ninja.exe"
"D:\Software\QT\content\Tools\CMake_64\bin\cmake.exe" --build "E:\dapm_qt\build" --config Release --parallel
```

打包时请确保 PATH 中优先使用 Qt 5.15.0，避免旧版本 Qt DLL 混入发布目录。
