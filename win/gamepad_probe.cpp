//   Copyright 2013 Justin TerAvest
//
//   Licensed under the Apache License, Version 2.0 (the "License");
//   you may not use this file except in compliance with the License.
//   You may obtain a copy of the License at
//
//       http://www.apache.org/licenses/LICENSE-2.0
//
//   Unless required by applicable law or agreed to in writing, software
//   distributed under the License is distributed on an "AS IS" BASIS,
//   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
//   See the License for the specific language governing permissions and
//   limitations under the License.

#include "stdafx.h"
#include <dinput.h>
#include <dinputd.h>

#ifndef DIDFT_OPTIONAL
#define DIDFT_OPTIONAL 0x80000000
#endif

struct Context {
  IDirectInput8* d_interface;
  IDirectInputDevice8* device;
  bool valid;
};

// Sets the deadzone value for all axes of a gamepad.
// deadzone values range from 0 (no deadzone) to 10,000 (entire range
// is dead).
bool SetDirectInputDeadZone(IDirectInputDevice8* gamepad,
                            int deadzone) {
  DIPROPDWORD prop;
  prop.diph.dwSize = sizeof(DIPROPDWORD);
  prop.diph.dwHeaderSize = sizeof(DIPROPHEADER);
  prop.diph.dwObj = 0;
  prop.diph.dwHow = DIPH_DEVICE;
  prop.dwData = deadzone;
  return SUCCEEDED(gamepad->SetProperty(DIPROP_DEADZONE, &prop.diph));
}


// We define our own data format structure to attempt to get as many
// axes as possible.
struct JoyData {
  long axes[10];
  char buttons[24];
  DWORD pov;  // Often used for D-pads.
};


bool GetVendorProduct(Context* ctxt) {
  DIPROPDWORD prop;
  prop.diph.dwSize = sizeof(DIPROPDWORD);
  prop.diph.dwHeaderSize = sizeof(DIPROPHEADER);
  prop.diph.dwObj = 0;
  prop.diph.dwHow = DIPH_DEVICE;

  if (FAILED(ctxt->device->GetProperty(DIPROP_VIDPID, &prop.diph)))
    return false;
  printf("Vendor: %04x Product: %04x\n", LOWORD(prop.dwData), HIWORD(prop.dwData));
  return true;
}

BOOL CALLBACK EnumCallback(const DIDEVICEINSTANCE* instance,
                  void* context) {
  Context* ctxt = reinterpret_cast<Context*>(context);
  if (FAILED(ctxt->d_interface->CreateDevice(instance->guidInstance,
                                           &ctxt->device,
                                           NULL)))
    return DIENUM_CONTINUE;
  ctxt->device->Acquire();

#define MAKE_AXIS(i) \
  {0, FIELD_OFFSET(JoyData, axes) + 4 * i, \
   DIDFT_AXIS | DIDFT_MAKEINSTANCE(i) | DIDFT_OPTIONAL, 0}
#define MAKE_BUTTON(i) \
  {&GUID_Button, FIELD_OFFSET(JoyData, buttons) + i, \
   DIDFT_BUTTON | DIDFT_MAKEINSTANCE(i) | DIDFT_OPTIONAL, 0}
#define MAKE_POV() \
  {&GUID_POV, FIELD_OFFSET(JoyData, pov), DIDFT_POV | DIDFT_OPTIONAL, 0}
  DIOBJECTDATAFORMAT rgodf[] = {
    MAKE_AXIS(0),
    MAKE_AXIS(1),
    MAKE_AXIS(2),
    MAKE_AXIS(3),
    MAKE_AXIS(4),
    MAKE_AXIS(5),
    MAKE_AXIS(6),
    MAKE_AXIS(7),
    MAKE_AXIS(8),
    MAKE_AXIS(9),
    MAKE_BUTTON(0),
    MAKE_BUTTON(1),
    MAKE_BUTTON(2),
    MAKE_BUTTON(3),
    MAKE_BUTTON(4),
    MAKE_BUTTON(5),
    MAKE_BUTTON(6),
    MAKE_BUTTON(7),
    MAKE_BUTTON(8),
    MAKE_BUTTON(9),
    MAKE_BUTTON(10),
    MAKE_BUTTON(11),
    MAKE_BUTTON(12),
    MAKE_BUTTON(13),
    MAKE_BUTTON(14),
    MAKE_BUTTON(15),
    MAKE_BUTTON(16),
    MAKE_POV(),
  };
#undef MAKE_AXIS
#undef MAKE_BUTTON
#undef MAKE_POV

  DIDATAFORMAT df = {
    sizeof (DIDATAFORMAT),
    sizeof (DIOBJECTDATAFORMAT),
    DIDF_ABSAXIS,
    sizeof (JoyData),
    sizeof (rgodf) / sizeof (rgodf[0]),
    rgodf
  };
  
  // If we can't set the data format on the device, don't add it to our
  // list, since we won't know how to read data from it.
  if (FAILED(ctxt->device->SetDataFormat(&df))) {
    ctxt->device->Release();
    return DIENUM_CONTINUE;
  }
  printf("Product name: %ls\n", instance->tszProductName);
  if (!GetVendorProduct(ctxt))
	return DIENUM_CONTINUE;

  // Dead zone at 10%.
  SetDirectInputDeadZone(ctxt->device, 1000);

  // A hack to disallow "zero" values from our axis.
  // This allows us to tell the difference between an axis that isn't
  // present, and one that is at its minimum value.
  DIPROPRANGE range;
  range.diph.dwSize = sizeof(DIPROPRANGE);
  range.diph.dwHeaderSize = sizeof(DIPROPHEADER);
  range.diph.dwHow = DIPH_DEVICE;
  range.diph.dwObj = 0;
  range.lMin = 1;
  range.lMax = 65535;
  if (SUCCEEDED(ctxt->device->SetProperty(DIPROP_RANGE, &range.diph)))
    ctxt->valid = true;
  return DIENUM_STOP;
}

