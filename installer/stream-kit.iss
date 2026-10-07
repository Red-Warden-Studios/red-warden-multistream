; Red Warden Stream Kit - Windows installer (Inno Setup 6)
;
; One installer, one checkbox per tool: Red Warden Multistream and Red Warden Pre-Flight.
; Built by tools\package.ps1, which passes /DAppVersion=... and /DStage=... and the
; signing tool. Do not run ISCC on this file by hand unless you pass the same defines.
;
; Each plugin installs where Multistream's own installer put it: the per-machine plugin
; folder OBS 28+ scans, C:\ProgramData\obs-studio\plugins\<module>\bin\64bit + data\locale.
; Upgrading = running a newer installer over the old one; destinations, settings and stream
; keys live elsewhere (OBS profile, Windows Credential Manager) and are never touched.
;
; TEST BUILDS ONLY: ISCC /DTestRoot=<dir> installs under <dir> instead of ProgramData, needs
; no admin rights, uses its own AppId, looks for a TEST-ONLY legacy AppId (never the real one),
; and never signs. The OBS-running check applies to test builds too. tools\test-installer.ps1
; uses it. Never ship a TestRoot build.

#ifndef AppVersion
  #error AppVersion must be passed: /DAppVersion=1.0.0
#endif
#ifndef Stage
  #error Stage must be passed: /DStage=<folder holding rws-multistream\ and rws-preflight\>
#endif

#ifdef TestRoot
  #define PluginRoot TestRoot
  #define LegacyAppId "RWS-KIT-TESTLEGACY-DA4CA68C"
#else
  #define PluginRoot "{commonappdata}\obs-studio\plugins"
  ; The AppId of the standalone Red Warden Multistream installer (rws-multistream.iss).
  #define LegacyAppId "{DA4CA68C-DB1B-4C5B-83B5-9BFB615B4E8B}"
#endif

