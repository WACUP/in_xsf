/*
 * xSF - File structure
 * By Naram Qashat (CyberBotX) [cyberbotx@cyberbotx.com]
 *
 * Partially based on the vio*sf framework
 */

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>
#include <vector>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include "XSFCommon.h"
#include "XSFFile.h"
#include "convert.h"
#if defined(_WIN32) && !defined(_MSC_VER)
# include "fstream_wfopen.h"
#endif
#include "zlib.h"

static inline void Set32BitsLE(std::uint32_t input, std::uint8_t *output)
{
	output[0] = input & 0xFF;
	output[1] = (input >> 8) & 0xFF;
	output[2] = (input >> 16) & 0xFF;
	output[3] = (input >> 24) & 0xFF;
}

// The whitespace trimming was modified from the following answer on Stack Overflow:
// http://stackoverflow.com/a/217605

bool IsWhitespace(const char &x)
{
	return x >= 0x01 && x <= 0x20;
}

static inline std::string LeftTrimWhitespace(const std::string &orig)
{
	auto first_non_space = std::find_if(orig.begin(), orig.end(), [](const char &x)
	{
		return !IsWhitespace(x);
	});
	return std::string(first_non_space, orig.end());
}

static inline std::string RightTrimWhitespace(const std::string &orig)
{
	auto last_non_space = std::find_if(orig.rbegin(), orig.rend(), [](const char &x)
	{
		return !IsWhitespace(x);
	}).base();
	return std::string(orig.begin(), last_non_space);
}

static inline std::string TrimWhitespace(const std::string &orig)
{
	return LeftTrimWhitespace(RightTrimWhitespace(orig));
}

XSFFile::XSFFile() : rawData(), reservedSection(), programSection(), tags(), fileName(""), xSFType(0), hasFile(false)
{
}

XSFFile::XSFFile(const std::string &filename) : xSFType(0), hasFile(false), rawData(), reservedSection(), programSection(), tags(), fileName(filename)
{
	this->ReadXSF(filename, 0, 0, true);
}

XSFFile::XSFFile(const std::string &filename, std::uint32_t programSizeOffset, std::uint32_t programHeaderSize) : xSFType(0), hasFile(false), rawData(), reservedSection(), programSection(), tags(), fileName(filename)
{
	this->ReadXSF(filename, programSizeOffset, programHeaderSize);
}

#ifdef _WIN32
XSFFile::XSFFile(const std::wstring &filename) : xSFType(0), hasFile(false), rawData(), reservedSection(), programSection(), tags(), fileName(ConvertFuncs::WStringToString(filename))
{
	this->ReadXSF(filename, 0, 0, true);
}

XSFFile::XSFFile(const std::wstring &filename, std::uint32_t programSizeOffset, std::uint32_t programHeaderSize) : xSFType(0), hasFile(false), rawData(), reservedSection(), programSection(), tags(), fileName(ConvertFuncs::WStringToString(filename))
{
	this->ReadXSF(filename, programSizeOffset, programHeaderSize);
}
#endif

void XSFFile::ReadXSF(const std::string &filename, std::uint32_t programSizeOffset, std::uint32_t programHeaderSize, bool readTagsOnly)
{
	if (!std::filesystem::is_regular_file(filename))
		throw std::logic_error("File " + filename + " does not exist.");

	std::ifstream xSF;
	xSF.exceptions(std::ifstream::failbit | std::ifstream::badbit);
	xSF.open(filename.c_str(), std::ifstream::in | std::ifstream::binary);

	this->ReadXSF(xSF, programSizeOffset, programHeaderSize, readTagsOnly);

	xSF.close();
}

#ifdef _WIN32
void XSFFile::ReadXSF(const std::wstring &filename, std::uint32_t programSizeOffset, std::uint32_t programHeaderSize, bool readTagsOnly)
{
	if (!std::filesystem::is_regular_file(filename))
		throw std::logic_error("File " + ConvertFuncs::WStringToString(filename) + " does not exist.");

#if defined(_WIN32) && !defined(_MSC_VER)
	ifstream_wfopen xSF;
#else
	std::ifstream xSF;
#endif
	xSF.exceptions(std::ifstream::failbit | std::ifstream::badbit);
	xSF.open(filename.c_str(), std::ifstream::in | std::ifstream::binary);

	this->ReadXSF(xSF, programSizeOffset, programHeaderSize, readTagsOnly);

	xSF.close();
}
#endif

