#pragma once

#include "Commandlets/Commandlet.h"

#include "UEIRInterchangeCommandlet.generated.h"

UCLASS()
class UEIRINTERCHANGE_API UUEIRInterchangeCommandlet final : public UCommandlet
{
	GENERATED_BODY()

public:
	UUEIRInterchangeCommandlet();
	virtual int32 Main(const FString& Params) override;
};
