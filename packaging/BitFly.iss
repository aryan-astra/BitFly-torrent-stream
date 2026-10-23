#ifndef SourceDir
  #error SourceDir must point to the staged BitFly deployment directory
#endif
#ifndef OutputDir
  #error OutputDir must point to the artifact directory
#endif
#ifndef AppVersion
  #error AppVersion must be set
#endif
#ifndef AppArch
  #error AppArch must be x64 or arm64
#endif

#if AppArch == "x64"
  #define AllowedArch "x64compatible"
#elif AppArch == "arm64"
  #define AllowedArch "arm64"
#else
  #error Unsupported AppArch
#endif

[Setup]
AppId={{8C7C7139-9634-4CA0-92B9-16C129DE05D2}
AppName=BitFly
AppVersion={#AppVersion}
AppPublisher=BitFly contributors
DefaultDirName={localappdata}\Programs\BitFly
DefaultGroupName=BitFly
DisableProgramGroupPage=yes
PrivilegesRequired=lowest
ArchitecturesAllowed={#AllowedArch}
ArchitecturesInstallIn64BitMode={#AllowedArch}
LicenseFile="{#SourceDir}\LICENSE"
OutputDir="{#OutputDir}"
OutputBaseFilename=BitFly-{#AppVersion}-windows-{#AppArch}-setup
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
UninstallDisplayIcon={app}\BitFly.exe

[Files]
Source: "{#SourceDir}\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
Name: "{autoprograms}\BitFly"; Filename: "{app}\BitFly.exe"
Name: "{autodesktop}\BitFly"; Filename: "{app}\BitFly.exe"; Tasks: desktopicon

[Tasks]
Name: "desktopicon"; Description: "Create a desktop shortcut"; GroupDescription: "Additional shortcuts:"; Flags: unchecked

[Run]
Filename: "{app}\BitFly.exe"; Description: "Launch BitFly"; Flags: nowait postinstall skipifsilent
