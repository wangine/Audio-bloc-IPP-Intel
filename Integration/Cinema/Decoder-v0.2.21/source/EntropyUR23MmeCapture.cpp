#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <mmsystem.h>

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

namespace
{
void u16(FILE* f, std::uint16_t v) { fwrite(&v, sizeof(v), 1, f); }
void u32(FILE* f, std::uint32_t v) { fwrite(&v, sizeof(v), 1, f); }

void writeWav(const std::filesystem::path& path, const std::vector<char>& audio,
              std::size_t bytes)
{
    FILE* file = nullptr;
    if (_wfopen_s(&file, path.c_str(), L"wb") != 0 || file == nullptr)
        throw std::runtime_error("cannot create WAV");
    fwrite("RIFF", 1, 4, file); u32(file, static_cast<std::uint32_t>(36 + bytes));
    fwrite("WAVEfmt ", 1, 8, file); u32(file, 16); u16(file, WAVE_FORMAT_PCM);
    u16(file, 2); u32(file, 48000); u32(file, 192000); u16(file, 4); u16(file, 16);
    fwrite("data", 1, 4, file); u32(file, static_cast<std::uint32_t>(bytes));
    fwrite(audio.data(), 1, bytes, file); fclose(file);
}

void printMmError(MMRESULT result)
{
    wchar_t text[MAXERRORLENGTH] {};
    waveInGetErrorTextW(result, text, MAXERRORLENGTH);
    std::wcerr << L"MMRESULT " << result << L": " << text << L"\n";
}
} // namespace

int wmain(int argc, wchar_t** argv)
{
    const int seconds = argc >= 2 ? std::max(1, _wtoi(argv[1])) : 10;
    const std::filesystem::path output = argc >= 3 ? argv[2] : L"ur23-mme.wav";
    const UINT count = waveInGetNumDevs();
    UINT selected = WAVE_MAPPER;
    std::wcout << L"MME input devices: " << count << L"\n";
    for (UINT index = 0; index < count; ++index)
    {
        WAVEINCAPSW caps {};
        if (waveInGetDevCapsW(index, &caps, sizeof(caps)) != MMSYSERR_NOERROR)
            continue;
        std::wcout << L"  [" << index << L"] " << caps.szPname << L"\n";
        std::wstring name = caps.szPname;
        std::transform(name.begin(), name.end(), name.begin(), towlower);
        if (name.find(L"ur23") != std::wstring::npos ||
            name.find(L"spdif interface") != std::wstring::npos)
            selected = index;
    }
    if (argc >= 4)
        selected = static_cast<UINT>(_wtoi(argv[3]));
    if (selected == WAVE_MAPPER)
    {
        std::cerr << "UR23 MME input not found\n";
        return 2;
    }
    WAVEFORMATEX format {};
    format.wFormatTag = WAVE_FORMAT_PCM;
    format.nChannels = 2;
    format.nSamplesPerSec = 48000;
    format.nAvgBytesPerSec = 192000;
    format.nBlockAlign = 4;
    format.wBitsPerSample = 16;
    HWAVEIN input = nullptr;
    MMRESULT result = waveInOpen(&input, selected, &format, 0, 0,
                                 CALLBACK_NULL | WAVE_FORMAT_DIRECT);
    if (result != MMSYSERR_NOERROR)
    {
        std::wcerr << L"WAVE_FORMAT_DIRECT failed; trying standard MME.\n";
        result = waveInOpen(&input, selected, &format, 0, 0, CALLBACK_NULL);
    }
    if (result != MMSYSERR_NOERROR)
    {
        printMmError(result);
        return 3;
    }
    std::vector<char> audio(static_cast<std::size_t>(seconds) * 192000u);
    WAVEHDR header {};
    header.lpData = audio.data();
    header.dwBufferLength = static_cast<DWORD>(audio.size());
    result = waveInPrepareHeader(input, &header, sizeof(header));
    if (result == MMSYSERR_NOERROR)
        result = waveInAddBuffer(input, &header, sizeof(header));
    if (result == MMSYSERR_NOERROR)
        result = waveInStart(input);
    if (result != MMSYSERR_NOERROR)
    {
        printMmError(result); waveInClose(input); return 4;
    }
    const auto deadline = GetTickCount64() + static_cast<ULONGLONG>(seconds * 1000u + 3000u);
    while ((header.dwFlags & WHDR_DONE) == 0 && GetTickCount64() < deadline)
        Sleep(5);
    waveInStop(input);
    waveInReset(input);
    const auto captured = static_cast<std::size_t>(header.dwBytesRecorded);
    waveInUnprepareHeader(input, &header, sizeof(header));
    waveInClose(input);
    std::uint64_t normal = 0, swapped = 0, nonzero = 0;
    for (std::size_t i = 0; i + 3 < captured; i += 4)
    {
        const auto* p = reinterpret_cast<const std::uint8_t*>(audio.data() + i);
        const auto a = static_cast<std::uint16_t>(p[0] | (p[1] << 8));
        const auto b = static_cast<std::uint16_t>(p[2] | (p[3] << 8));
        normal += a == 0xF872 && b == 0x4E1F;
        swapped += a == 0x72F8 && b == 0x1F4E;
        nonzero += a != 0 || b != 0;
    }
    writeWav(output, audio, captured);
    std::wcout << L"Selected MME device " << selected << L"; captured bytes="
               << captured << L" nonzero_frames=" << nonzero << L"\n"
               << L"IEC61937 Pa/Pb normal=" << normal << L" swapped=" << swapped
               << L"\nOutput=" << output.wstring() << L"\n";
    return captured > 0 ? 0 : 6;
}
