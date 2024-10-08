/*
 * xSF - Core Player
 * By Naram Qashat (CyberBotX) [cyberbotx@cyberbotx.com]
 *
 * Partially based on the vio*sf framework
 */

#include <algorithm>
#include <limits>
#include <memory>
#include <vector>
#include <cstdint>
#include "XSFCommon.h"
#include "XSFConfig.h"
#include "XSFPlayer.h"

extern XSFConfig *xSFConfig;

XSFPlayer::XSFPlayer() : xSF(), sampleRate(0), detectedSilenceSample(0), detectedSilenceSec(0), skipSilenceOnStartSec(5), lengthSample(0), fadeSample(0), currentSample(0),
	prevSampleL(CHECK_SILENCE_BIAS), prevSampleR(CHECK_SILENCE_BIAS), lengthInMS(-1), fadeInMS(-1), volume(1.0), ignoreVolume(false), uses32BitSamplesClampedTo16Bit(false)
{
}

XSFPlayer::XSFPlayer(const XSFPlayer &xSFPlayer) : xSF(new XSFFile()), sampleRate(xSFPlayer.sampleRate), detectedSilenceSample(xSFPlayer.detectedSilenceSample), detectedSilenceSec(xSFPlayer.detectedSilenceSec),
	skipSilenceOnStartSec(xSFPlayer.skipSilenceOnStartSec), lengthSample(xSFPlayer.lengthSample), fadeSample(xSFPlayer.fadeSample), currentSample(xSFPlayer.currentSample), prevSampleL(xSFPlayer.prevSampleL),
	prevSampleR(xSFPlayer.prevSampleR), lengthInMS(xSFPlayer.lengthInMS), fadeInMS(xSFPlayer.fadeInMS), volume(xSFPlayer.volume), ignoreVolume(xSFPlayer.ignoreVolume),
	uses32BitSamplesClampedTo16Bit(xSFPlayer.uses32BitSamplesClampedTo16Bit)
{
	*this->xSF = *xSFPlayer.xSF;
}

XSFPlayer &XSFPlayer::operator=(const XSFPlayer &xSFPlayer)
{
	if (this != &xSFPlayer)
	{
		if (!this->xSF)
			this->xSF.reset(new XSFFile());
		*this->xSF = *xSFPlayer.xSF;
		this->sampleRate = xSFPlayer.sampleRate;
		this->detectedSilenceSample = xSFPlayer.detectedSilenceSample;
		this->detectedSilenceSec = xSFPlayer.detectedSilenceSec;
		this->skipSilenceOnStartSec = xSFPlayer.skipSilenceOnStartSec;
		this->lengthSample = xSFPlayer.lengthSample;
		this->fadeSample = xSFPlayer.fadeSample;
		this->currentSample = xSFPlayer.currentSample;
		this->prevSampleL = xSFPlayer.prevSampleL;
		this->prevSampleR = xSFPlayer.prevSampleR;
		this->lengthInMS = xSFPlayer.lengthInMS;
		this->fadeInMS = xSFPlayer.fadeInMS;
		this->volume = xSFPlayer.volume;
		this->ignoreVolume = xSFPlayer.ignoreVolume;
		this->uses32BitSamplesClampedTo16Bit = xSFPlayer.uses32BitSamplesClampedTo16Bit;
	}
	return *this;
}

