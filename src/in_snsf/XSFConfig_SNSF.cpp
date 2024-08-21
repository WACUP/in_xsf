/*
 * xSF - SNSF configuration
 * By Naram Qashat (CyberBotX) [cyberbotx@cyberbotx.com]
 *
 * Partially based on the vio*sf framework
 *
 * NOTE: 16-bit sound is always enabled, the player is currently limited to
 * creating a 16-bit PCM for Winamp and can not handle 8-bit sound from
 * snes9x.
 */

#include <bitset>
#include <sstream>
#include <string>
#include <cstdint>
#include "windowsh_wrapper.h"
#include "XSFConfig_SNSF.h"
#include "convert.h"
#include "snes9x/apu/apu.h"
#define WA_UTILS_SIMPLE
#include <loader/loader/utils.h>

enum
{
	idSixteenBitSound = 1000,
	idReverseStereo,
	idResampler,
	idMutes
};

unsigned XSFConfig::initSampleRate = 44100;
const std::wstring XSFConfig::commonName = L"SNSF Decoder";
const std::wstring XSFConfig::versionNumber = L"1.0.8";
//bool XSFConfig_SNSF::initSixteenBitSound = true;
bool XSFConfig_SNSF::initReverseStereo = false;
unsigned XSFConfig_SNSF::initResampler = 1;
std::string XSFConfig_SNSF::initMutes = "00000000";

XSFConfig *XSFConfig::Create()
{
	return new XSFConfig_SNSF();
}

XSFConfig_SNSF::XSFConfig_SNSF() : XSFConfig(), /*sixteenBitSound(false), */reverseStereo(false), mutes(), resampler(0)
{
	this->supportedSampleRates.insert(this->supportedSampleRates.end(), { 8000, 11025, 16000, 22050, 32000, 44100,
																		  48000, 88200, 96000, 176400, 192000 });
}

void XSFConfig_SNSF::LoadSpecificConfig()
{
	//this->sixteenBitSound = this->configIO->GetValue("SixteenBitSound", XSFConfig_SNSF::initSixteenBitSound);
	this->reverseStereo = this->configIO->GetValue("ReverseStereo", XSFConfig_SNSF::initReverseStereo);
	this->resampler = this->configIO->GetValue("Resampler", XSFConfig_SNSF::initResampler);
	std::stringstream mutesSS(this->configIO->GetValue("Mutes", XSFConfig_SNSF::initMutes));
	mutesSS >> this->mutes;
}

void XSFConfig_SNSF::SaveSpecificConfig()
{
	//this->configIO->SetValue("SixteenBitSound", this->sixteenBitSound);
	this->configIO->SetValue("ReverseStereo", this->reverseStereo);
	this->configIO->SetValue("Resampler", this->resampler);
	this->configIO->SetValue("Mutes", this->mutes.to_string<char>());
}

void XSFConfig_SNSF::GenerateSpecificDialogs()
{
	/*this->configDialog.AddCheckBoxControl(DialogCheckBoxBuilder(L"Sixteen-Bit Sound").WithSize(80, 10).InGroup(L"Output").WithRelativePositionToSibling(RelativePosition::PositionType::FromBottomLeft, Point<short>(0, 7), 2).WithTabStop().
		WithID(idSixteenBitSound));*/
	this->configDialog.AddCheckBoxControl(DialogCheckBoxBuilder(L"Reverse Stereo").WithSize(80, 10).InGroup(L"Output").WithRelativePositionToSibling(RelativePosition::PositionType::FromBottomLeft, Point<short>(0, 7), 2).WithTabStop().
		WithID(idReverseStereo));
	this->configDialog.AddLabelControl(DialogLabelBuilder(L"Resampler").WithSize(50, 8).InGroup(L"Output").WithRelativePositionToSibling(RelativePosition::PositionType::FromBottomLeft, Point<short>(0, 10)).IsLeftJustified());
	this->configDialog.AddComboBoxControl(DialogComboBoxBuilder().WithSize(78, 14).InGroup(L"Output").WithRelativePositionToSibling(RelativePosition::PositionType::FromTopRight, Point<short>(5, -3)).WithTabStop().IsDropDownList().
		WithID(idResampler));
	this->configDialog.AddLabelControl(DialogLabelBuilder(L"Mute").WithSize(50, 8).InGroup(L"Output").WithRelativePositionToSibling(RelativePosition::PositionType::FromBottomLeft, Point<short>(0, 10), 2).IsLeftJustified());
	this->configDialog.AddListBoxControl(DialogListBoxBuilder().WithSize(78, 45).WithExactHeight().InGroup(L"Output").WithRelativePositionToSibling(RelativePosition::PositionType::FromTopRight, Point<short>(5, -3)).WithID(idMutes).WithBorder().
		WithVerticalScrollbar().WithMultipleSelect().WithTabStop());
}

