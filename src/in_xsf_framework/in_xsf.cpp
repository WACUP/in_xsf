/*
 * xSF - Winamp plugin
 * By Naram Qashat (CyberBotX) [cyberbotx@cyberbotx.com]
 *
 * Partially based on the vio*sf framework
 */

#include <algorithm>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>
#include <cstddef>
#include <cstdint>
#include "windowsh_wrapper.h"
#include "XSFCommon.h"
#include "XSFConfig.h"
#include "XSFFile.h"
#include "XSFPlayer.h"
#include "convert.h"
#include <winamp/in2.h>
#include <winamp/wa_ipc.h>
#define WA_UTILS_SIMPLE
#define SKIP_SUBCLASS
#include <loader/loader/utils.h>
#include <loader/loader/runtime_helper.h>
#include <api/memmgr/api_memmgr.h>
#include <agave/config/api_config.h>

extern In_Module inMod;
static const XSFFile *xSFFile = nullptr;
/*XSFFile *xSFFileInInfo = nullptr;*/
static XSFPlayer *xSFPlayer = nullptr;
XSFConfig *xSFConfig = nullptr;
static bool paused;
static int seek_needed;
static int decode_pos_ms;
static HANDLE thread_handle = INVALID_HANDLE_VALUE;
static bool killThread = false;

static const unsigned NumChannels = 2;
static const unsigned BitsPerSample = 16;

DWORD WINAPI playThread(void *b)
{
	bool done = false;
	while (!*static_cast<bool *>(b))
	{
		const auto buffer_size = (576 * NumChannels * (BitsPerSample / 8));
		if (seek_needed != -1)
		{
			decode_pos_ms = seek_needed - (seek_needed % 1000);
			seek_needed = -1;
			if (xSFPlayer)
			{
				auto dummyBuffer = std::vector<std::uint8_t>(buffer_size);
				xSFPlayer->Seek(decode_pos_ms, nullptr, dummyBuffer, inMod.outMod);
			}
		}

		if (done)
		{
			inMod.outMod->CanWrite();
			if (!inMod.outMod->IsPlaying())
			{
				PostEOF();
				return 0;
			}
			Sleep(10);
		}
		else if (static_cast<unsigned>(inMod.outMod->CanWrite()) >= (buffer_size << (inMod.dsp_isactive() ? 1 : 0)))
		{
			auto sampleBuffer = std::vector<std::uint8_t>(buffer_size);
			unsigned samplesWritten = 0;
			done = (xSFPlayer ? xSFPlayer->FillBuffer(sampleBuffer, samplesWritten) : true);
			if (samplesWritten)
			{
				const int sampleRate = (xSFPlayer ? xSFPlayer->GetSampleRate() : 0);
				if (!sampleRate)
				{
					done = true;
					continue;
				}

				inMod.SAAddPCMData(reinterpret_cast<char *>(&sampleBuffer[0]), NumChannels, BitsPerSample, decode_pos_ms);
				/*inMod.VSAAddPCMData(reinterpret_cast<char *>(&sampleBuffer[0]), NumChannels, BitsPerSample, decode_pos_ms);*/
				if (inMod.dsp_isactive())
					samplesWritten = inMod.dsp_dosamples(reinterpret_cast<short *>(&sampleBuffer[0]), samplesWritten, BitsPerSample, NumChannels, sampleRate);
				decode_pos_ms += static_cast<int>(samplesWritten * 1000.0 / sampleRate);
				inMod.outMod->Write(reinterpret_cast<char *>(&sampleBuffer[0]), samplesWritten * NumChannels * (BitsPerSample / 8));
			}
		}
		else
			Sleep(10);
	}
	return 0;
}

void config(HWND hwndParent)
{
	if (xSFConfig)
	{
		xSFConfig->InitConfig();
		xSFConfig->CallConfigDialog(inMod.hDllInstance, hwndParent);

		if (xSFPlayer)
		{
			xSFConfig->CopyConfigToMemory(xSFPlayer, false);
		}
	}
}

void about(HWND hwndParent)
{
	if (xSFConfig)
		xSFConfig->About(hwndParent);
}

