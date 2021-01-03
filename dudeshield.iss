; Script generated by the Inno Setup Script Wizard.
; SEE THE DOCUMENTATION FOR DETAILS ON CREATING INNO SETUP SCRIPT FILES!

#define Major
#define Minor
#define Build
#define Dummy
#define ApplicationName "DudeShield"
#define BaseDir  ".\"
#define ApplicationExeName "dudeshield.exe"
#define ApplicationFullPath BaseDir + ApplicationExeName
#expr ParseVersion(ApplicationFullPath, Major, Minor,Build,Dummy)
#define ApplicationVersion Str(Major) + "." + Str(Minor)
#define ApplicationPublisher "Artemia76"
#define ApplicationURL "https://github.com/OpenShield/DudeShield"
#define InstallFileName "dudeshield_" + str(Major) +"_" + str(Minor) + "_win"

[Setup]
; NOTE: The value of AppId uniquely identifies this application.
; Do not use the same AppId value in installers for other applications.
; (To generate a new GUID, click Tools | Generate GUID inside the IDE.)
ArchitecturesInstallIn64BitMode=x64
AppId={{639F1D5A-3E8E-47B2-8DCF-0E042E72AC6D}}
AppName={#ApplicationName}
AppVerName={#ApplicationName} {#ApplicationVersion}
AppPublisher={#ApplicationPublisher}
AppPublisherURL={#ApplicationURL}
AppSupportURL={#ApplicationURL}
AppUpdatesURL={#ApplicationURL}
DefaultDirName={pf}\{#ApplicationName}
DisableProgramGroupPage=yes
LicenseFile=gpl-2.0.md
OutputDir=.\
OutputBaseFilename= {#InstallFileName}
SetupIconFile=.\dudeshield.ico
Compression=lzma
SolidCompression=yes

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"
Name: "french"; MessagesFile: "compiler:Languages\French.isl"

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"; Flags: unchecked
Name: "quicklaunchicon"; Description: "{cm:CreateQuickLaunchIcon}"; GroupDescription: "{cm:AdditionalIcons}"; Flags: unchecked; OnlyBelowVersion: 0,6.1

[Files]
source: "{#BaseDir}vc_redist.x64.exe"; DestDir: "{tmp}"; Flags: nocompression createallsubdirs recursesubdirs deleteafterinstall
Source: "{#ApplicationFullPath}"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#BaseDir}Qt5Core.dll"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#BaseDir}Qt5Gui.dll"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#BaseDir}Qt5Multimedia.dll"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#BaseDir}Qt5Network.dll"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#BaseDir}Qt5SerialPort.dll"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#BaseDir}Qt5Svg.dll"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#BaseDir}Qt5Widgets.dll"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#BaseDir}audio\*"; DestDir: "{app}\audio"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "{#BaseDir}bearer\*"; DestDir: "{app}\bearer"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "{#BaseDir}iconengines\*"; DestDir: "{app}\iconengines"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "{#BaseDir}imageformats\*"; DestDir: "{app}\imageformats"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "{#BaseDir}mediaservice\*"; DestDir: "{app}\mediaservice"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "{#BaseDir}platforms\*"; DestDir: "{app}\platforms"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "{#BaseDir}playlistformats\*"; DestDir: "{app}\playlistformats"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "{#BaseDir}styles\*"; DestDir: "{app}\styles"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "{#BaseDir}translations\*"; DestDir: "{app}\translations"; Flags: ignoreversion recursesubdirs createallsubdirs

; NOTE: Don't use "Flags: ignoreversion" on any shared system files

[Icons]
Name: "{commonprograms}\{#ApplicationName}"; Filename: "{app}\{#ApplicationExeName}"
Name: "{commondesktop}\{#ApplicationName}"; Filename: "{app}\{#ApplicationExeName}"; Tasks: desktopicon
Name: "{userappdata}\Microsoft\Internet Explorer\Quick Launch\{#ApplicationName}"; Filename: "{app}\{#ApplicationExeName}"; Tasks: quicklaunchicon

[Run]
Filename: "{app}\{#ApplicationExeName}"; Description: "{cm:LaunchProgram,{#StringChange(ApplicationName, '&', '&&')}}"; Flags: nowait postinstall skipifsilent
Filename: "{tmp}\vc_redist.x64.exe"; Parameters: "/Q"; Flags: waituntilterminated skipifdoesntexist; StatusMsg: "Microsoft Visual C++ 2019 (x64) installation. Please Wait..."