bool XSFPlayer::FillBuffer(std::vector<std::uint8_t> &buf, unsigned &samplesWritten)
{
	bool endFlag = false;
	unsigned detectSilence = xSFConfig->GetDetectSilenceSec();
	unsigned pos = 0;
	auto bufsize = (buf.size() >> 2);
	auto trueBuffer = std::vector<std::uint8_t>(bufsize << (this->uses32BitSamplesClampedTo16Bit ? 3 : 2));
	auto longBuffer = std::vector<std::uint8_t>(bufsize << 3);
	auto bufLong = reinterpret_cast<std::int32_t *>(&longBuffer[0]);
	while (pos < bufsize)
	{
		unsigned remain = static_cast<unsigned>(bufsize - pos), offset = pos;
		this->GenerateSamples(trueBuffer, pos << (this->uses32BitSamplesClampedTo16Bit ? 2 : 1), remain);
		if (this->uses32BitSamplesClampedTo16Bit)
		{
			auto trueBufLong = reinterpret_cast<std::int32_t *>(&trueBuffer[0]);
			std::copy_n(&trueBufLong[0], bufsize << 1, &bufLong[0]);
		}
		else
		{
			auto trueBufShort = reinterpret_cast<std::int16_t *>(&trueBuffer[0]);
			std::copy_n(&trueBufShort[0], bufsize << 1, &bufLong[0]);
		}
		if (detectSilence || skipSilenceOnStartSec)
		{
			unsigned skipOffset = 0;
			for (unsigned ofs = 0; ofs < remain; ++ofs)
			{
				std::uint32_t sampleL = bufLong[2 * (offset + ofs)], sampleR = bufLong[2 * (offset + ofs) + 1];
				bool silence = (sampleL + CHECK_SILENCE_BIAS + CHECK_SILENCE_LEVEL) - this->prevSampleL <= CHECK_SILENCE_LEVEL * 2 &&
					(sampleR + CHECK_SILENCE_BIAS + CHECK_SILENCE_LEVEL) - this->prevSampleR <= CHECK_SILENCE_LEVEL * 2;

				if (silence)
				{
					if (++this->detectedSilenceSample >= this->sampleRate)
					{
						this->detectedSilenceSample -= this->sampleRate;
						++this->detectedSilenceSec;
						if (this->skipSilenceOnStartSec && this->detectedSilenceSec >= this->skipSilenceOnStartSec)
						{
							this->skipSilenceOnStartSec = this->detectedSilenceSec = 0;
							if (ofs)
								skipOffset = ofs;
						}
					}
				}
				else
				{
					this->detectedSilenceSample = this->detectedSilenceSec = 0;
					if (this->skipSilenceOnStartSec)
					{
						this->skipSilenceOnStartSec = 0;
						if (ofs)
							skipOffset = ofs;
					}
				}

				prevSampleL = sampleL + CHECK_SILENCE_BIAS;
				prevSampleR = sampleR + CHECK_SILENCE_BIAS;
			}

			if (!this->skipSilenceOnStartSec)
			{
				if (skipOffset)
				{
					auto tmpBuf = std::vector<std::int32_t>((bufsize - skipOffset) << 1);
					std::copy(&bufLong[(offset + skipOffset) << 1], &bufLong[bufsize << 1], &tmpBuf[0]);
					std::copy_n(&tmpBuf[0], (bufsize - skipOffset) << 1, &bufLong[offset << 1]);
					pos += skipOffset;
				}
				else
					pos += remain;
			}
		}
		else
			pos += remain;
		if (pos < bufsize)
		{
			if (this->uses32BitSamplesClampedTo16Bit)
			{
				auto trueBufLong = reinterpret_cast<std::int32_t *>(&trueBuffer[0]);
				std::copy_n(&bufLong[0], bufsize << 1, &trueBufLong[0]);
			}
			else
			{
				auto trueBufShort = reinterpret_cast<std::int16_t *>(&trueBuffer[0]);
				std::copy_n(&bufLong[0], bufsize << 1, &trueBufShort[0]);
			}
		}
	}

	/* Detect end of song */
	if (!xSFConfig->GetPlayInfinitely())
	{
		if (this->currentSample >= this->lengthSample + this->fadeSample)
		{
			samplesWritten = 0;
			return true;
		}
		if (this->currentSample + bufsize >= this->lengthSample + this->fadeSample)
		{
			bufsize = this->lengthSample + this->fadeSample - this->currentSample;
			endFlag = true;
		}
	}

	/* Volume */
	if (!this->ignoreVolume && (!fEqual(this->volume, 1.0) || !fEqual(xSFConfig->GetVolume(), 1.0)))
	{
		double scale = this->volume * xSFConfig->GetVolume();
		for (unsigned ofs = 0; ofs < bufsize; ++ofs)
		{
			double s1 = bufLong[2 * ofs] * scale, s2 = bufLong[2 * ofs + 1] * scale;
			if (!this->uses32BitSamplesClampedTo16Bit)
			{
				s1 = std::clamp<double>(s1, std::numeric_limits<std::int16_t>::min(), std::numeric_limits<std::int16_t>::max());
				s2 = std::clamp<double>(s2, std::numeric_limits<std::int16_t>::min(), std::numeric_limits<std::int16_t>::max());
			}
			bufLong[2 * ofs] = static_cast<std::int32_t>(s1);
			bufLong[2 * ofs + 1] = static_cast<std::int32_t>(s2);
		}
	}

	if (this->uses32BitSamplesClampedTo16Bit)
	{
		auto bufShort = reinterpret_cast<std::int16_t *>(&buf[0]);
		for (unsigned ofs = 0; ofs < bufsize; ++ofs)
		{
			std::int32_t s1 = bufLong[2 * ofs], s2 = bufLong[2 * ofs + 1];
			s1 = std::clamp<std::int32_t>(s1, std::numeric_limits<std::int16_t>::min(), std::numeric_limits<std::int16_t>::max());
			s2 = std::clamp<std::int32_t>(s2, std::numeric_limits<std::int16_t>::min(), std::numeric_limits<std::int16_t>::max());
			bufShort[2 * ofs] = static_cast<std::int16_t>(s1);
			bufShort[2 * ofs + 1] = static_cast<std::int16_t>(s2);
		}
	}
	else
	{
		auto trueBufShort = reinterpret_cast<std::int16_t *>(&trueBuffer[0]);
		std::copy_n(&bufLong[0], bufsize << 1, &trueBufShort[0]);
		std::copy(&trueBuffer[0], &trueBuffer[bufsize << 2], &buf[0]);
	}

	/* Fading */
	if (!xSFConfig->GetPlayInfinitely() && this->fadeSample && this->currentSample + bufsize >= this->lengthSample)
	{
		auto bufShort = reinterpret_cast<std::int16_t *>(&buf[0]);
		for (unsigned ofs = 0; ofs < bufsize; ++ofs)
		{
			if (this->currentSample + ofs >= this->lengthSample && this->currentSample + ofs < this->lengthSample + this->fadeSample)
			{
				int scale = static_cast<std::uint64_t>(this->lengthSample + this->fadeSample - (this->currentSample + ofs)) * 0x10000 / this->fadeSample;
				bufShort[2 * ofs] = (bufShort[2 * ofs] * scale) >> 16;
				bufShort[2 * ofs + 1] = (bufShort[2 * ofs + 1] * scale) >> 16;
			}
			else if (this->currentSample + ofs >= this->lengthSample + this->fadeSample)
				bufShort[2 * ofs] = bufShort[2 * ofs + 1] = 0;
		}
	}

	this->currentSample += static_cast<unsigned>(bufsize);
	samplesWritten = static_cast<unsigned>(bufsize);
	return endFlag;
}

