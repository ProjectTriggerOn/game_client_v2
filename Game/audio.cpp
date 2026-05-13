#include "audio.h"

#include <xaudio2.h>
#include <cassert>

#pragma comment(lib, "winmm.lib")

namespace
{
	IXAudio2* g_Xaudio{};
	IXAudio2MasteringVoice* g_MasteringVoice{};

	constexpr int AUDIO_MAX = 100;

	struct AudioClip
	{
		IXAudio2SourceVoice* sourceVoice{};
		BYTE*                soundData{};
		int                  length{};
		int                  playLength{};
	};

	AudioClip g_Audio[AUDIO_MAX]{};
}

void InitAudio()
{
	XAudio2Create(&g_Xaudio, 0);
	g_Xaudio->CreateMasteringVoice(&g_MasteringVoice);
}

void UninitAudio()
{
	g_MasteringVoice->DestroyVoice();
	g_Xaudio->Release();
}

int LoadAudio(const char* fileName)
{
	int index = -1;
	for (int i = 0; i < AUDIO_MAX; i++)
	{
		if (g_Audio[i].sourceVoice == nullptr)
		{
			index = i;
			break;
		}
	}
	if (index == -1) return -1;

	WAVEFORMATEX wfx = { 0 };
	{
		HMMIO hmmio = NULL;
		MMIOINFO mmioinfo = { 0 };
		MMCKINFO riffchunkinfo = { 0 };
		MMCKINFO datachunkinfo = { 0 };
		MMCKINFO mmckinfo = { 0 };
		UINT32 buflen;
		LONG readlen;

		hmmio = mmioOpen((LPSTR)fileName, &mmioinfo, MMIO_READ);
		assert(hmmio);

		riffchunkinfo.fccType = mmioFOURCC('W', 'A', 'V', 'E');
		mmioDescend(hmmio, &riffchunkinfo, NULL, MMIO_FINDRIFF);

		mmckinfo.ckid = mmioFOURCC('f', 'm', 't', ' ');
		mmioDescend(hmmio, &mmckinfo, &riffchunkinfo, MMIO_FINDCHUNK);

		if (mmckinfo.cksize >= sizeof(WAVEFORMATEX))
		{
			mmioRead(hmmio, (HPSTR)&wfx, sizeof(wfx));
		}
		else
		{
			PCMWAVEFORMAT pcmwf = { 0 };
			mmioRead(hmmio, (HPSTR)&pcmwf, sizeof(pcmwf));
			memset(&wfx, 0x00, sizeof(wfx));
			memcpy(&wfx, &pcmwf, sizeof(pcmwf));
			wfx.cbSize = 0;
		}
		mmioAscend(hmmio, &mmckinfo, 0);

		datachunkinfo.ckid = mmioFOURCC('d', 'a', 't', 'a');
		mmioDescend(hmmio, &datachunkinfo, &riffchunkinfo, MMIO_FINDCHUNK);

		buflen = datachunkinfo.cksize;
		g_Audio[index].soundData = new unsigned char[buflen];
		readlen = mmioRead(hmmio, (HPSTR)g_Audio[index].soundData, buflen);

		g_Audio[index].length = readlen;
		g_Audio[index].playLength = readlen / wfx.nBlockAlign;

		mmioClose(hmmio, 0);
	}

	g_Xaudio->CreateSourceVoice(&g_Audio[index].sourceVoice, &wfx);
	assert(g_Audio[index].sourceVoice);

	return index;
}

void UnloadAudio(int index)
{
	g_Audio[index].sourceVoice->Stop();
	g_Audio[index].sourceVoice->DestroyVoice();

	delete[] g_Audio[index].soundData;
	g_Audio[index].soundData = nullptr;
}

void PlayAudio(int index, bool loop)
{
	g_Audio[index].sourceVoice->Stop();
	g_Audio[index].sourceVoice->FlushSourceBuffers();

	XAUDIO2_BUFFER bufinfo;
	memset(&bufinfo, 0x00, sizeof(bufinfo));
	bufinfo.AudioBytes = g_Audio[index].length;
	bufinfo.pAudioData = g_Audio[index].soundData;
	bufinfo.PlayBegin = 0;
	bufinfo.PlayLength = g_Audio[index].playLength;

	if (loop)
	{
		bufinfo.LoopBegin = 0;
		bufinfo.LoopLength = g_Audio[index].playLength;
		bufinfo.LoopCount = XAUDIO2_LOOP_INFINITE;
	}

	g_Audio[index].sourceVoice->SubmitSourceBuffer(&bufinfo, NULL);
	g_Audio[index].sourceVoice->Start();
}

void SetVolume(int index, float volume)
{
	assert(g_Audio[index].sourceVoice);
	g_Audio[index].sourceVoice->SetVolume(volume);
}
