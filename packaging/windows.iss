[Setup]
AppId={{886D5F64-EDE0-4692-8EAA-F84C3DC63695}
AppName=Pixel Studio
AppVersion=0.1.0
DefaultDirName={localappdata}\Programs\Pixel Studio
DefaultGroupName=Pixel Studio
PrivilegesRequired=lowest
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
OutputDir=..\dist
OutputBaseFilename=PixelStudio-0.1.0-windows-x64-setup
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
LicenseFile=..\LICENSE
UninstallDisplayIcon={app}\pixelstudio.exe

[Files]
Source: "..\dist\windows\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
Name: "{group}\Pixel Studio"; Filename: "{app}\pixelstudio.exe"
Name: "{autodesktop}\Pixel Studio"; Filename: "{app}\pixelstudio.exe"; Tasks: desktopicon

[Tasks]
Name: "desktopicon"; Description: "Create a desktop shortcut"; Flags: unchecked

[Run]
Filename: "{app}\pixelstudio.exe"; Description: "Launch Pixel Studio"; Flags: nowait postinstall skipifsilent