void XSFFile::ReadXSF(std::ifstream &xSF, std::uint32_t programSizeOffset, std::uint32_t programHeaderSize, bool readTagsOnly)
{
	xSF.seekg(0, std::ifstream::end);
	auto filesize = xSF.tellg();
	xSF.seekg(0, std::ifstream::beg);

	if (filesize < 4)
		throw std::runtime_error("File is too small.");

	char PSFHeader[4];
	xSF.read(PSFHeader, 4);

	if (PSFHeader[0] != 'P' || PSFHeader[1] != 'S' || PSFHeader[2] != 'F')
		throw std::runtime_error("Not a PSF file.");

	this->xSFType = PSFHeader[3];

	this->rawData.resize(4);
	std::copy_n(PSFHeader, 4, &this->rawData[0]);

	if (filesize < 16)
		throw std::runtime_error("File is too small.");

	std::uint32_t reservedSize = Get32BitsLE(xSF), programCompressedSize = Get32BitsLE(xSF);
	this->rawData.resize(reservedSize + programCompressedSize + 16);
	Set32BitsLE(reservedSize, &this->rawData[4]);
	Set32BitsLE(programCompressedSize, &this->rawData[8]);
	xSF.read(reinterpret_cast<char *>(&this->rawData[12]), 4);

	if (reservedSize)
	{
		if (filesize < reservedSize + 16)
			throw std::runtime_error("File is too small.");

		if (readTagsOnly)
			xSF.read(reinterpret_cast<char *>(&this->rawData[16]), reservedSize);
		else
		{
			this->reservedSection.resize(reservedSize);
			xSF.read(reinterpret_cast<char *>(&this->reservedSection[0]), reservedSize);
			std::copy_n(&this->reservedSection[0], reservedSize, &this->rawData[16]);
		}
	}

	if (programCompressedSize)
	{
		if (filesize < reservedSize + programCompressedSize + 16)
			throw std::runtime_error("File is too small.");

		if (readTagsOnly)
			xSF.read(reinterpret_cast<char *>(&this->rawData[reservedSize + 16]), programCompressedSize);
		else
		{
			auto programSectionCompressed = std::vector<std::uint8_t>(programCompressedSize);
			xSF.read(reinterpret_cast<char *>(&programSectionCompressed[0]), programCompressedSize);
			std::copy_n(&programSectionCompressed[0], programCompressedSize, &this->rawData[reservedSize + 16]);

			auto programSectionUncompressed = std::vector<std::uint8_t>(programHeaderSize);
			unsigned long programUncompressedSize = programHeaderSize;
			uncompress(&programSectionUncompressed[0], &programUncompressedSize, &programSectionCompressed[0], programCompressedSize);
			programUncompressedSize = Get32BitsLE(&programSectionUncompressed[programSizeOffset]) + programHeaderSize;
			this->programSection.resize(programUncompressedSize);
			uncompress(&this->programSection[0], &programUncompressedSize, &programSectionCompressed[0], programCompressedSize);
		}
	}

	if (xSF.tellg() != filesize && filesize >= reservedSize + programCompressedSize + 21)
	{
		char tagheader[6] = "";
		xSF.read(tagheader, 5);
		if (std::string(tagheader) == "[TAG]")
		{
			auto startOfTags = xSF.tellg();
			unsigned lengthOfTags = static_cast<unsigned>(filesize - startOfTags);
			if (lengthOfTags)
			{
				auto rawtags = std::vector<char>(lengthOfTags);
				xSF.read(&rawtags[0], lengthOfTags);
				std::string name, value;
				bool onName = true;
				for (auto curr : rawtags)
				{
					if (curr == 0x0A)
					{
						if (!name.empty() && !value.empty())
						{
							name = TrimWhitespace(name);
							value = TrimWhitespace(value);
							if (this->tags.Exists(name))
								this->tags[name] += "\n" + value;
							else
								this->tags[name] = value;
						}
						name = value = "";
						onName = true;
						continue;
					}
					if (curr == '=')
					{
						onName = false;
						continue;
					}
					if (onName)
						name += curr;
					else
						value += curr;
				}
			}
		}
	}

	this->hasFile = true;
}

