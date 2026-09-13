// Aseprite
// Copyright (C) 2018-2023  Igara Studio S.A.
// Copyright (C) 2001-2018  David Capello
//
// This program is distributed under the terms of
// the End-User License Agreement for Aseprite.

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "app/app.h"
#include "app/commands/command.h"
#include "app/context.h"
#include "base/fs.h"
#include "net/http_request.h"
#include "net/http_response.h"
#include "ui/alert.h"
#include "ver/info.h"

#include <string>
#include <fstream>
#include <sstream>
#include <thread>
#include <cstdlib>

namespace app {

class AutoUpdateCommand : public Command {
public:
  AutoUpdateCommand();

protected:
  void onExecute(Context* context) override;
};

AutoUpdateCommand::AutoUpdateCommand()
  : Command(CommandId::AutoUpdate())
{
}

void AutoUpdateCommand::onExecute(Context* context)
{
  std::thread([]{
    net::HttpRequest request("https://api.github.com/repos/aseprite/aseprite/releases/latest");
    std::stringstream ss;
    net::HttpResponse response(&ss);
    
    if (request.send(response)) {
      std::string responseBody = ss.str();
      
      size_t tagPos = responseBody.find("\"tag_name\":");
      if (tagPos != std::string::npos) {
        size_t startQuote = responseBody.find("\"", tagPos + 11);
        if (startQuote != std::string::npos) {
          size_t endQuote = responseBody.find("\"", startQuote + 1);
          if (endQuote != std::string::npos) {
            std::string tag = responseBody.substr(startQuote + 1, endQuote - startQuote - 1);
            
            if (tag != get_app_version()) {
              size_t zipballPos = responseBody.find("\"zipball_url\":");
              if (zipballPos != std::string::npos) {
                size_t zipStartQuote = responseBody.find("\"", zipballPos + 14);
                if (zipStartQuote != std::string::npos) {
                  size_t zipEndQuote = responseBody.find("\"", zipStartQuote + 1);
                  if (zipEndQuote != std::string::npos) {
                    std::string zipUrl = responseBody.substr(zipStartQuote + 1, zipEndQuote - zipStartQuote - 1);
                    
                    std::string tempDir = base::get_temp_path();
                    std::string scriptPath = base::join_path(tempDir, "aseprite_updater.ps1");
                    
                    std::ofstream script(scriptPath);
                    if (script.is_open()) {
                      script << "param (\n";
                      script << "    [string]$ZipUrl = '" << zipUrl << "'\n";
                      script << ")\n";
                      script << "if (-not ([Security.Principal.WindowsPrincipal][Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)) {\n";
                      script << "    Start-Process powershell.exe -ArgumentList \"-NoProfile -ExecutionPolicy Bypass -File `\"$PSCommandPath`\"\" -Verb RunAs\n";
                      script << "    exit\n";
                      script << "}\n";
                      
                      script << "Write-Host 'Closing Aseprite...'\n";
                      script << "Stop-Process -Name 'aseprite' -Force -ErrorAction SilentlyContinue\n";
                      script << "Start-Sleep -Seconds 2\n";
                      
                      script << "$TempDir = $env:TEMP\n";
                      script << "$ZipPath = Join-Path $TempDir 'aseprite_update.zip'\n";
                      script << "$ExtractDir = Join-Path $TempDir 'aseprite_src'\n";
                      
                      script << "Write-Host 'Downloading source from GitHub...'\n";
                      script << "Invoke-WebRequest -Uri $ZipUrl -OutFile $ZipPath -UserAgent 'Aseprite-Auto-Update'\n";
                      
                      script << "Write-Host 'Extracting...'\n";
                      script << "if (Test-Path $ExtractDir) { Remove-Item -Recurse -Force $ExtractDir }\n";
                      script << "Expand-Archive -Path $ZipPath -DestinationPath $ExtractDir -Force\n";
                      
                      script << "$SrcDir = Get-ChildItem -Path $ExtractDir -Directory | Select-Object -First 1\n";
                      script << "Set-Location $SrcDir.FullName\n";
                      
                      script << "Write-Host 'Compiling...'\n";
                      script << "cmd.exe /c build.cmd\n";
                      script << "if ($LASTEXITCODE -ne 0) {\n";
                      script << "    Write-Host 'Build failed! Aborting update.'\n";
                      script << "    Start-Sleep -Seconds 5\n";
                      script << "    Start-Process 'aseprite.exe'\n";
                      script << "    exit 1\n";
                      script << "}\n";
                      
                      script << "Write-Host 'Replacing files...'\n";
                      script << "$InstallDir = 'D:\\Aesprite\\aseprite\\build\\bin'\n";
                      script << "Copy-Item -Path 'build\\bin\\*' -Destination $InstallDir -Force -Recurse\n";
                      
                      script << "Write-Host 'Relaunching...'\n";
                      script << "Start-Process (Join-Path $InstallDir 'aseprite.exe')\n";
                      script << "Write-Host 'Done!'\n";
                      script << "Start-Sleep -Seconds 2\n";
                      script.close();
                      
                      std::string command = "start /B powershell.exe -ExecutionPolicy Bypass -WindowStyle Normal -File \"" + scriptPath + "\"";
                      std::system(command.c_str());
                    }
                  }
                }
              }
            }
          }
        }
      }
    }
  }).detach();
}

Command* CommandFactory::createAutoUpdateCommand()
{
  return new AutoUpdateCommand;
}

} // namespace app
