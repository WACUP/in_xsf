/*
 * xSF - NCSF configuration
 * By Naram Qashat (CyberBotX) [cyberbotx@cyberbotx.com]
 *
 * Partially based on the vio*sf framework
 */

#include <sstream>
#include <string>
#include <cstddef>
#include "windowsh_wrapper.h"
#include <windowsx.h>
#include "XSFConfig_NCSF.h"
#include "XSFPlayer_NCSF.h"
#include "convert.h"
#ifdef _DEBUG
# include <cstdint>
# include "SSEQPlayer/common.h"
# include "SSEQPlayer/consts.h"
# include "resource.h"
# include <CommCtrl.h>
#endif
#define WA_UTILS_SIMPLE
#define SKIP_SUBCLASS
#include <loader/loader/utils.h>

enum
{
	idInterpolation = 1000,
	idMutes
};

unsigned XSFConfig::initSampleRate = 44100;
const wchar_t* XSFConfig::commonName = L"NCSF Decoder";
const wchar_t* XSFConfig::versionNumber = L"1.13.6";
unsigned XSFConfig_NCSF::initInterpolation = 4;
std::string XSFConfig_NCSF::initMutes = "0000000000000000";

XSFConfig *XSFConfig::Create()
{
	return new XSFConfig_NCSF();
}

XSFConfig_NCSF::XSFConfig_NCSF() : XSFConfig(), interpolation(0)
#ifdef _DEBUG
	, soundViewData()
#endif
{
	this->supportedSampleRates.insert(this->supportedSampleRates.end(), { 8000, 11025, 16000, 22050, 32000, 44100,
																		  48000, 88200, 96000, 176400, 192000 });
}

void XSFConfig_NCSF::LoadSpecificConfig()
{
	this->interpolation = this->configIO->GetValue("Interpolation", XSFConfig_NCSF::initInterpolation);
	std::stringstream mutesSS(this->configIO->GetValue("Mutes", XSFConfig_NCSF::initMutes));
	mutesSS >> this->mutes;
}

void XSFConfig_NCSF::SaveSpecificConfig()
{
	this->configIO->SetValue("Interpolation", this->interpolation);
	this->configIO->SetValue("Mutes", this->mutes.to_string<char>());
}

void XSFConfig_NCSF::GenerateSpecificDialogs()
{
	this->configDialog.AddLabelControl(DialogLabelBuilder(L"Interpolation").WithSize(50, 8).InGroup(L"Output").WithRelativePositionToSibling(RelativePosition::PositionType::FromBottomLeft, Point<short>(0, 10), 2).IsLeftJustified());
	this->configDialog.AddComboBoxControl(DialogComboBoxBuilder().WithSize(110, 14).InGroup(L"Output").WithRelativePositionToSibling(RelativePosition::PositionType::FromTopRight, Point<short>(5, -3)).WithID(idInterpolation).
		IsDropDownList().WithTabStop());
	this->configDialog.AddLabelControl(DialogLabelBuilder(L"Mute").WithSize(50, 8).InGroup(L"Output").WithRelativePositionToSibling(RelativePosition::PositionType::FromBottomLeft, Point<short>(0, 10), 2).IsLeftJustified());
	this->configDialog.AddListBoxControl(DialogListBoxBuilder().WithSize(78, 45).WithExactHeight().InGroup(L"Output").WithRelativePositionToSibling(RelativePosition::PositionType::FromTopRight, Point<short>(5, -3)).WithID(idMutes).
		WithBorder().WithVerticalScrollbar().WithMultipleSelect().WithTabStop());
}

INT_PTR CALLBACK XSFConfig_NCSF::ConfigDialogProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch (uMsg)
	{
		case WM_INITDIALOG:
			// Interpolation
			SendDlgItemMessage(hwndDlg, idInterpolation, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"None"));
			SendDlgItemMessage(hwndDlg, idInterpolation, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Linear"));
			SendDlgItemMessage(hwndDlg, idInterpolation, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"4-point, 3rd-order Legrange"));
			SendDlgItemMessage(hwndDlg, idInterpolation, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"6-point, 5th-order Legrange"));
			SendDlgItemMessage(hwndDlg, idInterpolation, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"16-point Sinc (Nuttall 3-term Window)"));
			SendDlgItemMessage(hwndDlg, idInterpolation, CB_SETCURSEL, this->interpolation, 0);
			// Mutes
			for (std::size_t x = 0, numMutes = this->mutes.size(); x < numMutes; ++x)
			{
				SendDlgItemMessage(hwndDlg, idMutes, LB_ADDSTRING, 0, reinterpret_cast<LPARAM>((L"SPU " + std::to_wstring(x + 1)).c_str()));
				SendDlgItemMessage(hwndDlg, idMutes, LB_SETSEL, this->mutes[x], x);
			}
			break;
		case WM_COMMAND:
			break;
	}

	return XSFConfig::ConfigDialogProc(hwndDlg, uMsg, wParam, lParam);
}