bool XSFFile::IsValidType(std::uint8_t type) const
{
	return this->xSFType == type;
}

void XSFFile::Clear()
{
	this->xSFType = 0;
	this->hasFile = false;
	this->reservedSection.clear();
	this->programSection.clear();
	this->tags.Clear();
}

bool XSFFile::HasFile() const
{
	return this->hasFile;
}

std::vector<std::uint8_t> &XSFFile::GetReservedSection()
{
	return this->reservedSection;
}

std::vector<std::uint8_t> XSFFile::GetReservedSection() const
{
	return this->reservedSection;
}

std::vector<std::uint8_t> &XSFFile::GetProgramSection()
{
	return this->programSection;
}

std::vector<std::uint8_t> XSFFile::GetProgramSection() const
{
	return this->programSection;
}

const TagList &XSFFile::GetAllTags() const
{
	return this->tags;
}

void XSFFile::SetAllTags(const TagList &newTags)
{
	this->tags = newTags;
}

void XSFFile::SetTag(const std::string &name, const std::string &value)
{
	this->tags[name] = value;
}

void XSFFile::SetTag(const std::string &name, const std::wstring &value)
{
	this->tags[name] = ConvertFuncs::WStringToString(value);
}

bool XSFFile::GetTagExists(const std::string &name) const
{
	return this->tags.Exists(name);
}

std::string XSFFile::GetTagValue(const std::string &name) const
{
	return this->GetTagExists(name) ? this->tags[name] : "";
}

unsigned long XSFFile::GetLengthMS(unsigned long defaultLength) const
{
	unsigned long length = 0;
	std::string value = this->GetTagValue("length");
	if (!value.empty())
		length = ConvertFuncs::StringToMS(value);
	if (!length)
		length = defaultLength;
	return length;
}

unsigned long XSFFile::GetFadeMS(unsigned long defaultFade) const
{
	unsigned long fade = defaultFade;
	std::string value = this->GetTagValue("fade");
	if (!value.empty())
		fade = ConvertFuncs::StringToMS(value);
	return fade;
}

float XSFFile::GetVolume(VolumeType preferredVolumeType, PeakType preferredPeakType) const
{
	if (preferredVolumeType == VolumeType::None)
		return 1.0f;
	std::string replaygain_album_gain = this->GetTagValue("replaygain_album_gain"), replaygain_album_peak = this->GetTagValue("replaygain_album_peak");
	std::string replaygain_track_gain = this->GetTagValue("replaygain_track_gain"), replaygain_track_peak = this->GetTagValue("replaygain_track_peak");
	std::string volume = this->GetTagValue("volume");
	float gain = 0.0f;
	bool hadReplayGain = false;
	if (preferredVolumeType == VolumeType::ReplayGainAlbum && !replaygain_album_gain.empty())
	{
		gain = ConvertFuncs::To<float>(replaygain_album_gain);
		hadReplayGain = true;
	}
	if (!hadReplayGain && preferredVolumeType != VolumeType::Volume && !replaygain_track_gain.empty())
	{
		gain = ConvertFuncs::To<float>(replaygain_track_gain);
		hadReplayGain = true;
	}
	if (hadReplayGain)
	{
		float vol = std::pow(10.0f, gain / 20.0f), peak = 1.0f;
		if (preferredPeakType == PeakType::ReplayGainAlbum && !replaygain_album_peak.empty())
			peak = ConvertFuncs::To<float>(replaygain_album_peak);
		else if (preferredPeakType != PeakType::None && !replaygain_track_peak.empty())
			peak = ConvertFuncs::To<float>(replaygain_track_peak);
		return !fEqual(peak, 1.0f) ? std::min(vol, 1.0f / peak) : vol;
	}
	return volume.empty() ? 1.0f : ConvertFuncs::To<float>(volume);
}