bool XSFPlayer::Load()
{
	this->lengthInMS = this->xSF->GetLengthMS(xSFConfig->GetDefaultLength());
	this->fadeInMS = this->xSF->GetFadeMS(xSFConfig->GetDefaultFade());
	this->lengthSample = static_cast<std::uint64_t>(this->lengthInMS) * this->sampleRate / 1000;
	this->fadeSample = static_cast<std::uint64_t>(this->fadeInMS) * this->sampleRate / 1000;
	this->volume = this->xSF->GetVolume(xSFConfig->GetVolumeType(), xSFConfig->GetPeakType());
	return true;
}

void XSFPlayer::SeekTop()
{
	this->skipSilenceOnStartSec = xSFConfig->GetSkipSilenceOnStartSec();
	this->currentSample = this->detectedSilenceSec = this->detectedSilenceSample = 0;
	this->prevSampleL = this->prevSampleR = CHECK_SILENCE_BIAS;
}

#ifdef WINAMP_PLUGIN
static inline DWORD TicksDiff(DWORD prev, DWORD cur) { return cur >= prev ? cur - prev : 0xFFFFFFFF - prev + cur; }

int XSFPlayer::Seek(unsigned seekPosition, volatile int *killswitch, std::vector<std::uint8_t> &buf, Out_Module *outMod)
{
	const unsigned bufsize = static_cast<unsigned>(buf.size() >> 2), seekSample = static_cast<std::uint64_t>(seekPosition) * this->sampleRate / 1000;
	DWORD prevTick = outMod ? GetTickCount() : 0;
	if (seekSample < this->currentSample)
	{
		this->Terminate();
		this->Load();
		this->SeekTop();
	}
	while (seekSample - this->currentSample > bufsize)
	{
		if (killswitch && *killswitch)
			return 1;
		if (outMod)
		{
			DWORD curTick = GetTickCount();
			if (TicksDiff(prevTick, curTick) >= 500)
			{
				prevTick = curTick;
				unsigned cur = static_cast<std::uint64_t>(this->currentSample) * 1000 / this->sampleRate;
				outMod->Flush(cur);
			}
		}
		this->GenerateSamples(buf, 0, bufsize, false);
		this->currentSample += bufsize;
	}
	if (seekSample - this->currentSample > 0)
	{
		this->GenerateSamples(buf, 0, seekSample - this->currentSample);
		this->currentSample = seekSample;
	}
	if (outMod)
		outMod->Flush(seekPosition);
	return 0;
}
#endif
