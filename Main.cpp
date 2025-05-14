#include <iostream>

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <rtaudio/RtAudio.h>

using namespace std;

int BoxCount = 30;

string Title = "MelodicPolyRhythm";
int Width = 1280;
int Height = 720;
int CX = Width / 2;
int CY = Height / 2;

SDL_Window* SdlWindow;
SDL_Renderer* SdlRenderer;
SDL_Event SdlEvent;
SDL_Rect SdlRect;

RtAudio TheRtAudio;
unsigned int AudioDevice = 0;
unsigned int Samplerate = 44100;
unsigned int Channels = 2;
unsigned int BufferSize = 256;
unsigned int NumBuffers = 3;

double PI2 = M_PI * 2;
double D2R = M_PI / 180;

class Delay
{
public:
	int Samplerate;
	int Time;
	double Feedback;
	double Mix;

	vector<double> Buffer;
	int BufferLength;
	int Index = 0;

	Delay(int Samplerate, double Time, double Feedback, double Mix)
	{
		this->Samplerate = Samplerate;
		this->Time = Time;
		this->Feedback = Feedback;
		this->Mix = Mix;

		BufferLength = static_cast<int>(Time * Samplerate / 1000);
		Buffer.resize(BufferLength, 0.0f);
	}

	double Execute(double Input)
	{
		double DelayedSample = Buffer[Index];
		double Output = Input * (1.0f - Mix) + DelayedSample * Mix;
		Buffer[Index] = Input + DelayedSample * Feedback;
		Index = (Index + 1) % BufferLength;
		return Output;
	}
};
Delay TheDelay1(44100, 500, 0.6, 0.5);
Delay TheDelay2(44100, 375, 0.6, 0.5);

struct Box
{
	double R;
	double G;
	double B;

	double X;
	double Y;
	double W;
	double H;
	double Angle;
	double SinY;
	double LastSinY;
	double Fading;

	double Envelope1;
	double Envelope2;

	double EnvelopeStep1;
	double EnvelopeStep2;

	double Frequency1;
	double Frequency2;

	double Phase1;
	double Phase2;

	double FrequencyFactor1;
	double FrequencyFactor2;

	double Amplitude;
	double Output;

	int Direction;
};
vector<Box> BoxVector;

double Pitch2Frequency(int Pitch)
{
	return 440 * pow(2, 1.0 / 12.0 * (((Pitch / 12) - 2) * 12 + (Pitch % 12) - 45));
}
double Interpolate(double A, double B, double C)
{
	return (B - A) * C + B;
}
double GetSample()
{
	double Sample = 0;

	for (int I = 0; I < BoxVector.size(); I++)
	{
		BoxVector[I].Envelope1 -= BoxVector[I].EnvelopeStep1;
		BoxVector[I].Envelope2 -= BoxVector[I].EnvelopeStep2;

		if (BoxVector[I].Envelope1 < 0) { BoxVector[I].Envelope1 = 0; }
		if (BoxVector[I].Envelope2 < 0) { BoxVector[I].Envelope2 = 0; }

		double Output1 = sin(BoxVector[I].Phase1 * PI2) * BoxVector[I].Envelope1;
		double Output2 = sin(Output1 + BoxVector[I].Phase2 * PI2) * BoxVector[I].Envelope2;

		BoxVector[I].Output = Output2 * BoxVector[I].Amplitude;
		Sample += BoxVector[I].Output;

		BoxVector[I].Phase1 += BoxVector[I].FrequencyFactor1;
		BoxVector[I].Phase1 -= round(BoxVector[I].Phase1);

		BoxVector[I].Phase2 += BoxVector[I].FrequencyFactor2;
		BoxVector[I].Phase2 -= round(BoxVector[I].Phase2);
	}

	Sample /= BoxVector.size() / 2;
	return Sample;
}
int AudioCallback(void *OutputBuffer, void *InputBuffer, unsigned int BufferFrames, double StreamTime, RtAudioStreamStatus Status, void *UserData)
{
	double* SampleBuffer = reinterpret_cast<double*>(OutputBuffer);

	for (int I1 = 0; I1 < BufferSize; I1++)
	{
		double Sample = GetSample();

		for (int I2 = 0; I2 < Channels; I2++)
		{
			if (I2 == 0) { SampleBuffer[I1 * Channels + I2] = TheDelay1.Execute(Sample); }
			if (I2 == 1) { SampleBuffer[I1 * Channels + I2] = TheDelay2.Execute(Sample); }
		}
	}

	return 0;
}



