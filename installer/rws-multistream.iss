; Red Warden Multistream - Windows installer (Inno Setup 6)
;
; Built by tools\package.ps1, which passes /DAppVersion=... and /DStage=... and the
; signing tool. Do not run ISCC on this file by hand unless you pass the same defines.
;
; Installs to C:\ProgramData\obs-studio\plugins\rws-multistream\ - the per-machine
; plugin folder OBS 28+ scans. Upgrading = running a newer installer over the old one;
; destinations (rws-multistream.json in each OBS profile) and stream keys (Windows
; Credential Manager) live elsewhere and are never touched, on install or uninstall.

#ifndef AppVersion
  #error AppVersion must be passed: /DAppVersion=1.0.0
#endif
#ifndef Stage
  #error Stage must be passed: /DStage=<folder holding bin\64bit and data>
#endif

[Setup]
; Never change AppId - it is how a newer installer finds and replaces an older install.
AppId={{DA4CA68C-DB1B-4C5B-83B5-9BFB615B4E8B}
AppName=Red Warden Multistream
AppVersion={#AppVersion}
AppVerName=Red Warden Multistream {#AppVersion}
AppPublisher=Red Warden Studios LLC
AppPublisherURL=https://redwardenstudios.com/division/bastion/
AppSupportURL=https://redwardenstudios.com/division/bastion/
AppUpdatesURL=https://redwardenstudios.com/division/bastion/
VersionInfoVersion={#AppVersion}
VersionInfoCompany=Red Warden Studios LLC
VersionInfoDescription=Red Warden Multistream installer
DefaultDirName={commonappdata}\obs-studio\plugins\rws-multistream
DisableDirPage=yes
DisableProgramGroupPage=yes
PrivilegesRequired=admin
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
LicenseFile=..\LICENSE
SetupIconFile=..\brand\logo.ico
UninstallDisplayIcon={app}\logo.ico
UninstallDisplayName=Red Warden Multistream (OBS plugin)
OutputDir=..\dist
OutputBaseFilename=red-warden-multistream-{#AppVersion}-windows-x64-setup
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
#ifndef NoSign
SignTool=rws
SignedUninstaller=yes
#endif

[Files]
; The DLL arrives already signed by package.ps1; Inno signs only setup.exe and the uninstaller.
Source: "{#Stage}\bin\64bit\rws-multistream.dll"; DestDir: "{app}\bin\64bit"; Flags: ignoreversion
Source: "{#Stage}\data\*"; DestDir: "{app}\data"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "..\LICENSE"; DestDir: "{app}"; DestName: "LICENSE.txt"; Flags: ignoreversion
Source: "..\NOTICE.txt"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\brand\logo.ico"; DestDir: "{app}"; Flags: ignoreversion

[InstallDelete]
; 0.1.x was installed under its working name, "Beacon".
Type: filesandordirs; Name: "{commonappdata}\obs-studio\plugins\rws-beacon"
; A dev build may have left a .pdb beside the DLL; a stale one confuses crash reports.
Type: files; Name: "{app}\bin\64bit\rws-multistream.pdb"

[Messages]
FinishedLabel=Red Warden Multistream is installed.%n%nStart OBS, then open Docks > Red Warden Multistream.

[Code]
function ObsIsRunning(): Boolean;
var
  Wmi, Procs: Variant;
begin
  Result := False;
  try
    Wmi := CreateOleObject('WbemScripting.SWbemLocator');
    Procs := Wmi.ConnectServer('.', 'root\CIMV2').ExecQuery(
      'SELECT ProcessId FROM Win32_Process WHERE Name = ''obs64.exe''');
    Result := Procs.Count > 0;
  except
    Result := False;  // can't tell - don't block the install on a WMI hiccup
  end;
end;

function WaitForObsToClose(): Boolean;
begin
  Result := True;
  while ObsIsRunning() do
  begin
    if MsgBox('OBS Studio is running. Close OBS, then click OK to continue.' + #13#10 +
              '(Plugins load only when OBS starts.)',
              mbInformation, MB_OKCANCEL) = IDCANCEL then
    begin
      Result := False;
      Exit;
    end;
  end;
end;

function InitializeSetup(): Boolean;
begin
  Result := WaitForObsToClose();
end;

function InitializeUninstall(): Boolean;
begin
  Result := WaitForObsToClose();
end;