int init()
{
	xSFConfig = XSFConfig::Create();
	if (xSFConfig)
	{
		inMod.description = (char*)XSFConfig::CommonNameWithVersion().c_str();

		// changed from doing this to delaying until it's
		// needed to minimise the impact on loading times
		//xSFConfig->LoadConfig();
		//xSFConfig->GenerateDialogs();
		//xSFConfig->SetHInstance(inMod.hDllInstance);
		return IN_INIT_SUCCESS;
	}
	return IN_INIT_FAILURE;
}

void quit()
{
	if (xSFPlayer)
	{
		delete xSFPlayer;
		xSFPlayer = nullptr;
	}

	if (xSFConfig)
	{
		delete xSFConfig;
		xSFConfig = nullptr;
	}
}

void getFileInfo(const in_char *file, in_char *title, int *length_in_ms)
{
	const XSFFile *xSF;
	bool toFree = false;
	if (!file || !*file)
		xSF = xSFFile;
	else
	{
		try
		{
			xSF = new XSFFile(file);
		}
		catch (const std::exception &)
		{
			if (title)
				CopyToString("", title);
			if (length_in_ms)
				*length_in_ms = -1000;
			return;
		}
		toFree = true;
	}

	if (xSF)
	{
		if (title)
			CopyToString(xSF->GetFormattedTitle("%game%[ - [%disc%.]%track%] - %title%"/*XSFConfig::initTitleFormat/*/
								   /*xSFConfig->GetTitleFormat()/**/).substr(0, GETFILEINFO_TITLE_LENGTH - 1), title);
		if (length_in_ms)
			*length_in_ms = xSF->GetLengthMS(xSFConfig->GetDefaultLength()) + xSF->GetFadeMS(xSFConfig->GetDefaultFade());
		if (toFree)
			delete xSF;
	}
	else
	{
		if (title)
			CopyToString("", title);
		if (length_in_ms)
			*length_in_ms = -1000;
	}
}

int infoBox(const in_char *file, HWND hwndParent)
{
	xSFConfig->InitConfig();

	auto xSF = std::make_unique<XSFFile>();
	if (!file || !*file)
		*xSF = *xSFFile;
	else
	{
		try
		{
			auto tmpxSF = std::make_unique<XSFFile>(file);
			*xSF = *tmpxSF;
		}
		catch (const std::exception &)
		{
			return INFOBOX_UNCHANGED;
		}
	}
	// TODO: Eventually make a dialog box for editing the info
	/*xSFFileInInfo = xSF.get();
	xSFConfig->CallInfoDialog(inMod.hDllInstance, hwndParent);*/
	const auto& tags = xSF->GetAllTags();
	const auto& keys = tags.GetKeys();
	std::wstring info;
	for (size_t x = 0, numTags = keys.size(); x < numTags; ++x)
	{
		if (x)
		{
			info.append(L"\n", 1);
		}

		const auto& key = keys[x];
		info += ConvertFuncs::StringToWString(key + "=" + tags[key]);
	}
	MessageBoxW(hwndParent, info.c_str(), ConvertFuncs::StringToWString(xSF->GetFilenameWithoutPath()).c_str(), MB_OK);
	return INFOBOX_EDITED;
}

/*int isOurFile(const in_char *)
{
	return 0;
}*/

int play(const in_char *fn)
{
	try
	{
		xSFConfig->InitConfig();
		auto tmpxSFPlayer = std::unique_ptr<XSFPlayer>(XSFPlayer::Create(fn));
		xSFConfig->CopyConfigToMemory(tmpxSFPlayer.get(), true);
		if (!tmpxSFPlayer->Load())
			return 1;
		xSFConfig->CopyConfigToMemory(tmpxSFPlayer.get(), false);
		xSFFile = tmpxSFPlayer->GetXSFFile();
		paused = false;
		seek_needed = -1;
		decode_pos_ms = 0;

		const int sampleRate = tmpxSFPlayer->GetSampleRate();
		int maxlatency = (inMod.outMod && inMod.outMod->Open && sampleRate &&
						  NumChannels ? inMod.outMod->Open(sampleRate,
						  NumChannels, BitsPerSample, -1, -1) : -1);
		if (maxlatency < 0)
			return 1;
		inMod.SetInfo((sampleRate * NumChannels * BitsPerSample) / 1000, sampleRate / 1000, NumChannels, 1);
		inMod.SAVSAInit(maxlatency, sampleRate);
		inMod.VSASetInfo(sampleRate, NumChannels);
		inMod.outMod->SetVolume(-666);

		xSFPlayer = tmpxSFPlayer.release();
		killThread = false;
		thread_handle = StartPlaybackThread(playThread, &killThread, 0, nullptr);
		return 0;
	}
	catch (const std::exception &)
	{
		return 1;
	}
}

