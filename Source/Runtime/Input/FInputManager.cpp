#include "FInputManager.h"

#include "Runtime/Core/IntTypes.h"
#include <Windows.h>
#include <cstring>

void FInputManager::BeginFrame()
{
	memcpy(PreviousKeyStates, CurrentKeyStates, sizeof(bool) * MAX_KEYS);
	for (int i = 0; i < MAX_KEYS; ++i)
	{
		CurrentKeyStates[i] = GetAsyncKeyState(i) & 0x8000 ? true : false;
	}

	MouseDelta = CurrentMousePosition - PreviousMousePosition;
	PreviousMousePosition = CurrentMousePosition;
	WheelMoveAccumLast = WheelMoveAccum;
	WheelMoveAccum = 0.f;
}


bool FInputManager::IsKeyDown(uint32 Key) const
{
	if (Key >= MAX_KEYS)
	{
		return false;
	}
	return CurrentKeyStates[Key];
}

bool FInputManager::IsKeyJustPressed(uint32 Key) const
{
	if (Key >= MAX_KEYS)
	{
		return false;
	}
	return IsKeyDown(Key) && !IsPrevKeyDown(Key);
}

bool FInputManager::IsKeyJustReleased(uint32 Key) const
{
	if (Key >= MAX_KEYS)
	{
		return false;
	}
	return !IsKeyDown(Key) && IsPrevKeyDown(Key);
}

bool FInputManager::IsMouseDown(EMouseButton Button) const
{
	switch (Button)
	{
	case EMouseButton::Left:
		return bMouseLeftPressed;
	case EMouseButton::Right:
		return bMouseRightPressed;
	case EMouseButton::Middle:
		return bMouseMiddlePressed;
	}

	return false;
}

void FInputManager::OnMouseMove(FVector2 Position)
{
	CurrentMousePosition = Position;
}

void FInputManager::OnMouseButtonDown(EMouseButton Button, FVector2 Position)
{
	switch (Button)
	{
	case EMouseButton::Left:
		bMouseLeftPressed = true;
		break;
	case EMouseButton::Right:
		bMouseRightPressed = true;
		break;
	case EMouseButton::Middle:
		bMouseMiddlePressed = true;
		break;
	}

	CurrentMousePosition = Position;
	PreviousMousePosition = Position;
}

void FInputManager::OnMouseButtonUp(EMouseButton Button, FVector2 Position)
{
	switch (Button)
	{
	case EMouseButton::Left:
		bMouseLeftPressed = false;
		break;
	case EMouseButton::Right:
		bMouseRightPressed = false;
		break;
	case EMouseButton::Middle:
		bMouseMiddlePressed = false;
		break;
	}

	CurrentMousePosition = Position;
	PreviousMousePosition = Position;
}

void FInputManager::OnMouseWheelScroll(int16 WheelDelta)
{
	WheelMoveAccum += static_cast<float>(WheelDelta) / WHEEL_DELTA;
}

float FInputManager::GetMouseWheelScroll()
{
	return WheelMoveAccumLast;
}

FVector2 FInputManager::GetMousePosition() const
{
	return CurrentMousePosition;
}

FVector2 FInputManager::GetMouseDelta() const
{
	return MouseDelta;
}

bool FInputManager::IsPrevKeyDown(uint32 Key) const
{
	if (Key >= MAX_KEYS)
	{
		return false;
	}
	return PreviousKeyStates[Key];
}