bool PollGamepad(Context* ctxt, char buf[80]) {
  JoyData state;
  if (FAILED(ctxt->device->Poll())) {
    fprintf(stderr, "Couldn't poll device\n");
    exit(2);
  }
  if (FAILED(ctxt->device->GetDeviceState(sizeof(JoyData), &state))) {
	fprintf(stderr, "Couldn't get device state\n");
	exit(3);
  }
  for (int i = 0; i < 17; i++) {
    if (state.buttons[i] & 0x80) {
	  sprintf_s(buf, 80, "directinput button %d", i);
	  return true;
	}
  }
  if (state.pov != -1) {
	sprintf_s(buf, 80, "pov %d", state.pov);
	return true;
  }
  for (int i = 0; i < 10; i++) {
    if (state.axes[i] == 0)
	  continue;
	if (state.axes[i] < 30000) {
	  sprintf_s(buf, 80, "axis %d negative (%d)", i, state.axes[i]);
	  return true;
	} else if (state.axes[i] > 37000) {
	  sprintf_s(buf, 80, "axis %d positive (%d)", i, state.axes[i]);
	  return true;
	}
  }
  // Or see if the enter button is hit.
  if (GetKeyState(VK_RETURN) < 0) {
    sprintf_s(buf, 80, "(not present)");
	return true;
  }
  return false;
}

void GetButton(Context* ctxt, int id) {
  printf("Press button %d:\n", id);
  bool buttonPressed = false;
  while (!buttonPressed) {
	char buf[80];
	buttonPressed = PollGamepad(ctxt, buf);
	if (buttonPressed) {
	  printf("button %d = %s\n", id, buf);
	  break;
	}
	Sleep(50); // Sleep 50ms between polls.
  }
}

void GetAxis(Context* ctxt, const char* str, int id) {
  printf("Press axis %d %s:\n", id, str);
  bool buttonPressed = false;
  while (!buttonPressed) {
	char buf[80];
	buttonPressed = PollGamepad(ctxt, buf);
	if (buttonPressed) {
      printf("axis %d %s = %s\n", id, str, buf);
	  break;
	}
	Sleep(50);
  }
}

void WaitForNeutralGamepad(Context* ctxt) {
  bool neutral = false;
  char buf[80];
  while (PollGamepad(ctxt, buf))
    Sleep(50);
}

int _tmain(int argc, _TCHAR* argv[])
{
  printf("Open:\n");
  printf("https://dvcs.w3.org/hg/gamepad/raw-file/default/gamepad.html#remapping\n");
  printf("  for an image with axis and button numbers.\n");
  printf("Hit ENTER if a given button or axis does not exist on your gamepad.\n\n");
  Context ctxt;
  ctxt.valid = false;
  if (!SUCCEEDED(DirectInput8Create(
      GetModuleHandle(NULL),
      DIRECTINPUT_VERSION,
      IID_IDirectInput8,
      reinterpret_cast<void**>(&ctxt.d_interface),
      NULL))) {
    fprintf(stderr, "Could not create DirectInput interface.");
    return 1;
  }
  ctxt.d_interface->EnumDevices(
      DI8DEVCLASS_GAMECTRL,
      &EnumCallback,
      &ctxt,
      DIEDFL_ATTACHEDONLY);
  if (!ctxt.valid) {
    fprintf(stderr, "No DirectInput devices found!\n");
	fprintf(stderr, "Press ENTER to exit.\n");
	while (GetKeyState(VK_RETURN) >= 0)  // Wait for enter to be pressed.
      Sleep(50);
	exit(1);
  }

  ctxt.device->Acquire();
  for (int i = 0; i < 17; i++) {
	GetButton(&ctxt, i);
	WaitForNeutralGamepad(&ctxt);
  }
  for (int i = 0; i < 2; i++) {
    GetAxis(&ctxt, "up", i);
	WaitForNeutralGamepad(&ctxt);
	GetAxis(&ctxt, "left", i);
	WaitForNeutralGamepad(&ctxt);
  }

  printf("Thanks! Please file a bug at http://crbug.com with your gamepad information.\n");
  printf("Press ENTER to exit.\n");
  while (GetKeyState(VK_RETURN) >= 0)  // Wait for enter to be pressed.
    Sleep(50);
}