[Setup]
; Never change AppId - it is how a newer installer finds and replaces an older install.
#ifdef TestRoot
AppId=RWS-KIT-TESTROOT-035D8EC0
#else
AppId={{035D8EC0-7626-4A56-BF6D-A31548414EFA}
#endif
AppName=Red Warden Stream Kit
AppVersion={#AppVersion}
AppVerName=Red Warden Stream Kit {#AppVersion}
AppPublisher=Red Warden Studios LLC
AppPublisherURL=https://redwardenstudios.com/division/bastion/stream-kit/
AppSupportURL=https://redwardenstudios.com/division/bastion/stream-kit/
AppUpdatesURL=https://redwardenstudios.com/division/bastion/stream-kit/
VersionInfoVersion={#AppVersion}
VersionInfoCompany=Red Warden Studios LLC
VersionInfoDescription=Red Warden Stream Kit installer
; {app} only holds the uninstaller, the licence and the logo. The plugins go under the
; plugin root, one folder per module.
#ifdef TestRoot
DefaultDirName={#PluginRoot}\_kit
PrivilegesRequired=lowest
#else
DefaultDirName={commonappdata}\Red Warden Studios\Stream Kit
PrivilegesRequired=admin
#endif
DisableDirPage=yes
DisableProgramGroupPage=yes
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
LicenseFile=..\LICENSE
SetupIconFile=..\..\red-warden-multistream\brand\logo.ico
UninstallDisplayIcon={app}\logo.ico
UninstallDisplayName=Red Warden Stream Kit (OBS plugins)
#ifdef TestRoot
OutputDir=..\.testbed
OutputBaseFilename=RedWardenStreamKit-{#AppVersion}-TestRoot-Setup
#else
OutputDir=..\dist
OutputBaseFilename=RedWardenStreamKit-{#AppVersion}-Setup
#endif
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
#if !defined(NoSign) && !defined(TestRoot)
SignTool=rws
SignedUninstaller=yes
#endif

[Types]
Name: "full"; Description: "Full installation (both tools)"
Name: "custom"; Description: "Custom installation"; Flags: iscustom

[Components]
Name: "multistream"; Description: "Red Warden Multistream - stream to several platforms at once"; Types: full custom
Name: "preflight"; Description: "Red Warden Pre-Flight - checks your setup before you go live"; Types: full custom

[Files]
; The DLLs arrive already signed by package.ps1; Inno signs only setup.exe and the uninstaller.
Source: "{#Stage}\rws-multistream\bin\64bit\rws-multistream.dll"; DestDir: "{#PluginRoot}\rws-multistream\bin\64bit"; Flags: ignoreversion; Components: multistream
Source: "{#Stage}\rws-multistream\data\*"; DestDir: "{#PluginRoot}\rws-multistream\data"; Flags: ignoreversion recursesubdirs createallsubdirs; Components: multistream
Source: "{#Stage}\rws-preflight\bin\64bit\rws-preflight.dll"; DestDir: "{#PluginRoot}\rws-preflight\bin\64bit"; Flags: ignoreversion; Components: preflight
Source: "{#Stage}\rws-preflight\data\*"; DestDir: "{#PluginRoot}\rws-preflight\data"; Flags: ignoreversion recursesubdirs createallsubdirs; Components: preflight
Source: "..\LICENSE"; DestDir: "{app}"; DestName: "LICENSE.txt"; Flags: ignoreversion
Source: "..\NOTICE.txt"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\..\red-warden-multistream\brand\logo.ico"; DestDir: "{app}"; Flags: ignoreversion

[InstallDelete]
; Never a whole-directory delete: a user's own files in a plugin folder must survive. Remove exactly
; the files these installers place (dll, a stray pdb, each locale ini), then each folder only if empty,
; bottom-up. A tool that is unchecked is removed if an earlier install left it. (When a locale is
; added, add its ini here too.)
Type: files; Name: "{#PluginRoot}\rws-multistream\bin\64bit\rws-multistream.dll"; Check: not WizardIsComponentSelected('multistream')
Type: files; Name: "{#PluginRoot}\rws-multistream\bin\64bit\rws-multistream.pdb"
Type: files; Name: "{#PluginRoot}\rws-multistream\data\locale\en-US.ini"; Check: not WizardIsComponentSelected('multistream')
Type: dirifempty; Name: "{#PluginRoot}\rws-multistream\data\locale"; Check: not WizardIsComponentSelected('multistream')
Type: dirifempty; Name: "{#PluginRoot}\rws-multistream\data"; Check: not WizardIsComponentSelected('multistream')
Type: dirifempty; Name: "{#PluginRoot}\rws-multistream\bin\64bit"; Check: not WizardIsComponentSelected('multistream')
Type: dirifempty; Name: "{#PluginRoot}\rws-multistream\bin"; Check: not WizardIsComponentSelected('multistream')
Type: dirifempty; Name: "{#PluginRoot}\rws-multistream"; Check: not WizardIsComponentSelected('multistream')
Type: files; Name: "{#PluginRoot}\rws-preflight\bin\64bit\rws-preflight.dll"; Check: not WizardIsComponentSelected('preflight')
Type: files; Name: "{#PluginRoot}\rws-preflight\bin\64bit\rws-preflight.pdb"
Type: files; Name: "{#PluginRoot}\rws-preflight\data\locale\en-US.ini"; Check: not WizardIsComponentSelected('preflight')
Type: dirifempty; Name: "{#PluginRoot}\rws-preflight\data\locale"; Check: not WizardIsComponentSelected('preflight')
Type: dirifempty; Name: "{#PluginRoot}\rws-preflight\data"; Check: not WizardIsComponentSelected('preflight')
Type: dirifempty; Name: "{#PluginRoot}\rws-preflight\bin\64bit"; Check: not WizardIsComponentSelected('preflight')
Type: dirifempty; Name: "{#PluginRoot}\rws-preflight\bin"; Check: not WizardIsComponentSelected('preflight')
Type: dirifempty; Name: "{#PluginRoot}\rws-preflight"; Check: not WizardIsComponentSelected('preflight')
; Multistream 0.1.x was installed under its working name, "Beacon": its files only, then its folders if empty.
Type: files; Name: "{#PluginRoot}\rws-beacon\bin\64bit\rws-beacon.dll"
Type: files; Name: "{#PluginRoot}\rws-beacon\bin\64bit\rws-beacon.pdb"
Type: files; Name: "{#PluginRoot}\rws-beacon\data\locale\en-US.ini"
Type: dirifempty; Name: "{#PluginRoot}\rws-beacon\data\locale"
Type: dirifempty; Name: "{#PluginRoot}\rws-beacon\data"
Type: dirifempty; Name: "{#PluginRoot}\rws-beacon\bin\64bit"
Type: dirifempty; Name: "{#PluginRoot}\rws-beacon\bin"
Type: dirifempty; Name: "{#PluginRoot}\rws-beacon"

[Messages]
FinishedLabel=Red Warden Stream Kit is installed.%n%nStart OBS, then open the Docks menu: Red Warden Multistream and Red Warden Pre-Flight.

[Code]
const
  // The standalone Red Warden Multistream installer's uninstall entry (its AppId; a test-only id in TestRoot builds).
  LegacyUninstallKey = 'Software\Microsoft\Windows\CurrentVersion\Uninstall\{#LegacyAppId}_is1';
  LegacyFailed = 'Couldn''t remove the old Red Warden Multistream - uninstall it from Windows Settings > Apps, then run this installer again.';

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

function WaitForObsToClose(Silent: Boolean): Boolean;
begin
  Result := True;
  while ObsIsRunning() do
  begin
    // A silent run has nobody to click OK: refuse instead of looping forever.
    if Silent then
    begin
      Result := False;
      Exit;
    end;
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
  Result := WaitForObsToClose(WizardSilent());
end;

function InitializeUninstall(): Boolean;
begin
  Result := WaitForObsToClose(UninstallSilent());
end;

// The old standalone Multistream installer registered its own uninstall entry. Two uninstallers
// owning the same plugin folder would fight, so remove the old one first (silently). Its
// destinations and stream keys live outside the plugin folder and are not touched.
function LegacyKeyExists(): Boolean;
begin
  Result := RegKeyExists(HKLM64, LegacyUninstallKey) or RegKeyExists(HKCU, LegacyUninstallKey);
end;

function LegacyUninstallString(): String;
begin
  Result := '';
  if not RegQueryStringValue(HKLM64, LegacyUninstallKey, 'UninstallString', Result) then
    if not RegQueryStringValue(HKCU, LegacyUninstallKey, 'UninstallString', Result) then
      Result := '';
  Result := RemoveQuotes(Result);
end;

function PrepareToInstall(var NeedsRestart: Boolean): String;
var
  Old: String;
  Code, I: Integer;
begin
  Result := '';
  if not LegacyKeyExists() then
    Exit;
  Old := LegacyUninstallString();
  Log('Removing the standalone Multistream install first: ' + Old);
  if (Old = '') or not FileExists(Old) then
  begin
    Result := LegacyFailed;
    Exit;
  end;
  // Require the uninstaller's exit code 0 AND its registry entry gone afterwards.
  if not Exec(Old, '/VERYSILENT /SUPPRESSMSGBOXES /NORESTART', '', SW_HIDE, ewWaitUntilTerminated, Code) or (Code <> 0) then
  begin
    Result := LegacyFailed;
    Exit;
  end;
  // The uninstaller finishes in a second process: wait (up to 60 s) until it has removed its registry
  // entry AND deleted itself. Require both, or abort.
  I := 0;
  while (LegacyKeyExists() or FileExists(Old)) and (I < 300) do
  begin
    Sleep(200);
    I := I + 1;
  end;
  if LegacyKeyExists() or FileExists(Old) then
    Result := LegacyFailed;
end;
