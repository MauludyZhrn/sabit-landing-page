// Sabit Central Application & Release Configuration
// Single source of truth for verified links, release assets, and scientific attributions.

export interface AppConfig {
  appName: string;
  version: string;
  edition: string;
  githubRepoUrl: string;
  windowsInstallerUrl: string;
  windowsInstallerFilename: string;
  installerSizeLabel: string;
  installerArch: string;
  lunarDataCredit: string;
}

export const APP_CONFIG: AppConfig = {
  appName: 'Sabit',
  version: '1.0.0',
  edition: 'Basic',
  githubRepoUrl: 'https://github.com/MauludyZhrn/Sabit',
  windowsInstallerUrl: '/downloads/Sabit-Setup-v1.0.0-x64.exe',
  windowsInstallerFilename: 'Sabit-Setup-v1.0.0-x64.exe',
  installerSizeLabel: '32.0 MB',
  installerArch: 'Windows 10 / 11 (64-bit)',
  lunarDataCredit: 'NASA/GSFC/Arizona State University — LRO/LOLA (CGI Moon Kit)'
};