void XSFConfig_NCSF::ResetSpecificConfigDefaults(HWND hwndDlg)
{
	SendDlgItemMessage(hwndDlg, idInterpolation, CB_SETCURSEL, XSFConfig_NCSF::initInterpolation, 0);
	auto tmpMutes = std::bitset<16>(XSFConfig_NCSF::initMutes);
	for (std::size_t x = 0, numMutes = tmpMutes.size(); x < numMutes; ++x)
		SendDlgItemMessage(hwndDlg, idMutes, LB_SETSEL, tmpMutes[x], x);
}

void XSFConfig_NCSF::SaveSpecificConfigDialog(HWND hwndDlg)
{
	this->interpolation = static_cast<unsigned>(SendDlgItemMessage(hwndDlg, idInterpolation, CB_GETCURSEL, 0, 0));
	for (std::size_t x = 0, numMutes = this->mutes.size(); x < numMutes; ++x)
		this->mutes[x] = !!SendDlgItemMessage(hwndDlg, idMutes, LB_GETSEL, x, 0);
}

void XSFConfig_NCSF::CopySpecificConfigToMemory(XSFPlayer *xSFPlayer, bool)
{
	auto NCSFPlayer = static_cast<XSFPlayer_NCSF *>(xSFPlayer);
	NCSFPlayer->SetInterpolation(this->interpolation);
	NCSFPlayer->SetMutes(this->mutes);
}

void XSFConfig_NCSF::About(HWND parent)
{
	AboutMessageBox(parent, (XSFConfig::CommonNameWithVersion() + L"\n\nBuild date: " +
		TEXT(__DATE__) + L"\n\nUsing xSF Winamp plugin framework (based on the vio*sf "
		L"plugins) by Naram Qashat (CyberBotX) [cyberbotx@cyberbotx.com]\n\nUtilizes "
		L"code adapted from the FeOS Sound System library by\nfincs, git revision "
		L"5204c55 on GitHub, for playback.").c_str(), XSFConfig::commonName);
}

#ifdef _DEBUG
INT_PTR CALLBACK XSFConfig_NCSF::SoundViewDialogProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	auto data = reinterpret_cast<SoundViewData *>(GetWindowLonPtrW(hwndDlg, DWLP_USER));
	if (!data && uMsg != WM_INITDIALOG)
		return false;

	switch (uMsg)
	{
		case WM_INITDIALOG:
			if (!data)
			{
				data = reinterpret_cast<SoundViewData *>(lParam);
				SetWindowLongW(hwndDlg, DWLP_USER, reinterpret_cast<LONG>(data));
			}
			data->hDlg = hwndDlg;
			for (int chanId = 0; chanId < 16; ++chanId)
			{
				SendDlgItemMessage(hwndDlg, IDC_SOUND0VOLBAR + chanId, PBM_SETRANGE, 0, MAKELPARAM(0, 128));
				SendDlgItemMessage(hwndDlg, IDC_SOUND0PANBAR + chanId, PBM_SETRANGE, 0, MAKELPARAM(0, 128));
				SendDlgItemMessage(hwndDlg, IDC_SOUND0MUTE + chanId, BM_SETCHECK, data->config->mutes[chanId], 0);
			}
			break;
		case WM_CLOSE:
		case WM_DESTROY:
			data->config->CloseSoundView();
			break;
		case WM_COMMAND:
			{
				auto id = GET_WM_COMMAND_ID(wParam, lParam);
				switch (id)
				{
					case IDC_BUTTON_VOLMODE:
						data->volModeAlternative = !!IsDlgButtonChecked(hwndDlg, IDC_BUTTON_VOLMODE);
						break;
					case IDC_SOUND0MUTE:
					case IDC_SOUND1MUTE:
					case IDC_SOUND2MUTE:
					case IDC_SOUND3MUTE:
					case IDC_SOUND4MUTE:
					case IDC_SOUND5MUTE:
					case IDC_SOUND6MUTE:
					case IDC_SOUND7MUTE:
					case IDC_SOUND8MUTE:
					case IDC_SOUND9MUTE:
					case IDC_SOUND10MUTE:
					case IDC_SOUND11MUTE:
					case IDC_SOUND12MUTE:
					case IDC_SOUND13MUTE:
					case IDC_SOUND14MUTE:
					case IDC_SOUND15MUTE:
						data->config->mutes[id - IDC_SOUND0MUTE] = !!IsDlgButtonChecked(hwndDlg, id);
						data->config->SaveConfig();
						data->player->SetMutes(data->config->mutes);
						break;
					case IDC_SOUND_UNMUTE_ALL:
						for (int chanId = 0; chanId < 16; ++chanId)
							SendDlgItemMessage(hwndDlg, IDC_SOUND0MUTE + chanId, BM_SETCHECK, false, 0);
						data->config->mutes.reset();
						data->config->SaveConfig();
						data->player->SetMutes(data->config->mutes);
						break;
					default:
						return false;
				}
			}
			break;
		default:
			return false;
	}

	return true;
}

