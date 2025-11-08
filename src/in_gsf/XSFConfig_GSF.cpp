/*
 * xSF - GSF configuration
 * By Naram Qashat (CyberBotX) [cyberbotx@cyberbotx.com]
 *
 * Partially based on the vio*sf framework
 */

#include <sstream>
#include <string>
#include "windowsh_wrapper.h"
#include "XSFConfig.h"
#include "convert.h"
#include "vbam/gba/Sound.h"
#define WA_UTILS_SIMPLE
#define SKIP_SUBCLASS
#include <loader/loader/utils.h>

class XSFPlayer;

enum
{
	idLowPassFiltering = 1000,
	idMutes
};

#pragma pack(1)
class XSFConfig_GSF : public XSFConfig
{
protected:
	static bool initLowPassFiltering;
	static std::string initMutes;

	friend class XSFConfig;
	bool lowPassFiltering;
	std::bitset<6> mutes;

	XSFConfig_GSF();
	void LoadSpecificConfig();
	void SaveSpecificConfig();
	void GenerateSpecificDialogs();
	INT_PTR CALLBACK ConfigDialogProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam);
	void ResetSpecificConfigDefaults(HWND hwndDlg);
	void SaveSpecificConfigDialog(HWND hwndDlg);
	void CopySpecificConfigToMemory(XSFPlayer *xSFPlayer, bool preLoad);
public:
	void About(HWND parent);
};
#pragma pack()

unsigned XSFConfig::initSampleRate = 44100;
const wchar_t* XSFConfig::commonName = L"GSF Decoder";
const wchar_t* XSFConfig::versionNumber = L"1.0.11";
bool XSFConfig_GSF::initLowPassFiltering = true;
std::string XSFConfig_GSF::initMutes = "000000";

XSFConfig *XSFConfig::Create()
{
	return new XSFConfig_GSF();
}

XSFConfig_GSF::XSFConfig_GSF() : XSFConfig(), lowPassFiltering(false), mutes()
{
	this->supportedSampleRates.insert(this->supportedSampleRates.end(), { 8000, 11025, 16000, 22050, 32000, 44100,
																		  48000, 88200, 96000, 176400, 192000 });
}

void XSFConfig_GSF::LoadSpecificConfig()
{
	this->lowPassFiltering = this->configIO->GetValue("LowPassFiltering", XSFConfig_GSF::initLowPassFiltering);
	std::stringstream mutesSS(this->configIO->GetValue("Mutes", XSFConfig_GSF::initMutes));
	mutesSS >> this->mutes;
}

void XSFConfig_GSF::SaveSpecificConfig()
{
	this->configIO->SetValue("LowPassFiltering", this->lowPassFiltering);
	this->configIO->SetValue("Mutes", this->mutes.to_string<char>());
}

void XSFConfig_GSF::GenerateSpecificDialogs()
{
	this->configDialog.AddCheckBoxControl(DialogCheckBoxBuilder(L"Low-Pass Filtering").WithSize(80, 10).InGroup(L"Output").WithRelativePositionToSibling(RelativePosition::PositionType::FromBottomLeft, Point<short>(0, 7), 2).WithTabStop().
		WithID(idLowPassFiltering));
	this->configDialog.AddLabelControl(DialogLabelBuilder(L"Mute").WithSize(50, 8).InGroup(L"Output").WithRelativePositionToSibling(RelativePosition::PositionType::FromBottomLeft, Point<short>(0, 10)).IsLeftJustified());
	this->configDialog.AddListBoxControl(DialogListBoxBuilder().WithSize(78, 45).WithExactHeight().InGroup(L"Output").WithRelativePositionToSibling(RelativePosition::PositionType::FromTopRight, Point<short>(5, -3)).WithID(idMutes).WithBorder().
		WithVerticalScrollbar().WithMultipleSelect().WithTabStop());
}

INT_PTR CALLBACK XSFConfig_GSF::ConfigDialogProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch (uMsg)
	{
		case WM_INITDIALOG:
			// Low-Pass Filtering
			if (this->lowPassFiltering)
				SendDlgItemMessage(hwndDlg, idLowPassFiltering, BM_SETCHECK, BST_CHECKED, 0);
			// Mutes
			SendDlgItemMessage(hwndDlg, idMutes, LB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Square 1"));
			SendDlgItemMessage(hwndDlg, idMutes, LB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Square 2"));
			SendDlgItemMessage(hwndDlg, idMutes, LB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Wave Pattern"));
			SendDlgItemMessage(hwndDlg, idMutes, LB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Noise"));
			SendDlgItemMessage(hwndDlg, idMutes, LB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"PCM A"));
			SendDlgItemMessage(hwndDlg, idMutes, LB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"PCM B"));
			for (size_t x = 0, numMutes = this->mutes.size(); x < numMutes; ++x)
				SendDlgItemMessage(hwndDlg, idMutes, LB_SETSEL, this->mutes[x], x);
			break;
		case WM_COMMAND:
			break;
	}

	return XSFConfig::ConfigDialogProc(hwndDlg, uMsg, wParam, lParam);
}

void XSFConfig_GSF::ResetSpecificConfigDefaults(HWND hwndDlg)
{
	SendDlgItemMessage(hwndDlg, idLowPassFiltering, BM_SETCHECK, XSFConfig_GSF::initLowPassFiltering ? BST_CHECKED : BST_UNCHECKED, 0);
	auto tmpMutes = std::bitset<6>(XSFConfig_GSF::initMutes);
	for (size_t x = 0, numMutes = tmpMutes.size(); x < numMutes; ++x)
		SendDlgItemMessage(hwndDlg, idMutes, LB_SETSEL, tmpMutes[x], x);
}

void XSFConfig_GSF::SaveSpecificConfigDialog(HWND hwndDlg)
{
	this->lowPassFiltering = SendDlgItemMessage(hwndDlg, idLowPassFiltering, BM_GETCHECK, 0, 0) == BST_CHECKED;
	for (size_t x = 0, numMutes = this->mutes.size(); x < numMutes; ++x)
		this->mutes[x] = !!SendDlgItemMessage(hwndDlg, idMutes, LB_GETSEL, x, 0);
}

void XSFConfig_GSF::CopySpecificConfigToMemory(XSFPlayer *, bool preLoad)
{
	if (!preLoad)
	{
		soundInterpolation = this->lowPassFiltering;
		unsigned long tmpMutes = this->mutes.to_ulong();
		soundSetEnable((((tmpMutes & 0x30) << 4) | (tmpMutes & 0xF)) ^ 0x30F);
	}
}

void XSFConfig_GSF::About(HWND parent)
{
	AboutMessageBox(parent, (XSFConfig::CommonNameWithVersion() + L"\n\nBuild date: " +
		TEXT(__DATE__) + L"\n\nUsing xSF Winamp plugin framework (based on the vio*sf "
		L"plugins) by Naram Qashat (CyberBotX) [cyberbotx@cyberbotx.com]\n\nUtilizes "
		L"modified VBA-M, SVN revision 1231 for playback.").c_str(), XSFConfig::commonName);
}
