#define AppName "Финансовый помощник"
#define AppVersion "1.0.0"
#define AppPublisher "Financial Assistant"
#define AppExeName "BudgetTracker.exe"

[Setup]
AppId={{A9AA1EF1-5F5E-4E93-BB23-31D4774A52D9}
AppName={#AppName}
AppVersion={#AppVersion}
AppPublisher={#AppPublisher}
DefaultDirName={autopf}\Financial Assistant
DefaultGroupName={#AppName}
OutputDir=output
OutputBaseFilename=FinancialAssistantSetup
Compression=lzma2/ultra64
SolidCompression=yes
WizardStyle=modern
PrivilegesRequired=admin
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
UninstallDisplayIcon={app}\{#AppExeName}
SetupLogging=yes
CloseApplications=yes
RestartApplications=no
DisableProgramGroupPage=yes

[Languages]
Name: "russian"; MessagesFile: "compiler:Languages\Russian.isl"

[Tasks]
Name: "desktopicon"; Description: "Создать ярлык на рабочем столе"; GroupDescription: "Дополнительные значки:"

[Files]
Source: "..\x64\Release\BudgetTracker.exe"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\x64\Release\storage.config"; DestDir: "{app}"; Flags: ignoreversion
Source: "redist\vc_redist.x64.exe"; DestDir: "{tmp}"; Flags: deleteafterinstall

[Icons]
Name: "{autoprograms}\{#AppName}"; Filename: "{app}\{#AppExeName}"
Name: "{autodesktop}\{#AppName}"; Filename: "{app}\{#AppExeName}"; Tasks: desktopicon

[Run]
Filename: "{tmp}\vc_redist.x64.exe"; Parameters: "/install /quiet /norestart"; \
    StatusMsg: "Установка компонентов Microsoft Visual C++..."; Flags: waituntilterminated; \
    Check: not VCRuntimeInstalled
Filename: "{app}\{#AppExeName}"; Description: "Запустить {#AppName}"; \
    Flags: nowait postinstall skipifsilent

[Code]
function VCRuntimeInstalled: Boolean;
var
  Installed: Cardinal;
begin
  Result :=
    RegQueryDWordValue(
      HKLM64,
      'SOFTWARE\Microsoft\VisualStudio\14.0\VC\Runtimes\x64',
      'Installed',
      Installed
    ) and (Installed = 1);
end;

function DotNet48Installed: Boolean;
var
  Release: Cardinal;
begin
  Result :=
    RegQueryDWordValue(
      HKLM64,
      'SOFTWARE\Microsoft\NET Framework Setup\NDP\v4\Full',
      'Release',
      Release
    ) and (Release >= 528040);
end;

function InitializeSetup: Boolean;
begin
  Result := DotNet48Installed;
  if not Result then
    MsgBox(
      'Для работы приложения требуется .NET Framework 4.8. ' +
      'Установите его через Центр обновления Windows и повторите установку.',
      mbError,
      MB_OK
    );
end;
