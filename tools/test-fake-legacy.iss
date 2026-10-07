; TEST ONLY. A tiny stand-in for the old standalone "Red Warden Multistream" installer, so
; tools\test-installer.ps1 can check that the Stream Kit installer removes it. It has a TEST-ONLY
; AppId (never the real one), installs a few dummy files under <TestRoot>\rws-multistream\ and
; registers an uninstall entry in HKCU (no admin). Compiled into %TEMP% by the test; never shipped.
#ifndef TestRoot
  #error TestRoot must be passed: /DTestRoot=<scratch folder>
#endif

[Setup]
AppId=RWS-KIT-TESTLEGACY-DA4CA68C
AppName=TEST fake legacy Red Warden Multistream
AppVersion=1.0.0
DefaultDirName={#TestRoot}\rws-multistream
PrivilegesRequired=lowest
DisableDirPage=yes
DisableProgramGroupPage=yes
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
Compression=none
OutputBaseFilename=fake-legacy-setup

[Files]
; Any small file will do as a dummy payload: this script itself.
Source: "{#SourcePath}\test-fake-legacy.iss"; DestDir: "{app}\bin\64bit"; DestName: "rws-multistream.dll"; Flags: ignoreversion
Source: "{#SourcePath}\test-fake-legacy.iss"; DestDir: "{app}\data\locale"; DestName: "en-US.ini"; Flags: ignoreversion
Source: "{#SourcePath}\test-fake-legacy.iss"; DestDir: "{app}"; DestName: "LICENSE.txt"; Flags: ignoreversion