void pause()
{
	paused = true;
	if (inMod.outMod)
	{
		inMod.outMod->Pause(1);
	}
}

void unPause()
{
	paused = false;
	if (inMod.outMod)
	{
		inMod.outMod->Pause(0);
	}
}

int isPaused()
{
	return paused;
}

void stop()
{
	if (thread_handle != INVALID_HANDLE_VALUE)
	{
		killThread = true;
		if (WaitForSingleObject(thread_handle, 2000) == WAIT_TIMEOUT)
		{
			//MessageBoxW(inMod.hMainWindow, L"error asking thread to die!", L"error killing decode thread", 0);
			TerminateThread(thread_handle, 0);
		}
		CloseHandle(thread_handle);
		thread_handle = INVALID_HANDLE_VALUE;
	}
	if (inMod.outMod)
	{
		if (inMod.outMod->Close)
		{
			inMod.outMod->Close();
		}
		inMod.SAVSADeInit();
	}
	delete xSFPlayer;
	xSFPlayer = nullptr;
	xSFFile = nullptr;
}

int getLength()
{
	if (!xSFFile || !xSFFile->HasFile())
		return -1000;
	return xSFFile->GetLengthMS(xSFConfig->GetDefaultLength()) + xSFFile->GetFadeMS(xSFConfig->GetDefaultFade());
}

int getOutputTime()
{
	return (inMod.outMod ? inMod.outMod->GetOutputTime() : 0);
}

void setOutputTime(int time_in_ms)
{
	seek_needed = time_in_ms;
}

void setVolume(int volume)
{
	if (inMod.outMod && inMod.outMod->SetVolume)
	{
		inMod.outMod->SetVolume(volume);
	}
}

void setPan(int pan)
{
	if (inMod.outMod && inMod.outMod->SetPan)
	{
		inMod.outMod->SetPan(pan);
	}
}

void GetFileExtensions(void)
{
	static bool loaded_extensions;
	if (!loaded_extensions)
	{
		loaded_extensions = true;

		inMod.FileExtensions = (char *)XSFPlayer::WinampExts;
	}
}

In_Module inMod =
{
	IN_VER_WACUP,
	const_cast<char *>(XSFConfig::CommonNameWithVersion().c_str()), /* Unsafe but Winamp's SDK requires this */
	nullptr, /* Filled by Winamp */
	nullptr, /* Filled by Winamp */
	nullptr,
	1,
	IN_MODULE_FLAG_USES_OUTPUT_PLUGIN | IN_MODULE_FLAG_REPLAYGAIN,
	config,
	about,
	init,
	quit,
	getFileInfo,
	infoBox,
	0/*isOurFile*/,
	play,
	pause,
	unPause,
	isPaused,
	stop,
	getLength,
	getOutputTime,
	setOutputTime,
	setVolume,
	setPan,
	IN_INIT_VIS_RELATED_CALLS,
	nullptr, nullptr, /* DSP stuff, filled by Winamp */
	IN_INIT_WACUP_EQSET_EMPTY
	nullptr, /* Filled by Winamp */
	nullptr, /* Filled by Winamp */
	NULL,	// api_service
	INPUT_HAS_READ_META | INPUT_HAS_WRITE_META | INPUT_USES_UNIFIED_ALT3 |
	INPUT_HAS_FORMAT_CONVERSION_UNICODE | INPUT_HAS_FORMAT_CONVERSION_SET_TIME_MODE,
	GetFileExtensions,	// loading optimisation
	IN_INIT_WACUP_END_STRUCT
};

extern "C" __declspec(dllexport) In_Module *winampGetInModule2()
{
	return &inMod;
}

