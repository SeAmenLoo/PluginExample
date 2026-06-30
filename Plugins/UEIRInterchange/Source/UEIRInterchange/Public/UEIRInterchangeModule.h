#pragma once

#include "Modules/ModuleManager.h"

class FUEIRInterchangeModule final : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

private:
	void RegisterMenus();
	void ExportSelectedAssetsToDefaultLocation();
	void ExportSelectedAssetsToDefaultLocationSingleFile();
	void GenerateReportFromUEIRJsonFile();
	void ExportSelectedMaterialsToDefaultLocation();
	void ImportMaterialFromJsonFile();
};