std::string XSFFile::FormattedTitleOptionalBlock(const std::string &block, bool &hadReplacement, unsigned level) const
{
	std::string formattedBlock;
	for (std::size_t x = 0, len = block.length(); x < len; ++x)
	{
		char c = block[x];
		if (c == '%')
		{
			std::size_t origX = x;
			for (++x; x < len; ++x)
				if (block[x] == '%')
					break;
			if (x != len)
			{
				std::string tagname = block.substr(origX + 1, x - origX - 1);
				std::string value = this->GetTagValue(tagname);
				if (!value.empty())
				{
					formattedBlock += value;
					hadReplacement = true;
				}
			}
			continue;
		}
		if (c == '[' && level + 1 < 10)
		{
			std::size_t origX = x;
			unsigned nests = 0;
			for (++x; x < len; ++x)
			{
				if (block[x] == '[')
					++nests;
				else if (block[x] == ']')
				{
					if (!nests)
						break;
					--nests;
				}
			}
			if (x != len)
			{
				std::string innerBlock = block.substr(origX + 1, x - origX - 1);
				bool hadInnerReplacement = false;
				std::string replacedBlock = this->FormattedTitleOptionalBlock(innerBlock, hadInnerReplacement, level + 1);
				if (hadInnerReplacement)
					formattedBlock += replacedBlock;
			}
			continue;
		}
		formattedBlock += c;
	}
	return formattedBlock;
}

std::string XSFFile::GetFormattedTitle(const std::string &format) const
{
	std::string formattedTitle;
	for (std::size_t x = 0, len = format.length(); x < len; ++x)
	{
		char c = format[x];
		if (c == '%')
		{
			std::size_t origX = x;
			for (++x; x < len; ++x)
				if (format[x] == '%')
					break;
			if (x != len)
			{
				std::string tagname = format.substr(origX + 1, x - origX - 1);
				std::string value = this->GetTagValue(tagname);
				if (value.empty())
					formattedTitle += "???";
				else
					formattedTitle += value;
			}
		}
		else if (c == '[')
		{
			std::size_t origX = x;
			unsigned nests = 0;
			for (++x; x < len; ++x)
			{
				if (format[x] == '[')
					++nests;
				else if (format[x] == ']')
				{
					if (!nests)
						break;
					--nests;
				}
			}
			if (x != len)
			{
				std::string block = format.substr(origX + 1, x - origX - 1);
				bool hadReplacement = false;
				std::string replacedBlock = this->FormattedTitleOptionalBlock(block, hadReplacement, 1);
				if (hadReplacement)
					formattedTitle += replacedBlock;
			}
		}
		else
			formattedTitle += c;
	}
	return formattedTitle;
}

std::string XSFFile::GetFilename() const
{
	return this->fileName;
}

std::string XSFFile::GetFilenameWithoutPath() const
{
	return std::filesystem::path(this->fileName).filename().string();
}

void XSFFile::SaveFile() const
{
#if defined(_WIN32) && !defined(_MSC_VER)
	ofstream_wfopen xSF;
#else
	std::ofstream xSF;
#endif
	xSF.exceptions(std::ofstream::failbit);
#ifdef _WIN32
	xSF.open(ConvertFuncs::StringToWString(this->fileName).c_str(), std::ofstream::out | std::ofstream::binary);
#else
	xSF.open(this->fileName.c_str(), std::ofstream::out | std::ofstream::binary);
#endif

	xSF.write(reinterpret_cast<const char *>(&this->rawData[0]), this->rawData.size());

	auto allTags = this->tags.GetTags();
	if (!allTags.empty())
	{
		xSF.write("[TAG]", 5);
		for (const auto &tag : allTags)
		{
			xSF.write(tag.c_str(), tag.length());
			xSF.write("\n", 1);
		}
	}
}