int main()
{
	int Result = 0;



	if (AudioDevice == -1)
	{
		while (true)
		{
			Result = system("clear");
			cout << "Select Audio Device (-1 for exit)" << endl;

			int DeviceCount = TheRtAudio.getDeviceCount();
			for (int I = 0; I < DeviceCount; I++)
			{
				RtAudio::DeviceInfo TheDeviceInfo = TheRtAudio.getDeviceInfo(I);
				cout << I << ". " << TheDeviceInfo.name << endl;
			}

			cin >> AudioDevice;

			if (AudioDevice == -1) { return 0; }
			if (AudioDevice >= 0 && AudioDevice < DeviceCount) { break; }
		}
	}
	Result = system("clear");



	Result = SDL_Init(SDL_INIT_VIDEO);
	if (Result < 0) { throw runtime_error("Error: SDL_Init"); }

	SdlWindow = SDL_CreateWindow(Title.c_str(), SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, Width, Height, SDL_WINDOW_HIDDEN);
	if (SdlWindow == NULL) { throw runtime_error("Error: SDL_CreateWindow"); }

	SdlRenderer = SDL_CreateRenderer(SdlWindow, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
	if (SdlRenderer == NULL) { throw runtime_error("Error: SDL_CreateRenderer"); }



	RtAudio::StreamParameters TheStreamParameters;
	TheStreamParameters.deviceId = AudioDevice;
	TheStreamParameters.nChannels = Channels;
	TheStreamParameters.firstChannel = 0;

	RtAudio::StreamOptions TheStreamOptions;
	TheStreamOptions.flags = 0;
	TheStreamOptions.numberOfBuffers = NumBuffers;
	TheStreamOptions.priority = 99;

	TheRtAudio.openStream(&TheStreamParameters, nullptr, RTAUDIO_FLOAT64, Samplerate, &BufferSize, &AudioCallback, NULL, &TheStreamOptions);
	TheRtAudio.startStream();



	double BW = 1.0 / BoxCount;

	for (int I = 0; I < BoxCount; I++)
	{
		double F1 = I / (double)BoxCount;
		double F2 = I / (double)(BoxCount + 1);

		Box NewBox;
		NewBox.X = F1 * Width;
		NewBox.Y = CY - (BW * Width / 2);
		NewBox.W = BW * Width;
		NewBox.H = NewBox.W;
		NewBox.Angle = 0;
		NewBox.SinY = 0;
		NewBox.LastSinY = 0;
		NewBox.Fading = 0;
		NewBox.Output = 0;
		NewBox.Envelope1 = 0;
		NewBox.Envelope2 = 0;
		NewBox.EnvelopeStep1 = 1.0 / 4410;
		NewBox.EnvelopeStep2 = 1.0 / 44100;
		NewBox.Amplitude = (1 - F1);
		NewBox.Direction = 0;
		NewBox.R = 0;
		NewBox.G = 0;
		NewBox.B = 0;

		int Pitch = (I / 5) * 12 + 36;

		if (I % 5 == 0) { Pitch += 0; }
		if (I % 5 == 1) { Pitch += 3; }
		if (I % 5 == 2) { Pitch += 5; }
		if (I % 5 == 3) { Pitch += 7; }
		if (I % 5 == 4) { Pitch += 10; }

		NewBox.Frequency1 = Pitch2Frequency(Pitch + 24);
		NewBox.Frequency2 = Pitch2Frequency(Pitch);

		NewBox.Phase1 = 0;
		NewBox.Phase2 = 0;

		NewBox.FrequencyFactor1 = NewBox.Frequency1 / 44100;
		NewBox.FrequencyFactor2 = NewBox.Frequency2 / 44100;

		BoxVector.push_back(NewBox);
	}



	int Frame = 0;
	bool IsRunning = true;

	SDL_ShowWindow(SdlWindow);

	while (IsRunning)
	{
		while (SDL_PollEvent(&SdlEvent))
		{
			if (SdlEvent.type == SDL_QUIT) { IsRunning = false; }
		}

		for (int I = 0; I < BoxCount; I++)
		{
			float Factor = (float)I / BoxCount;
			float FrameFactor = (float)(Frame % 500) / 500;
			Factor += FrameFactor;

			Factor = fmod(Factor, 1.0);

			int Value1 = (int)(Factor * 6);
			float Value2 = fmod(Factor * 6, 1.0);

			float R = 0;
			float G = 0;
			float B = 0;

			if (Value1 == 0) { R = 1; G = Value2; B = 0; }
			else if (Value1 == 1) { R = 1 - Value2; G = 1; B = 0; }
			else if (Value1 == 2) { R = 0; G = 1; B = Value2; }
			else if (Value1 == 3) { R = 0; G = 1 - Value2; B = 1; }
			else if (Value1 == 4) { R = Value2; G = 0; B = 1; }
			else if (Value1 == 5) { R = 1; G = 0; B = 1 - Value2; }

			BoxVector[I].R = R;
			BoxVector[I].G = G;
			BoxVector[I].B = B;

			double F = I / (double)(BoxCount - 1);
			BoxVector[I].LastSinY = BoxVector[I].SinY;
			
			BoxVector[I].SinY = sin(sin(BoxVector[I].Angle * D2R) + BoxVector[I].Angle * D2R);
			BoxVector[I].Y = (CY - BoxVector[I].W / 2) + BoxVector[I].SinY * (CY - BoxVector[I].W / 2);
			BoxVector[I].Angle += Interpolate(0.9, 1.1, 1 - F);
			BoxVector[I].Fading *= 0.95;

			bool Reset = false;
			if (BoxVector[I].Direction == 0 && BoxVector[I].LastSinY >= 0 && BoxVector[I].SinY < 0) { Reset = true; }
			if (BoxVector[I].Direction == 1 && BoxVector[I].LastSinY < 0 && BoxVector[I].SinY >= 0) { Reset = true; }

			if (Reset)
			{
				BoxVector[I].Fading = 1;

				BoxVector[I].Phase1 = 0;
				BoxVector[I].Phase2 = 0;

				BoxVector[I].Envelope1 = 1;
				BoxVector[I].Envelope2 = 1;

				BoxVector[I].Direction++;
				BoxVector[I].Direction %= 2;
			}
		}

		SDL_SetRenderDrawColor(SdlRenderer, 0, 0, 0, 255);
		SDL_RenderClear(SdlRenderer);

		for (int I = 0; I < BoxCount; I++)
		{
			Uint8 R = BoxVector[I].R * BoxVector[I].Fading * 255;
			Uint8 G = BoxVector[I].G * BoxVector[I].Fading * 255;
			Uint8 B = BoxVector[I].B * BoxVector[I].Fading * 255;

			SdlRect.x = BoxVector[I].X;
			SdlRect.y = BoxVector[I].Y;
			SdlRect.w = BoxVector[I].W;
			SdlRect.h = BoxVector[I].H;
			SDL_SetRenderDrawColor(SdlRenderer, R, G, B, 255);
			SDL_RenderFillRect(SdlRenderer, &SdlRect);

			SdlRect.x = BoxVector[I].X;
			SdlRect.y = BoxVector[I].Y;
			SdlRect.w = BoxVector[I].W;
			SdlRect.h = BoxVector[I].H;
			SDL_SetRenderDrawColor(SdlRenderer, 255, 255, 255, 255);
			SDL_RenderDrawRect(SdlRenderer, &SdlRect);
		}

		SDL_SetRenderDrawColor(SdlRenderer, 255, 255, 255, 255);
		SDL_RenderDrawLine(SdlRenderer, 0, CY, Width - 1, CY);

		SDL_RenderPresent(SdlRenderer);
		Frame++;
	}



	TheRtAudio.stopStream();
	TheRtAudio.closeStream();

	SDL_DestroyRenderer(SdlRenderer);
	SDL_DestroyWindow(SdlWindow);
	SDL_Quit();

	return 0;
}