extern "C" __declspec(dllexport) int winampUninstallPlugin(HINSTANCE hDllInst, HWND hwndDlg, int param)
{
	// TODO
	// prompt to remove our settings with default as no (just incase)
	/*if (UninstallSettingsPrompt(reinterpret_cast<const wchar_t*>(plugin.description)))
	{
		REMOVE_INI_SECTION_W(app_nameW, 0);
	}*/

	// as we're doing too much in subclasses, etc we cannot allow for on-the-fly removal so need to do a normal reboot
	return IN_PLUGIN_UNINSTALL_REBOOT;
}

//static eq_str eqstr;

template<typename T> int nonspecificWinampGetExtendedFileInfo(const char *data, T *dest, std::size_t destlen)
{
	// the core can send a *.<ext> to us so for these values we
	// don't need to hit the files & can do a default response.
	if (SameStrA(data, "type") ||
		SameStrA(data, "lossless") ||
		SameStrA(data, "streammetadata"))
	{
		dest[0] = L'0';
		dest[1] = L'\0';
		return 1;
	}
    else if (SameStrNA(data, "stream", 6) &&
             (SameStrA((data + 6), "type") ||
              SameStrA((data + 6), "genre") ||
              SameStrA((data + 6), "url") ||
              SameStrA((data + 6), "name") ||
			  SameStrA((data + 6), "title")))
	{
		return 0;
	}
	else if (SameStrA(data, "family"))
	{
		CopyToString(const_cast<char *>(XSFPlayer::ShellDescription), dest);
		return 1;
	}
	return 0;
}

template<typename T> int wrapperWinampGetExtendedFileInfo(const XSFFile &file, const char *data, T *dest, std::size_t destlen)
{
	if (xSFConfig)
	{
		xSFConfig->InitConfig();

		try
		{
			std::string tagToGet = data;
			if (SameStrA(data, "album"))
				tagToGet = "game";
			else if (SameStrA(data, "publisher"))
				tagToGet = "copyright";
			else if (SameStrA(data, "tool"))
				tagToGet = XSFPlayer::SFby;

			std::string info;
			LPCSTR tag = tagToGet.c_str();
			if (!file.GetTagExists(tagToGet))
			{
				if (SameStrA(tag, "replaygain_track_gain"))
					return 1;
				else if (SameStrA(tag, "formatinformation"))
				{
					const int fade = file.GetFadeMS(xSFConfig->GetDefaultFade()),
							  length = file.GetLengthMS(xSFConfig->GetDefaultLength()) + fade;

					info = "Length: " + std::to_string(((length > 0) ? (length / 1000) : 0)) + " seconds\n"
						   "Fade: " + std::to_string((fade > 0) ? (fade / 1000) : 0) + " seconds\n"
						   "Data: " + file.GetTagValue("_lib") + "\nRipped by: " +
						   file.GetTagValue(XSFPlayer::SFby) + "\nTagger: " + file.GetTagValue("tagger");
				}
				else if (SameStrA(tag, "bitrate"))
				{
					const int br = (xSFConfig->GetSampleRate() * NumChannels * BitsPerSample);
					if (br > 0)
					{
						info = std::to_string((br / 1000));
					}
				}
				else if (SameStrA(tag, "samplerate"))
				{
					info = std::to_string(xSFConfig->GetSampleRate());
				}
				else if (SameStrA(tag, "bitdepth"))
				{
					// TODO is this correct though as it's been
					//      hard-coded to be 16-bit it should
					dest[0] = L'1';
					dest[1] = L'6';
					dest[2] = 0;
					return 1;
				}
			}
			else if (SameStrA(tag, "length"))
				info = std::to_string(file.GetLengthMS(xSFConfig->GetDefaultLength()) + file.GetFadeMS(xSFConfig->GetDefaultFade()));
			else if (SameStrA(tag, "year"))
			{
				const int year = AStr2I(file.GetTagValue(tagToGet).c_str());
				if (year > 0)
				{
					info = std::to_string(year);
				}
			}
			else
				info = file.GetTagValue(tagToGet);

			if (!info.empty())
			{
				CopyToString(info.substr(0, destlen - 1), dest);
				return 1;
			}
		}
		catch (const std::exception&)
		{
			return 0;
		}
	}
	return 0;
}

