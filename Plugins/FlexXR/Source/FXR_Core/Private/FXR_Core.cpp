// Copyright (c) 2026 Low Sze Hao. All rights reserved.

#include "FXR_Core.h"
#include "Types/FXR_LogChannels.h"

#define LOCTEXT_NAMESPACE "FXR_Core"

DEFINE_LOG_CATEGORY(LogFXR);

void FFXR_CoreModule::StartupModule()
{
	UE_LOG(LogFXR, Log, TEXT("FXR_Core started."));
}

void FFXR_CoreModule::ShutdownModule()
{
	UE_LOG(LogFXR, Log, TEXT("FXR_Core shut down."));
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FFXR_CoreModule, FXR_Core)