void XSFConfig_NCSF::CallSoundView(XSFPlayer *xSFPlayer, HINSTANCE hInstance, HWND hwndParent)
{
	this->soundViewData.reset(new SoundViewData);
	this->soundViewData->config = this;
	this->soundViewData->player = static_cast<XSFPlayer_NCSF *>(xSFPlayer);

	auto hDlg = CreateDialogParamW(hInstance, MAKEINTRESOURCEW(IDD_SOUND_VIEW), hwndParent, XSFConfig_NCSF::SoundViewDialogProc, reinterpret_cast<LPARAM>(this->soundViewData.get()));
	if (!hDlg)
	{
		this->soundViewData.reset();
		return;
	}

	ShowWindow(hDlg, SW_SHOW);
	UpdateWindow(hDlg);
}

static inline void ProgressSetPosImmediate(HWND hDlg, int nIDDlgItem, int nPos)
{
	int nMin = SendDlgItemMessage(hDlg, nIDDlgItem, PBM_GETRANGE, true, reinterpret_cast<LPARAM>(nullptr));
	int nMax = SendDlgItemMessage(hDlg, nIDDlgItem, PBM_GETRANGE, false, reinterpret_cast<LPARAM>(nullptr));

	// get rid of fancy progress animation since Windows Vista
	// http://stackoverflow.com/questions/1061715/how-do-i-make-tprogressbar-stop-lagging
	if (nPos < nMax)
	{
		SendDlgItemMessage(hDlg, nIDDlgItem, PBM_SETPOS, nPos + 1, 0);
		SendDlgItemMessage(hDlg, nIDDlgItem, PBM_SETPOS, nPos, 0); // This will set Progress backwards and give an instant update
	}
	else
	{
		SendDlgItemMessage(hDlg, nIDDlgItem, PBM_SETRANGE32, nMin, nPos + 1);
		SendDlgItemMessage(hDlg, nIDDlgItem, PBM_SETPOS, nPos + 1, 0);
		SendDlgItemMessage(hDlg, nIDDlgItem, PBM_SETRANGE32, nMin, nPos); // This will also set Progress backwards also so instant update
	}
}

static inline std::int32_t muldiv7(std::int32_t val, std::uint8_t mul)
{
	return mul == 127 ? val : ((val * mul) >> 7);
}