/*extern "C" __declspec(dllexport) int winampGetExtendedFileInfo(const char *fn, const char *data, char *dest, std::size_t destlen)
{
	try
	{
		if (!nonspecificWinampGetExtendedFileInfo(data, dest, destlen))
		{
			auto file = XSFFile(fn);
			return wrapperWinampGetExtendedFileInfo(file, data, dest, destlen);
		}
		return 1;
	}
	catch (const std::exception &)
	{
		return 0;
	}
}*/

// return 1 if you want winamp to show it's own file info dialogue, 0 if you want to show your own (via In_Module.InfoBox)
// if returning 1, remember to implement winampGetExtendedFileInfo("formatinformation")!
extern "C" __declspec(dllexport) int winampUseUnifiedFileInfoDlg(const wchar_t * fn)
{
	return 1;
}

// should return a child window of 513x271 pixels (341x164 in msvc dlg units), or return NULL for no tab.
// Fill in name (a buffer of namelen characters), this is the title of the tab (defaults to "Advanced").
// filename will be valid for the life of your window. n is the tab number. This function will first be 
// called with n == 0, then n == 1 and so on until you return NULL (so you can add as many tabs as you like).
// The window you return will recieve WM_COMMAND, IDOK/IDCANCEL messages when the user clicks OK or Cancel.
// when the user edits a field which is duplicated in another pane, do a SendMessage(GetParent(hwnd),WM_USER,(WPARAM)L"fieldname",(LPARAM)L"newvalue");
// this will be broadcast to all panes (including yours) as a WM_USER.
extern "C" __declspec(dllexport) HWND winampAddUnifiedFileInfoPane(int n, const wchar_t * filename,
																   HWND parent, wchar_t *name, size_t namelen)
{
	return NULL;
}

extern "C" __declspec(dllexport) int winampGetExtendedFileInfoW(const wchar_t *fn, const char *data, wchar_t *dest, std::size_t destlen)
{
	try
	{
		const bool reset = SameStrA(data, "reset");
		if (!reset && !nonspecificWinampGetExtendedFileInfo(data, dest, destlen))
		{
			static XSFFile *info_file;
			if (reset || !info_file || ConvertFuncs::StringToWString(info_file->GetFilename()) != fn)
			{
				if (info_file)
				{
					delete info_file;
					info_file = nullptr;
				}

				if (!reset && FilePathExists(fn, NULL))
				{
					info_file = new XSFFile(fn);
				}
			}
			return (info_file ? wrapperWinampGetExtendedFileInfo(*info_file, data, dest, destlen) : 0);
		}
		return 0;
	}
	catch (const std::exception &)
	{
		return 0;
	}
}

static std::unique_ptr<XSFFile> extendedXSFFile;

int wrapperWinampSetExtendedFileInfo(const char *data, const wchar_t *val)
{
	extendedXSFFile->SetTag(data, (val ? val : L""));
	return 1;
}

/*extern "C" __declspec(dllexport) int winampSetExtendedFileInfo(const char *fn, const char *data, const wchar_t *val)
{
	try
	{
		xSFConfig->InitConfig();
		if (!extendedXSFFile || extendedXSFFile->GetFilename() != fn)
			extendedXSFFile.reset(new XSFFile(fn));
		return wrapperWinampSetExtendedFileInfo(data, val);
	}
	catch (const std::exception &)
	{
		return 0;
	}
}*/

extern "C" __declspec(dllexport) int winampSetExtendedFileInfoW(const wchar_t *fn, const char *data, const wchar_t *val)
{
	try
	{
		xSFConfig->InitConfig();
		if (!extendedXSFFile || ConvertFuncs::StringToWString(extendedXSFFile->GetFilename()) != fn)
			extendedXSFFile.reset(new XSFFile(fn));
		return wrapperWinampSetExtendedFileInfo(data, val);
	}
	catch (const std::exception &)
	{
		return 0;
	}
}

extern "C" __declspec(dllexport) int winampWriteExtendedFileInfo()
{
	if (!extendedXSFFile || extendedXSFFile->GetFilename().empty())
		return 0;
	extendedXSFFile->SaveFile();
	return 1;
}

