; AppEncrypt Inno Setup 脚本
; 构建: scripts\build_installer.bat

#ifndef StagingDir
  #define StagingDir "..\dist\staging"
#endif

#ifndef MyAppVersion
  #define MyAppVersion "1.0.0"
#endif

#define MyAppName "AppEncrypt"
#define MyAppPublisher "AppEncrypt"
#define MyAppExeName "AppEncrypt.exe"

[Setup]
AppId={{8F3C2A1B-9D4E-4F56-A8B2-1C3D5E7F9012}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppVerName={#MyAppName} {#MyAppVersion}
AppPublisher={#MyAppPublisher}
DefaultDirName={autopf}\{#MyAppName}
DefaultGroupName={#MyAppName}
DisableProgramGroupPage=yes
OutputDir=..\dist
OutputBaseFilename={#MyAppName}-Setup-{#MyAppVersion}
Compression=lzma2/ultra64
SolidCompression=yes
WizardStyle=modern
PrivilegesRequired=admin
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
UninstallDisplayName={#MyAppName}
CloseApplications=force
ChangesAssociations=no
VersionInfoVersion={#MyAppVersion}
VersionInfoCompany={#MyAppPublisher}
VersionInfoDescription={#MyAppName} 安装程序
VersionInfoProductName={#MyAppName}
VersionInfoProductVersion={#MyAppVersion}

[Languages]
Name: "cn"; MessagesFile: "compiler:Default.isl"

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"; Flags: unchecked

[Files]
Source: "{#StagingDir}\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
Name: "{group}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"
Name: "{autodesktop}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"; Tasks: desktopicon

[Run]
Filename: "{app}\{#MyAppExeName}"; Parameters: "register-shell --quiet"; StatusMsg: "正在注册 exe 右键菜单..."; Flags: waituntilidle runhidden postinstall skipifsilent

[UninstallRun]
Filename: "{app}\{#MyAppExeName}"; Parameters: "uninstall-restore --quiet"; RunOnceId: "AppEncryptRestore"; Flags: waituntilidle runhidden
Filename: "{app}\{#MyAppExeName}"; Parameters: "unregister-shell --quiet"; RunOnceId: "AppEncryptUnregisterShell"; Flags: waituntilidle runhidden

[UninstallDelete]
Type: filesandordirs; Name: "{commonappdata}\AppEncrypt"

[Messages]
cn.WelcomeLabel2=这将在您的计算机上安装 [name/ver]。%n%n安装完成后，可在任意 exe 文件上使用右键加密菜单。
cn.ClickFinish=安装已完成。您现在可以从开始菜单启动 AppEncrypt，或在 exe 文件上右键使用加密功能。
cn.FinishedHeadingLabel=安装完成
cn.FinishedLabel=安装向导已完成 AppEncrypt 的安装。
cn.ConfirmUninstall=确定要完全卸载 %1 及其所有组件吗？%n%n卸载前将自动还原所有已加密的程序。
cn.StatusCreateIcons=正在创建快捷方式...
cn.StatusRunProgram=正在注册右键菜单...
