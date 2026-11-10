// Fill out your copyright notice in the Description page of Project Settings.


#include "Hacking/LMHackingCell.h"

bool FLMHackingCell::ReceiveValue(uint8 InOriginalValue, uint8 NewValue, TArray<FLMHackingCell*>& PendingCells)
{
	if (CurrentValue != InOriginalValue) return false;	
	if (CurrentValue == NewValue) return false;
	
	CurrentValue = NewValue;
	for (auto it : Neighbors)
	{
		if (it!=nullptr&&it->GetCurrentValue() == InOriginalValue)
		{
			PendingCells.Add(it);
		}
	}
	return true;
}



