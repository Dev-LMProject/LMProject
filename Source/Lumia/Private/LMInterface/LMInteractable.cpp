// Fill out your copyright notice in the Description page of Project Settings.


#include "LMInterface/LMInteractable.h"


// Add default functionality here for any ILMInteractable functions that are not pure virtual.
bool ILMInteractable::IsInteractionBusy_Implementation() const
{
	return false;
}

ELMInteractionRequestHandling ILMInteractable::HandleInteractionRequest_Implementation(AActor* RequestActor)
{
	return ELMInteractionRequestHandling::PassThrough;
}