void XSFConfig_NCSF::RefreshSoundView()
{
	auto player = this->soundViewData->player;
	auto hDlg = this->soundViewData->hDlg;
	std::wstring buf;
	for (std::size_t chanId = 0; chanId < 16; ++chanId)
	{
		const auto &chn = player->GetChannel(chanId);

		if (chn.state > ChannelState::Start)
		{
			ProgressSetPosImmediate(hDlg, IDC_SOUND0PANBAR + chanId, muldiv7(128, chn.reg.panning));
			std::uint8_t datashift = chn.reg.volumeDiv;
			if (datashift == 3)
				datashift = 4;
			std::int32_t vol = muldiv7(128, chn.reg.volumeMul) >> datashift;
			ProgressSetPosImmediate(hDlg, IDC_SOUND0VOLBAR + chanId, vol);

			if (this->soundViewData->volModeAlternative)
				buf = std::to_wstring(chn.reg.volumeMul) + L"/" + std::to_wstring(1 << datashift);
			else
				buf = std::to_wstring(vol);
			SetDlgItemText(hDlg, IDC_SOUND0VOL + chanId, buf.c_str());

			if (!chn.reg.panning)
				buf = L"L";
			else if (chn.reg.panning == 64)
				buf = L"C";
			else if (chn.reg.panning == 127)
				buf = L"R";
			else if (chn.reg.panning < 64)
				buf = L"L" + std::to_wstring(64 - chn.reg.panning);
			else //if (chn.reg.panning > 64)
				buf = L"R" + std::to_wstring(chn.reg.panning - 64);
			SetDlgItemText(hDlg, IDC_SOUND0PAN + chanId, buf.c_str());

			static const std::wstring modes[] = { L"Manual", L"Loop Infinite", L"One-Shot", L"Prohibited" };
			SetDlgItemText(hDlg, IDC_SOUND0REPEATMODE + chanId, (std::to_wstring(chn.reg.repeatMode) + L" (" + modes[chn.reg.repeatMode] + L")").c_str());

			if (chn.reg.format != 3)
			{
				static const std::wstring formats[] = { L"PCM8", L"PCM16", L"IMA-ADPCM" };
				SetDlgItemText(hDlg, IDC_SOUND0FORMAT + chanId, (std::to_wstring(chn.reg.format) + L" (" + formats[chn.reg.format] + L")").c_str());
			}
			else
			{
				buf = L"3 (";
				if (chanId < 8)
					buf += L"PSG/Noise?";
				else if (chanId < 14)
					buf += ConvertFuncs::TrimDoubleString(std::to_wstring(chn.reg.waveDuty == 7 ? 0 : 12.5 * (chn.reg.waveDuty + 1))) + L"% Square";
				else
					buf += L"Noise";
				buf += L")";
				SetDlgItemText(hDlg, IDC_SOUND0FORMAT + chanId, buf.c_str());
			}

			static const std::wstring states[] = { L"NONE", L"START", L"ATTACK", L"DECAY", L"SUSTAIN", L"RELEASE" };
			SetDlgItemText(hDlg, IDC_SOUND0STATE + chanId, states[ToIntegral(chn.state)].c_str());

			SetDlgItemText(hDlg, IDC_SOUND0PNT + chanId, (L"samp #" + std::to_wstring(chn.reg.loopStart)).c_str());

			std::wstring tmpBuf = ConvertFuncs::StringToWString(NumToHexString(chn.reg.timer)).substr(2);
			buf = L"$" + tmpBuf + L" (";
			tmpBuf = ConvertFuncs::TrimDoubleString(std::to_wstring((ARM7_CLOCK / 2) / static_cast<double>(0x10000 - chn.reg.timer) / 8));
			if (tmpBuf.find('.') != std::wstring::npos)
				tmpBuf = tmpBuf.substr(0, tmpBuf.find('.') + 2);
			buf += tmpBuf + L" Hz)";
			SetDlgItemText(hDlg, IDC_SOUND0TMR + chanId, buf.c_str());

			SetDlgItemText(hDlg, IDC_SOUND0POSLEN + chanId, (L"samp #" + std::to_wstring(static_cast<std::uint32_t>(chn.reg.samplePosition)) + L" / " + std::to_wstring(chn.reg.totalLength)).c_str());
		}
		else if (this->soundViewData->channelLastStates[chanId] != ChannelState::None)
		{
			ProgressSetPosImmediate(hDlg, IDC_SOUND0PANBAR + chanId, 0);
			ProgressSetPosImmediate(hDlg, IDC_SOUND0VOLBAR + chanId, 0);
			SetDlgItemText(hDlg, IDC_SOUND0VOL + chanId, L"---");
			SetDlgItemText(hDlg, IDC_SOUND0PAN + chanId, L"---");
			SetDlgItemText(hDlg, IDC_SOUND0REPEATMODE + chanId, L"---");
			SetDlgItemText(hDlg, IDC_SOUND0FORMAT + chanId, L"---");
			SetDlgItemText(hDlg, IDC_SOUND0STATE + chanId, L"NONE");
			SetDlgItemText(hDlg, IDC_SOUND0PNT + chanId, L"---");
			SetDlgItemText(hDlg, IDC_SOUND0TMR + chanId, L"---");
			SetDlgItemText(hDlg, IDC_SOUND0POSLEN + chanId, L"---");
		}

		this->soundViewData->channelLastStates[chanId] = chn.state;
	}
}

void XSFConfig_NCSF::CloseSoundView()
{
	if (this->soundViewData)
	{
		DestroyWindow(this->soundViewData->hDlg);
		this->soundViewData.reset();
	}
}
#endif
