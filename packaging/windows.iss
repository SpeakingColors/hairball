; Windows installer + uninstaller (Inno Setup). Called as: ISCC /DName=Hairball "/DProduct=Hairball" /DVersion=0.1.0 packaging/windows.iss
; Files are staged in ..\stage by the release workflow. Works offline.
; Installs two VST3 plugins (Hairball for instrument tracks, Hairball FX for audio tracks) plus the standalone app and the manual.
; The uninstaller removes everything it installed. Your saved presets (%APPDATA%\Studio 87\Hairball) are kept
; unless you answer Yes when it asks. A silent uninstall (/SILENT, /VERYSILENT) always keeps them.
[Setup]
AppName={#Product}
AppVersion={#Version}
AppPublisher=Studio 87
AppPublisherURL=https://github.com/SpeakingColors
DefaultDirName={autopf}\Studio 87\{#Product}
DefaultGroupName=Studio 87
OutputDir=..\Output
OutputBaseFilename={#Name}-Setup
LicenseFile=..\LICENSE
PrivilegesRequired=admin
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
Compression=lzma2
SolidCompression=yes
UninstallDisplayName={#Product}
UninstallDisplayIcon={app}\{#Product}.exe
CloseApplications=yes

[Messages]
SelectDirLabel3=Setup will install the standalone app and the manual into the following folder. The two VST3 plugins (Hairball and Hairball FX) are installed separately into C:\Program Files\Common Files\VST3, where DAWs look for them.
SelectDirBrowseLabel=To continue, click Next. If you would like to select a different folder for the standalone app and manual, click Browse.

[Files]
Source: "..\stage\vst3\{#Product}.vst3\*"; DestDir: "{commoncf64}\VST3\{#Product}.vst3"; Flags: recursesubdirs createallsubdirs ignoreversion
Source: "..\stage\vst3\{#Product} FX.vst3\*"; DestDir: "{commoncf64}\VST3\{#Product} FX.vst3"; Flags: recursesubdirs createallsubdirs ignoreversion
Source: "..\stage\{#Product}.exe"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\stage\{#Name}-manual.pdf"; DestDir: "{app}"; Flags: ignoreversion

[Icons]
Name: "{group}\{#Product}"; Filename: "{app}\{#Product}.exe"
Name: "{group}\{#Product} manual"; Filename: "{app}\{#Name}-manual.pdf"
Name: "{group}\Uninstall {#Product}"; Filename: "{uninstallexe}"

[UninstallDelete]
; the plugin folders are removed by the uninstall log; these catch anything the program wrote next to its files
Type: filesandordirs; Name: "{commoncf64}\VST3\{#Product}.vst3"
Type: filesandordirs; Name: "{commoncf64}\VST3\{#Product} FX.vst3"
Type: filesandordirs; Name: "{app}"
; standalone app settings (audio device choice only, no presets); unverified exact location, removed if present
Type: filesandordirs; Name: "{userappdata}\{#Product}"
Type: files; Name: "{userappdata}\{#Product}.settings"

[Code]
procedure CurUninstallStepChanged(CurUninstallStep: TUninstallStep);
var
  presets, vendor: String;
begin
  if CurUninstallStep <> usPostUninstall then exit;
  presets := ExpandConstant('{userappdata}\Studio 87\{#Product}');
  vendor := ExpandConstant('{userappdata}\Studio 87');
  if DirExists(presets) and (not UninstallSilent) then
    if MsgBox('Also delete your saved {#Product} presets?' + #13#10 + #13#10 + presets + #13#10 + #13#10 +
              'Choose No to keep them (recommended if you plan to reinstall).', mbConfirmation, MB_YESNO or MB_DEFBUTTON2) = IDYES then
      DelTree(presets, True, True, True);
  RemoveDir(vendor);   { only succeeds when empty }
end;
