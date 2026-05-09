; ===================================================================
;  VoLaura — Modern Inno Setup Wizard (Multi-language, polished)
;  Build:  iscc installer\installer.iss
;  Output: dist\VoLaura-Setup-{version}.exe
; ===================================================================

#define AppName       "VoLaura"
#define AppVersion    "1.1.0"
#define AppPublisher  "VoLaura"
#define AppURL        "https://volaura.xyz"
#define AppSupportURL "https://volaura.xyz/support"
#define AppExeName    "VoLaura.exe"
#define AppCopyright  "Copyright (C) 2026 VoLaura"

[Setup]
AppId={{9E0F3B81-5A2D-4F2C-9A6B-6B6F4F5D2E31}
AppName={#AppName}
AppVersion={#AppVersion}
AppVerName={#AppName} {#AppVersion}
AppPublisher={#AppPublisher}
AppPublisherURL={#AppURL}
AppSupportURL={#AppSupportURL}
AppUpdatesURL={#AppURL}
AppContact={#AppURL}
AppCopyright={#AppCopyright}
VersionInfoVersion={#AppVersion}.0
VersionInfoCompany={#AppPublisher}
VersionInfoDescription={#AppName} - Sesli sohbet, ekran paylaşımı ve E2E mesajlaşma
VersionInfoProductName={#AppName}
VersionInfoCopyright={#AppCopyright}

DefaultDirName={autopf}\{#AppName}
DefaultGroupName={#AppName}
DisableProgramGroupPage=yes
DisableReadyPage=no
DisableFinishedPage=no
DisableWelcomePage=no
AllowNoIcons=yes
AllowCancelDuringInstall=yes
CloseApplications=force
CloseApplicationsFilter=*.exe,*.dll
RestartApplications=no

OutputDir=..\dist
OutputBaseFilename=VoLaura-Setup-{#AppVersion}
Compression=lzma2/ultra64
SolidCompression=yes
LZMAUseSeparateProcess=yes
LZMANumBlockThreads=4

WizardStyle=modern
WizardResizable=no
WizardSizePercent=120,120
ShowLanguageDialog=yes
UsePreviousLanguage=no

ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
PrivilegesRequired=lowest
PrivilegesRequiredOverridesAllowed=dialog
MinVersion=10.0

UninstallDisplayIcon={app}\{#AppExeName}
UninstallDisplayName={#AppName} {#AppVersion}
UninstallFilesDir={app}\uninstall

LicenseFile=LICENSE.txt
SetupIconFile=..\assets\volaura-logo.ico
WizardImageFile=images\wizard-large.bmp
WizardSmallImageFile=images\wizard-small2x.bmp
WizardImageAlphaFormat=defined

[Languages]
Name: "turkish";    MessagesFile: "compiler:Languages\Turkish.isl";    LicenseFile: "LICENSE.txt"
Name: "english";   MessagesFile: "compiler:Default.isl";              LicenseFile: "LICENSE_en.txt"
Name: "german";     MessagesFile: "compiler:Languages\German.isl";     LicenseFile: "LICENSE_de.txt"
Name: "french";     MessagesFile: "compiler:Languages\French.isl";     LicenseFile: "LICENSE_fr.txt"
Name: "spanish";    MessagesFile: "compiler:Languages\Spanish.isl";    LicenseFile: "LICENSE_es.txt"
Name: "italian";    MessagesFile: "compiler:Languages\Italian.isl";    LicenseFile: "LICENSE_en.txt"
Name: "portuguese"; MessagesFile: "compiler:Languages\Portuguese.isl"; LicenseFile: "LICENSE_en.txt"
Name: "dutch";      MessagesFile: "compiler:Languages\Dutch.isl";      LicenseFile: "LICENSE_en.txt"
Name: "polish";     MessagesFile: "compiler:Languages\Polish.isl";     LicenseFile: "LICENSE_en.txt"
Name: "russian";    MessagesFile: "compiler:Languages\Russian.isl";    LicenseFile: "LICENSE_en.txt"
Name: "ukrainian";  MessagesFile: "compiler:Languages\Ukrainian.isl";  LicenseFile: "LICENSE_en.txt"
Name: "czech";      MessagesFile: "compiler:Languages\Czech.isl";      LicenseFile: "LICENSE_en.txt"

[CustomMessages]
turkish.LaunchAfter=VoLaura'yı kurulumdan sonra başlat
turkish.AutoStart=Windows başlangıcında VoLaura'yı otomatik başlat
turkish.AutoStartDesc=Bilgisayarı açtığında VoLaura sistem tepsisinden başlatılır.
turkish.WelcomeTitle=VoLaura Kurulum Sihirbazı'na Hoş Geldiniz
turkish.WelcomeMsg=Sesli sohbet, ekran paylaşımı, video çağrı ve uçtan uca şifrelenmiş mesajlaşma — hepsi tek uygulamada.%n%nKuruluma devam etmek için İleri'ye tıklayın.
turkish.FinishedDesc=VoLaura artık yüklü. Aşağıdaki seçeneklerle başlayın.
turkish.RuntimeReady=64-bit Windows 10 / 11 hazır.
english.LaunchAfter=Launch VoLaura after installation
english.AutoStart=Start VoLaura automatically with Windows
english.AutoStartDesc=VoLaura will start in the system tray when your computer boots.
english.WelcomeTitle=Welcome to the VoLaura Setup Wizard
english.WelcomeMsg=Voice chat, screen sharing, video calls and end-to-end encrypted messaging — all in one app.%n%nClick Next to continue.
english.FinishedDesc=VoLaura is installed. Choose your starting option below.
english.RuntimeReady=64-bit Windows 10 / 11 ready.

[Tasks]
Name: "desktopicon";   Description: "{cm:CreateDesktopIcon}";    GroupDescription: "{cm:AdditionalIcons}"; Flags: checkedonce
Name: "quicklaunch";   Description: "{cm:CreateQuickLaunchIcon}"; GroupDescription: "{cm:AdditionalIcons}"; Flags: unchecked
Name: "autostart";     Description: "{cm:AutoStart}"; GroupDescription: "{cm:AdditionalIcons}"; Flags: unchecked

[Files]
; Lisans dosyaları (kullanıcı dilinde uygulama klasöründe bulunsun)
Source: "LICENSE.txt";    DestDir: "{app}"; Flags: ignoreversion
Source: "LICENSE_en.txt"; DestDir: "{app}"; Flags: ignoreversion
Source: "LICENSE_de.txt"; DestDir: "{app}"; Flags: ignoreversion
Source: "LICENSE_fr.txt"; DestDir: "{app}"; Flags: ignoreversion
Source: "LICENSE_es.txt"; DestDir: "{app}"; Flags: ignoreversion
; windeployqt ile hazırlanmış dağıtım klasörü
Source: "..\dist\VoLauraApp\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
Name: "{group}\{#AppName}";                                                Filename: "{app}\{#AppExeName}"; IconFilename: "{app}\{#AppExeName}"; Comment: "{#AppName} - {#AppVersion}"
Name: "{group}\{cm:UninstallProgram,{#AppName}}";                          Filename: "{uninstallexe}"
Name: "{autodesktop}\{#AppName}";                                          Filename: "{app}\{#AppExeName}"; Tasks: desktopicon; IconFilename: "{app}\{#AppExeName}"
Name: "{userappdata}\Microsoft\Internet Explorer\Quick Launch\{#AppName}"; Filename: "{app}\{#AppExeName}"; Tasks: quicklaunch

[Registry]
; Otomatik başlatma (kullanıcı seçerse)
Root: HKCU; Subkey: "Software\Microsoft\Windows\CurrentVersion\Run"; ValueType: string; ValueName: "{#AppName}"; ValueData: """{app}\{#AppExeName}"" --minimized"; Tasks: autostart; Flags: uninsdeletevalue
; volaura:// URL handler (deeplink desteği)
Root: HKCU; Subkey: "Software\Classes\volaura";                          ValueType: string; ValueName: "";              ValueData: "URL:VoLaura Protocol"; Flags: uninsdeletekey
Root: HKCU; Subkey: "Software\Classes\volaura";                          ValueType: string; ValueName: "URL Protocol"; ValueData: ""
Root: HKCU; Subkey: "Software\Classes\volaura\DefaultIcon";              ValueType: string; ValueName: "";              ValueData: "{app}\{#AppExeName},0"
Root: HKCU; Subkey: "Software\Classes\volaura\shell\open\command";       ValueType: string; ValueName: "";              ValueData: """{app}\{#AppExeName}"" ""%1"""
; Uygulama yolu (Windows Run/Open ile bulunabilsin)
Root: HKCU; Subkey: "Software\Microsoft\Windows\CurrentVersion\App Paths\{#AppExeName}"; ValueType: string; ValueName: ""; ValueData: "{app}\{#AppExeName}"; Flags: uninsdeletekey
Root: HKCU; Subkey: "Software\Microsoft\Windows\CurrentVersion\App Paths\{#AppExeName}"; ValueType: string; ValueName: "Path"; ValueData: "{app}"

[Run]
Filename: "{app}\{#AppExeName}"; Description: "{cm:LaunchAfter}"; Flags: nowait postinstall skipifsilent unchecked

[UninstallRun]
; Uygulama açıksa kapat
Filename: "{cmd}"; Parameters: "/C taskkill /f /im {#AppExeName}"; Flags: runhidden; RunOnceId: "KillVoLaura"

[UninstallDelete]
Type: filesandordirs; Name: "{userappdata}\VoLaura\cache"
Type: filesandordirs; Name: "{userappdata}\VoLaura\logs"
Type: dirifempty;     Name: "{userappdata}\VoLaura"

[Code]
function InitializeSetup(): Boolean;
begin
  Result := True;
end;