INT_PTR CALLBACK XSFConfig_SNSF::ConfigDialogProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch (uMsg)
	{
		case WM_INITDIALOG:
			// Sixteen-Bit Sound
			/*if (this->sixteenBitSound)
				SendDlgItemMessage(hwndDlg, idSixteenBitSound, BM_SETCHECK, BST_CHECKED, 0);*/
			// Reverse Stereo
			if (this->reverseStereo)
				SendDlgItemMessage(hwndDlg, idReverseStereo, BM_SETCHECK, BST_CHECKED, 0);
			// Resampler
			SendDlgItemMessage(hwndDlg, idResampler, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Linear Resampler"));
			SendDlgItemMessage(hwndDlg, idResampler, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Hermite Resampler"));
			SendDlgItemMessage(hwndDlg, idResampler, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Bspline Resampler"));
			SendDlgItemMessage(hwndDlg, idResampler, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Osculating Resampler"));
			SendDlgItemMessage(hwndDlg, idResampler, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Sinc Resampler"));
			SendDlgItemMessage(hwndDlg, idResampler, CB_SETCURSEL, this->resampler, 0);
			// Mutes
			for (size_t x = 0, numMutes = this->mutes.size(); x < numMutes; ++x)
			{
				SendDlgItemMessage(hwndDlg, idMutes, LB_ADDSTRING, 0, reinterpret_cast<LPARAM>((L"BRRPCM " + std::to_wstring(x + 1)).c_str()));
				SendDlgItemMessage(hwndDlg, idMutes, LB_SETSEL, this->mutes[x], x);
			}
			break;
		case WM_COMMAND:
			break;
	}

	return XSFConfig::ConfigDialogProc(hwndDlg, uMsg, wParam, lParam);
}

void XSFConfig_SNSF::ResetSpecificConfigDefaults(HWND hwndDlg)
{
	//SendDlgItemMessage(hwndDlg, idSixteenBitSound, BM_SETCHECK, XSFConfig_SNSF::initSixteenBitSound ? BST_CHECKED : BST_UNCHECKED, 0);
	SendDlgItemMessage(hwndDlg, idReverseStereo, BM_SETCHECK, XSFConfig_SNSF::initReverseStereo ? BST_CHECKED : BST_UNCHECKED, 0);
	SendDlgItemMessage(hwndDlg, idResampler, CB_SETCURSEL, XSFConfig_SNSF::initResampler, 0);
	auto tmpMutes = std::bitset<8>(XSFConfig_SNSF::initMutes);
	for (size_t x = 0, numMutes = tmpMutes.size(); x < numMutes; ++x)
		SendDlgItemMessage(hwndDlg, idMutes, LB_SETSEL, tmpMutes[x], x);
}

void XSFConfig_SNSF::SaveSpecificConfigDialog(HWND hwndDlg)
{
	//this->sixteenBitSound = SendDlgItemMessage(hwndDlg, idSixteenBitSound, BM_GETCHECK, 0, 0) == BST_CHECKED;
	this->reverseStereo = SendDlgItemMessage(hwndDlg, idReverseStereo, BM_GETCHECK, 0, 0) == BST_CHECKED;
	this->resampler = static_cast<unsigned>(SendDlgItemMessage(hwndDlg, idResampler, CB_GETCURSEL, 0, 0));
	for (size_t x = 0, numMutes = this->mutes.size(); x < numMutes; ++x)
		this->mutes[x] = !!SendDlgItemMessage(hwndDlg, idMutes, LB_GETSEL, x, 0);
}

void XSFConfig_SNSF::CopySpecificConfigToMemory(XSFPlayer *, bool preLoad)
{
	if (preLoad)
	{
		memset(&Settings, 0, sizeof(Settings));
		//Settings.SixteenBitSound = this->sixteenBitSound;
		//Settings.ReverseStereo = this->reverseStereo;
	}
	else
		S9xSetSoundControl(static_cast<std::uint8_t>(this->mutes.to_ulong()) ^ 0xFF);
}

void XSFConfig_SNSF::About(HWND parent)
{
	AboutMessageBox(parent, (XSFConfig::CommonNameWithVersion() + L"\n\nBuild date: " +
		TEXT(__DATE__) + L"\n\nUsing xSF Winamp plugin framework (based on the vio*sf "
		L"plugins) by Naram Qashat (CyberBotX) [cyberbotx@cyberbotx.com]\n\nUtilizes "
		L"modified snes9x v1.60 for playback.").c_str(), XSFConfig::commonName.c_str());
}
