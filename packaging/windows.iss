[Setup]
AppId={{886D5F64-EDE0-4692-8EAA-F84C3DC63695}
AppName=PhotoShip
AppVersion=0.2.1
DefaultDirName={localappdata}\Programs\PhotoShip
DefaultGroupName=PhotoShip
PrivilegesRequired=lowest
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
OutputDir=..\dist
OutputBaseFilename=PhotoShip-0.2.1-windows-x64-setup
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
SetupIconFile=photoship.ico
LicenseFile=..\LICENSE
UninstallDisplayIcon={app}\photoship.exe

[Files]
Source: "..\dist\windows\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
Name: "{group}\PhotoShip"; Filename: "{app}\photoship.exe"
Name: "{autodesktop}\PhotoShip"; Filename: "{app}\photoship.exe"; Tasks: desktopicon

[Tasks]
Name: "desktopicon"; Description: "Create a desktop shortcut"; Flags: unchecked

[Run]
Filename: "{app}\photoship.exe"; Description: "Launch PhotoShip"; Flags: nowait postinstall skipifsilent