/*extern "C" __declspec(dllexport) int winampClearExtendedFileInfoW(const wchar_t *)
{
	return 0;
}*/

std::intptr_t wrapperWinampGetExtendedRead_open(std::unique_ptr<XSFPlayer> &&tmpxSFPlayer, int *size, int *bps, int *nch, int *srate)
{
	xSFConfig->CopyConfigToMemory(tmpxSFPlayer.get(), true);
	if (!tmpxSFPlayer->Load())
		return 0;
	tmpxSFPlayer->IgnoreVolume();
	xSFConfig->CopyConfigToMemory(tmpxSFPlayer.get(), false);
	if (size)
		*size = tmpxSFPlayer->GetLengthInSamples() * NumChannels * (BitsPerSample / 8);
	if (bps)
		*bps = BitsPerSample;
	if (nch)
		*nch = NumChannels;
	if (srate)
		*srate = tmpxSFPlayer->GetSampleRate();
	return reinterpret_cast<std::intptr_t>(tmpxSFPlayer.release());
}

/*extern "C" __declspec(dllexport) std::intptr_t winampGetExtendedRead_open(const char *fn, int *size, int *bps, int *nch, int *srate)
{
	try
	{
		auto tmpxSFPlayer = std::unique_ptr<XSFPlayer>(XSFPlayer::Create(fn));
		return wrapperWinampGetExtendedRead_open(std::move(tmpxSFPlayer), size, bps, nch, srate);
	}
	catch (const std::exception &)
	{
		return 0;
	}
}*/

extern "C" __declspec(dllexport) std::intptr_t winampGetExtendedRead_openW(const wchar_t *fn, int *size, int *bps, int *nch, int *srate)
{
	try
	{
		// because some / all of the libraries being called
		// are not thread-safe, it is not sensible for this
		// to be allowed to run if there's anything playing
		// otherwise it might play for a bit & then crashes
		if (thread_handle == INVALID_HANDLE_VALUE)
		{
			xSFConfig->InitConfig();
			auto tmpxSFPlayer = std::unique_ptr<XSFPlayer>(XSFPlayer::Create(fn));
			return wrapperWinampGetExtendedRead_open(std::move(tmpxSFPlayer), size, bps, nch, srate);
		}
		return 0;
	}
	catch (const std::exception &)
	{
		return 0;
	}
}

static int extendedSeekNeeded = -1;

extern "C" __declspec(dllexport) std::size_t winampGetExtendedRead_getData(std::intptr_t handle, char *dest, std::size_t len, int *killswitch)
{
	XSFPlayer *tmpxSFPlayer = reinterpret_cast<XSFPlayer *>(handle);
	if (!tmpxSFPlayer)
		return 0;

	const auto buffer_size = (576 * NumChannels * (BitsPerSample / 8));
	if (extendedSeekNeeded != -1)
	{
		auto dummyBuffer = std::vector<std::uint8_t>(buffer_size);
		if (tmpxSFPlayer->Seek(static_cast<unsigned>(extendedSeekNeeded), killswitch, dummyBuffer, nullptr))
			return 0;
		extendedSeekNeeded = -1;
	}
	unsigned copied = 0;
	bool done = false;
	while (copied + buffer_size < len && !done)
	{
		auto sampleBuffer = std::vector<std::uint8_t>(buffer_size);
		unsigned samplesWritten = 0;
		done = tmpxSFPlayer->FillBuffer(sampleBuffer, samplesWritten);
		std::copy_n(&sampleBuffer[0], samplesWritten * NumChannels * (BitsPerSample / 8), &dest[copied]);
		copied += samplesWritten * NumChannels * (BitsPerSample / 8);
		if (killswitch && *killswitch)
			break;
	}
	return copied;
}

extern "C" __declspec(dllexport) int winampGetExtendedRead_setTime(std::intptr_t, int millisecs)
{
	extendedSeekNeeded = millisecs;
	return 1;
}

extern "C" __declspec(dllexport) void winampGetExtendedRead_close(std::intptr_t handle)
{
	XSFPlayer *tmpxSFPlayer = reinterpret_cast<XSFPlayer *>(handle);
	if (tmpxSFPlayer)
		delete tmpxSFPlayer;
}

RUNTIME_HELPER_HANDLER