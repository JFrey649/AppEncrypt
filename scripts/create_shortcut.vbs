' Helper: create_shortcut.vbs <TargetExe> <ShortcutPath> <WorkingDir> [Description]
If WScript.Arguments.Count < 3 Then
    WScript.Echo "Usage: create_shortcut.vbs TargetExe ShortcutPath WorkingDir [Description]"
    WScript.Quit 1
End If

target = WScript.Arguments(0)
shortcutPath = WScript.Arguments(1)
workDir = WScript.Arguments(2)
description = ""
If WScript.Arguments.Count >= 4 Then
    description = WScript.Arguments(3)
End If

Set shell = CreateObject("WScript.Shell")
Set shortcut = shell.CreateShortcut(shortcutPath)
shortcut.TargetPath = target
shortcut.WorkingDirectory = workDir
shortcut.IconLocation = target & ",0"
If description <> "" Then shortcut.Description = description
shortcut.Save
