# installer/

生成 PopKiller 的安装包。**当前使用 Inno Setup**（`PopKiller.iss` + `New-Setup.ps1`）；旧的 WiX MSI 脚本仍保留在目录里，但不再用于发布。

## 前置

1. 已构建 Release|x64（存在 `x64\Release\winui\winui.exe`）。
2. 安装 **Inno Setup 6.5+**（https://jrsoftware.org/isdl.php）。简体中文语言文件已随仓库提供：`installer/lang/ChineseSimplified.isl`。
   - `ISCC.exe` 默认查找顺序：`C:\Program Files (x86)\Inno Setup 6`、`C:\Program Files\Inno Setup 6`、`%LOCALAPPDATA%\Programs\Inno Setup 6`；也可用 `-Iscc` 显式指定。

## 使用

```
# 默认：从 x64\Release\winui 生成 dist\PopKiller-Beta<Version>-x64-setup.exe
pwsh -File installer\New-Setup.ps1

# 指定版本与输出名
pwsh -File installer\New-Setup.ps1 -Version 0.10.5 -SetupName PopKiller-Beta0.10.5-x64-setup
```

参数：`-AppOutput` `-OutputDir` `-Version`（默认 `0.10.5`）`-SetupName` `-Iscc`。

脚本会把构建输出复制到临时舞台，排除 `pdb/lib/exp` 与运行期文件（`rules.json`、`winui.ini`、`blocklog.txt`、`labels.json`、`*.log` 等），补入 VC++ 运行时（app-local），再调用 `ISCC` 编译；不修改 `x64\Release\winui` 本身。

## 生成的安装包特性

- **机器级安装**（`PrivilegesRequired=admin`，需 UAC 提权）；默认目录 `C:\PopKiller`（可改）。
- **简体中文界面**（`installer/lang/ChineseSimplified.isl`）。
- **附加任务**：创建桌面快捷方式（默认勾选）、开机自动启动（默认不勾选，写 `HKCU\…\Run\PopKiller`）。
- 开始菜单快捷方式固定创建。
- 安装目录授予 `Users` 修改权限（程序需要写 `rules.json` / 日志 / 模型）。
- 随包携带 **VC++ 运行时**（app-local）。
- **升级**：同 `AppId` 直接覆盖安装；安装过程**不删除**既有文件，`rules.json` / `winui.ini` / `labels.json` / `StaticML\` 原样保留（无需备份-恢复）；`UsePreviousAppDir` / `UsePreviousTasks` 会记住上次的目录与任务勾选。
- **降级保护**：`[Code] InitializeSetup` 读取卸载项 `PopKiller_is1` 的 `DisplayVersion`，比当前包新时提示并中止。
- **旧 MSI 检测**：发现 `HKLM\Software\PopKiller` 标记（MSI 安装遗留）时提示先卸载旧 MSI。
- **卸载清理**：
  - `[UninstallDelete]` 删除运行期文件（`rules.json`(+`.bak`)、`winui.ini`、`blocklog.txt`、`labels.json`、`crash.log`、`autostart_debug.log`、`notifydiag.txt`、`*.log`、`*.dmp`、`StaticML\`）；
  - `CurUninstallStepChanged` 删除 `HKCU`/`HKLM` 的 `…\Run\PopKiller` 值、遗留启动文件夹快捷方式（`PopKiller.lnk`），并随 `Software\PopKiller` 键一起清理。

## 从 MSI 迁移到 Inno Setup

1. **先备份**安装目录（默认 `C:\PopKiller`）里的 `rules.json`、`winui.ini`、`labels.json` 与 `StaticML\` 文件夹；
2. 在「设置 → 应用 → 已安装的应用」中卸载 PopKiller（MSI 卸载会清理运行期数据）；
3. 运行新的 `PopKiller-Beta<x.y.z>-x64-setup.exe`；
4. 把备份的文件放回安装目录，重启程序即可恢复规则与模型。

## 旧 MSI（已弃用）

`New-Msi.ps1` + `CleanupCA/` 为 WiX v5 方案，保留仅作参考：`pwsh -File installer\New-Msi.ps1`（需 `dotnet tool install --global wix --version 5.0.2`）。
