Dim fso, wshShell, scriptDir
Set fso = CreateObject("Scripting.FileSystemObject")
Set wshShell = CreateObject("WScript.Shell")

scriptDir = fso.GetParentFolderName(WScript.ScriptFullName)

wshShell.Environment("Process").Item("APPDATA") = scriptDir & "\data"

wshShell.Run """" & scriptDir & "\EnhanceVision.exe""", 0, False
