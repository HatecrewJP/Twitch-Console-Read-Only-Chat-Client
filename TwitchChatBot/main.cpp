#include "math.h"
#include <intrin.h>
#include "ws2tcpip.h"
#include "stdio.h"
#include "malloc.h"
#include "consoleapi2.h"



#ifdef _DEBUG
#define Assert(x) if(!(x)) __debugbreak();
#else
#define Assert(x)
#endif



#define global static
#define internal static

#define DEBUG_FAST_POW 1
#define MAX_CONCURRENT_CHANNELS 64
#define MAX_LENGTH 500
#define MAX_COMMAND_LENGTH 20
#define MAX_USERNAME_LENGTH 25


struct RGB{
	unsigned char R;
	unsigned char G;
	unsigned char B;
};

struct Slice{
	char *Ptr;
	size_t Length;
};

enum FORMAT_RESULT{
	FORMAT_PLACEHOLDER,
	FORMAT_SUCCESS,
	FORMAT_PING,
	FORMAT_NON_MESSAGE,
	FORMAT_OUT_OF_BOUNDS,
	FORMAT_UNEXPECTED_CHAR,
	FORMAT_TOO_LONG_MESSAGE_TYPE,
	FORMAT_BUFFER_IN_SIZE_ZERO,
	FORMAT_BUFFER_OUT_SIZE_ZERO,
	FORMAT_BUFFER_IN_SIZE_NEGATIVE,
	FORMAT_BUFFER_OUT_SIZE_NEGATIVE,
	FORMAT_BUFFER_IN_NULL,
	FORMAT_BUFFER_OUT_NULL,
	FORMAT_MESSAGE_TRUNCATED,
	FORMAT_OUTPUT_OUT_OF_MEMORY,
	FORMAT_COMMAND,
	FORMAT_JOIN_RESPONSE,
	FORMAT_BUFFER_IN_TOO_SMALL,
	FORMAT_BUFFER_OUT_TOO_SMALL,
	FORMAT_NO_MESSAGE,

	FORMAT_ERROR_COUNT
};

global char *CurrentChannels[MAX_CONCURRENT_CHANNELS] = {};
global int CurrentChannelCount = 0;

global RGB DefaultUniformChannelColor = {255,125,125};
global RGB DefaultUniformUserColor = {255,0,125};
global RGB UniformChannelColor = DefaultUniformChannelColor;
global RGB UniformUserColor = DefaultUniformUserColor;

global RGB Color1 = {200,20,20};
global RGB Color2 = {255,120,255};

global SOCKET Socket = {};
global bool IsUniformColors = 1;

static int GlobalRunning = 1;







internal FORMAT_RESULT FormatTwitchUserMessage(char *BufferIn, int BufferInSize, char *BufferOut, int BufferOutSize){
	if(BufferIn == NULL){
		return FORMAT_BUFFER_IN_NULL;
	}
	if(BufferOut == NULL){
		return FORMAT_BUFFER_OUT_NULL;
	}
	if(BufferOutSize == 0){
		return FORMAT_BUFFER_OUT_SIZE_ZERO;
	}
	if(BufferInSize == 0){
		return FORMAT_BUFFER_IN_SIZE_ZERO;
	}
	if(BufferInSize < 0){
		return FORMAT_BUFFER_IN_SIZE_NEGATIVE;
	}
	if(BufferOutSize < 0){
		return FORMAT_BUFFER_OUT_SIZE_NEGATIVE;
	}
	if(BufferInSize > BufferOutSize){
		return FORMAT_BUFFER_IN_TOO_SMALL;
	}

	char *BufferInEnd = BufferIn + BufferInSize;
	char *BufferInRef = BufferIn;
	char *BufferOutRef = BufferOut;
	char *BufferOutEnd = BufferOut + BufferOutSize;

	if (BufferInSize < 4) {
		return FORMAT_BUFFER_IN_TOO_SMALL;
	}
	while (*BufferInRef != '\0') {
		bool IsPing = !strncmp(BufferInRef, "PING", 4);
		if(IsPing){
			//Output: "PONG <text in>\r\n"
			memcpy(BufferOut, "PONG", 4);
			BufferInRef += 4;
			//start writing after "PONG"
			int i = 4;
			//copy <text in>
			//6 = 4(PONG) + 2(\r\n)
			while((i < BufferOutSize - 6) && (*BufferInRef != '\r') && BufferInRef < BufferInEnd){
				BufferOut[i] = *BufferInRef;
				BufferInRef++;
				i++;
			}
			if(BufferInRef >= BufferInEnd){
				return FORMAT_OUT_OF_BOUNDS;
			}
			if((*BufferInRef != '\r') && (i > (BufferOutSize - 6))){
				return FORMAT_OUTPUT_OUT_OF_MEMORY;
			}
			Assert(strncmp(BufferInRef, "\r\n", 2) == 0);
			memcpy(&BufferOut[i], "\r\n", 2);
			BufferInRef += 2;
			return FORMAT_PING;
		}

		if(*BufferInRef == ':'){
			while(*BufferInRef == ':'){
				///////////////////
				//Extract Message//
				///////////////////
				BufferInRef++;
				Slice UserName;
				UserName.Ptr = BufferInRef;
				unsigned UserColorCalc = (unsigned)((*(unsigned*)BufferInRef) & 0x00ffffff);
				while(*BufferInRef != '!' && *BufferInRef != '.' && BufferInRef < BufferInEnd){
					BufferInRef++;
				}
				if(BufferInRef >= BufferInEnd){
					return FORMAT_OUT_OF_BOUNDS;
				}
				if(*BufferInRef == '.'){
					while(*BufferInRef != '\n' && BufferInRef < BufferInEnd){
						BufferInRef++;
					}
					if(BufferInRef >= BufferInEnd){
						return FORMAT_OUT_OF_BOUNDS;
					}
					Assert(*(BufferInRef-1) == '\r' && *BufferInRef == '\n');
					BufferInRef++;
					if(BufferInRef >= BufferInEnd){
						return FORMAT_OUT_OF_BOUNDS;
					}
					return FORMAT_NON_MESSAGE;
				}
				UserName.Length = BufferInRef - UserName.Ptr;
				

				unsigned char CalcUserR = (UserColorCalc >> 16 & 0xff);
				unsigned char CalcUserG = (UserColorCalc >> 8  & 0xff);
				unsigned char CalcUserB = (UserColorCalc	   & 0xff);
				
				float SpanR = (float) fabs((float)(Color1.R - Color2.R));
				float SpanG = (float) fabs((float)(Color1.G - Color2.G));
				float SpanB = (float) fabs((float)(Color1.B - Color2.B));

				float StepSizeR = SpanR / 27.0f;
				float StepSizeG = SpanG / 27.0f;
				float StepSizeB = SpanB / 27.0f;

				RGB UserColorRGB;
				UserColorRGB.R = (unsigned char) (StepSizeR * (float)('z' - CalcUserR) + min(Color1.R, Color2.R));
				UserColorRGB.G = (unsigned char) (StepSizeG * (float)('z' - CalcUserG) + min(Color1.G, Color2.G));
				UserColorRGB.B = (unsigned char) (StepSizeB * (float)('z' - CalcUserB) + min(Color1.B, Color2.B));

				
				while(*BufferInRef != ' ' && BufferInRef < BufferInEnd){
					BufferInRef++;
				}
				if(BufferInRef >= BufferInEnd){
					return FORMAT_OUT_OF_BOUNDS;
				}
				Assert(*BufferInRef == ' ');
				BufferInRef++;
				if(BufferInRef >= BufferInEnd){
					return FORMAT_OUT_OF_BOUNDS;
				}

				Slice MessageType;
				MessageType.Ptr = BufferInRef;
				while (*BufferInRef != ' ' && BufferInRef < BufferInEnd) {
					BufferInRef++;
				}

				if(BufferInRef >= BufferInEnd){
					return FORMAT_OUT_OF_BOUNDS;
				}
				MessageType.Length = BufferInRef - MessageType.Ptr;
				BufferInRef++;
				if(BufferInRef >= BufferInEnd){
					return FORMAT_OUT_OF_BOUNDS;
				}
				if(strncmp(MessageType.Ptr, "PRIVMSG",7) == 0){
					Assert(*BufferInRef == '#');
					BufferInRef++;
					if(BufferInRef >= BufferInEnd){
						return FORMAT_OUT_OF_BOUNDS;
					}
					Slice ChannelName;
					ChannelName.Ptr = BufferInRef;
					unsigned ChannelColorCalc = *(unsigned*)BufferInRef;
					while(*BufferInRef != ':' && BufferInRef < BufferInEnd){
						
						BufferInRef++;
					}
					if(BufferInRef >= BufferInEnd){
						return FORMAT_OUT_OF_BOUNDS;
					}
					float CalcChannelR = (float)(ChannelColorCalc >> 16 & 0xff);
					float CalcChannelG = (float)(ChannelColorCalc >> 8 & 0xff);
					float CalcChannelB = (float)(ChannelColorCalc & 0xff);

					RGB ChannelColorRGB;
					ChannelColorRGB.R = (unsigned char)(StepSizeR * (float)('z' - CalcChannelR) + min(Color1.R, Color2.R));
					ChannelColorRGB.G = (unsigned char)(StepSizeG * (float)('z' - CalcChannelG) + min(Color1.G, Color2.G));
					ChannelColorRGB.B = (unsigned char)(StepSizeB * (float)('z' - CalcChannelB) + min(Color1.B, Color2.B));

					Assert(*BufferInRef == ':');
					ChannelName.Length = BufferInRef - ChannelName.Ptr - 1;

					BufferInRef++;
					if (BufferInRef >= BufferInEnd) {
						return FORMAT_OUT_OF_BOUNDS;
					}

					Slice UserMessage;
					UserMessage.Ptr = BufferInRef;
					while (*BufferInRef != '\r' && BufferInRef < BufferInEnd) {
						BufferInRef++;
					}
					if (BufferInRef >= BufferInEnd) {
						return FORMAT_OUT_OF_BOUNDS;
					}
					Assert(*BufferInRef == '\r' && *(BufferInRef + 1) == '\n');
					UserMessage.Length = BufferInRef - UserMessage.Ptr;
					BufferInRef += 2;
					if (BufferInRef >= BufferInEnd) {
						return FORMAT_OUT_OF_BOUNDS;
					}
					//////////////////
					//Copy To Output//
					//////////////////
#define	SAFETY_PADDING 4
					char EscapeChannelColor[23] = {};
					int EscapeChannelColorCount = 0;
					char EscapeUserColor[23] = {};
					int EscapeUserColorCount = 0;

					const char EscapeClearColor[8] = "\033[0m";
					int EscapeClearColorCount = (int) strlen(EscapeClearColor);
					if(IsUniformColors){
						snprintf(EscapeChannelColor,23, "\033[38;2;%hhu;%hhu;%hhum", UniformChannelColor.R, UniformChannelColor.G, UniformChannelColor.B);
						EscapeChannelColorCount = (int) strlen(EscapeChannelColor);

						snprintf(EscapeUserColor, 23, "\033[38;2;%hhu;%hhu;%hhum", UniformUserColor.R, UniformUserColor.G, UniformUserColor.B);
						EscapeUserColorCount = (int) strlen(EscapeUserColor);
					}
					else{
						snprintf(EscapeChannelColor, 23, "\033[38;2;%hhu;%hhu;%hhum", ChannelColorRGB.R, ChannelColorRGB.G, ChannelColorRGB.B);
						EscapeChannelColorCount = (int)strlen(EscapeChannelColor);

						snprintf(EscapeUserColor, 23, "\033[38;2;%hhu;%hhu;%hhum", UserColorRGB.R, UserColorRGB.G, UserColorRGB.B);
						EscapeUserColorCount = (int)strlen(EscapeUserColor);
					}
					Assert(EscapeChannelColorCount > 0);
					Assert(EscapeUserColorCount > 0);
					
					size_t ExpectedSize = EscapeChannelColorCount + ChannelName.Length + sizeof(':')
						+ EscapeUserColorCount + UserName.Length
						+ EscapeClearColorCount + sizeof(':')
						+ UserMessage.Length + sizeof('\n')
						+ SAFETY_PADDING;

					unsigned long BufferOutSizeFree = (unsigned long)(BufferOutEnd - BufferOutRef);

					if(BufferOutSizeFree < ExpectedSize){
						return FORMAT_OUTPUT_OUT_OF_MEMORY;
					}
					memcpy(BufferOutRef, EscapeChannelColor, EscapeChannelColorCount);
					BufferOutRef += EscapeChannelColorCount;
					Assert(BufferOutRef < BufferOutEnd);

					memcpy(BufferOutRef, ChannelName.Ptr, ChannelName.Length);
					BufferOutRef += ChannelName.Length;
					Assert(BufferOutRef < BufferOutEnd);

					*BufferOutRef = ':';
					BufferOutRef++;
					Assert(BufferOutRef < BufferOutEnd);

					memcpy(BufferOutRef, EscapeUserColor, EscapeUserColorCount);
					BufferOutRef += EscapeUserColorCount;
					Assert(BufferOutRef < BufferOutEnd);

					memcpy(BufferOutRef, UserName.Ptr, UserName.Length);
					BufferOutRef += UserName.Length;
					Assert(BufferOutRef < BufferOutEnd);

					memcpy(BufferOutRef, EscapeClearColor, EscapeClearColorCount);
					BufferOutRef += EscapeClearColorCount;
					Assert(BufferOutRef < BufferOutEnd);

					*BufferOutRef = ':';
					BufferOutRef++;
					Assert(BufferOutRef < BufferOutEnd);

					memcpy(BufferOutRef, UserMessage.Ptr, UserMessage.Length);
					BufferOutRef += UserMessage.Length;
					Assert(BufferOutRef < BufferOutEnd);

					*BufferOutRef = '\n';
					BufferOutRef++;
					Assert(BufferOutRef < BufferOutEnd);

				}
				else if (strncmp(MessageType.Ptr, "JOIN", 4) == 0) {
					Assert(*BufferInRef == '#');
					BufferInRef++;
					if (BufferInRef >= BufferInEnd) {
						return FORMAT_OUT_OF_BOUNDS;
					}
					Slice JoinedChannel;
					JoinedChannel.Ptr = BufferInRef;
					while (*BufferInRef != '\n' && BufferInRef < BufferInEnd) {
						BufferInRef++;
					}
					if (BufferInRef >= BufferInEnd) {
						return FORMAT_OUT_OF_BOUNDS;
					}
					Assert(*(BufferInRef - 1) == '\r' && *BufferInRef == '\n');
					JoinedChannel.Length = BufferInRef - JoinedChannel.Ptr - 1;
					BufferInRef++;
					if (BufferInRef >= BufferInEnd) {
						return FORMAT_OUT_OF_BOUNDS;
					}

					printf("\033[2KYou joined %.*s\n", (int)JoinedChannel.Length, JoinedChannel.Ptr);
					int Index = 0;
					while (CurrentChannels[Index] != NULL) {
						Index++;
					}
					Assert(Index < MAX_CONCURRENT_CHANNELS);
					CurrentChannels[Index] = (char*)malloc(JoinedChannel.Length + 1);
					Assert(CurrentChannels[Index] != NULL);
					memcpy(*(CurrentChannels + Index), JoinedChannel.Ptr, JoinedChannel.Length);
					(*(CurrentChannels + Index))[JoinedChannel.Length] = '\0';
					CurrentChannelCount++;


				}
				else {
					return FORMAT_NON_MESSAGE;
				}
			}
			
		}
		else {
			return FORMAT_NON_MESSAGE;
		}

	}
	Assert(*BufferInRef == '\0');
	return FORMAT_SUCCESS;

}

internal int ReceiveMessage(SOCKET S ,char *Buffer, int BufferSize){
	memset(Buffer, 0, BufferSize);
	int Result = recv(S, Buffer, BufferSize, 0);
	if(Result == SOCKET_ERROR){
		int Error = WSAGetLastError();
		printf("Error: %d\n", Error);
	}
	return Result;
}

internal void LogTwitchMessage( char *Buffer){
	printf("%s", Buffer);
	
}

internal unsigned long long MeasurePowFunction(double (*func)(double, double), double base, double exp, double *Result){
	unsigned long long Start, End;
	for(int i = 0; i < 10000000; i++) continue;
	_mm_lfence();
	Start = __rdtsc();
	double Res = func(base, exp);
	End = __rdtsc();
	unsigned long long CycleCount = End - Start;
	*Result = Res;
	return CycleCount;
}
internal unsigned long long MeasureIntPowFunction(int (*func)(int, int), int base, int exp, int *Result){
	unsigned long long Start, End;
	for(int i = 0; i < 10000000; i++) continue;
	_mm_lfence();
	Start = __rdtsc();
	int Res = func(base, exp);
	End = __rdtsc();
	unsigned long long CycleCount = End - Start;
	*Result = Res;
	return CycleCount;
}

internal int IsInArray(char* *StringArray, int ArrayFillCount, char *ToFind,int ToFindSize){
	if(ArrayFillCount <= 0){
		return -1;
	}
	for(int i = 0; i < MAX_CONCURRENT_CHANNELS; i++){
		char *CurrentString = *(StringArray + i);
		if((int)strlen(CurrentString) == ToFindSize){
			if(strncmp(CurrentString, ToFind, ToFindSize)==0){
				return i;
			}
		}
	}
	return -1;
}

internal int IntPow(int x, int exp){
	int Result = 1;
	for(int i = 0; i < exp; i++){
		Result *= x;
	}
	return Result;
}

internal int IsSliceRGBValue(Slice Slice){ 
	int Sum = 0;
	for(int i = 0; i < (int) Slice.Length; i++){
		char CurrentChar = Slice.Ptr[(Slice.Length-1) - i];
		if(!('0' <= CurrentChar && CurrentChar <= '9')){
			printf("Wrong color format. To change the color use:\"/setcolor <red>;<green>;<blue>;\", where each color is a number between 0 and 255\n");
			return -1;
		}
		int CharValue = CurrentChar - '0';
		
#ifdef _DEBUG
#if DEBUG_FAST_POW
		int tmp = 0;
		unsigned long long CycleCount = MeasureIntPowFunction(&IntPow, 10, i, &tmp);
		Sum += tmp * CharValue;
#else
		double tmp = 0;
		unsigned long long CycleCount = MeasurePowFunction(&pow, 10.0f, (double) i, &tmp);
		Sum += (int)tmp * CharValue;
#endif
		char tmpBuffer[4096] = {};
		snprintf(tmpBuffer,4096,"%llu\n",CycleCount);
		OutputDebugStringA(tmpBuffer);
#else
		Sum += CharValue * IntPow(10, i);
#endif

		
	}
	Assert(Sum == atol(Slice.Ptr));
	if(Sum > 255){
		printf("Your number %d is too big. Only numbers between 0 and 255 are allowed.\n", Sum);
		return -2;
	}
	return Sum;

}

internal int SetColorStructFromString(RGB *ColorStruct, char *Array){
	char *InputArrayRef = Array;
	
	Slice StringRed;
	StringRed.Ptr = InputArrayRef;

	int i = 0;
	while(*InputArrayRef != ';' && i < 4){
		InputArrayRef++;
		i++;
	}
	if(i >= 4 && *InputArrayRef != ';'){
		return -2;
	}
	Assert(*InputArrayRef == ';');
	InputArrayRef++;
	StringRed.Length = i;

	Slice StringGreen;
	StringGreen.Ptr = InputArrayRef;
	i = 0;
	while(*InputArrayRef != ';' && i < 4){
		InputArrayRef++;
		i++;
	}
	if(i >= 4 && *InputArrayRef != ';'){
		return -2;
	}
	Assert(*InputArrayRef == ';');
	StringGreen.Length = i;
	InputArrayRef++;

	Slice StringBlue;
	StringBlue.Ptr = InputArrayRef;
	i = 0;
	while(*InputArrayRef != ';' && i < 4){
		InputArrayRef++;
		i++;
	}
	if(i >= 4 && *InputArrayRef != ';'){
		return -2;
	}
	Assert(*InputArrayRef == ';');
	StringBlue.Length = i;
	InputArrayRef++;


	int ValueRed = IsSliceRGBValue(StringRed);
	if(ValueRed == -1) return -1;
	int ValueGreen = IsSliceRGBValue(StringGreen);
	if(ValueGreen == -1) return -1;
	int ValueBlue = IsSliceRGBValue(StringBlue);
	if(ValueBlue == -1) return -1;
	if(ValueRed < -1 || ValueGreen < -1 || ValueBlue < -1){
		return -1;
	}
	ColorStruct->R = (unsigned char)ValueRed;
	ColorStruct->G = (unsigned char)ValueGreen;
	ColorStruct->B = (unsigned char)ValueBlue;
	return (int)(InputArrayRef - Array);
}

DWORD WINAPI ThreadProc(
	void* lpParameter
){
	char InputArray[MAX_LENGTH+1];
	printf("You are ready to join a chat. If you need help use /help.\n");
	
	while(GlobalRunning){
		memset(InputArray, 0, MAX_LENGTH+1);
		fgets(InputArray, MAX_LENGTH + 1, stdin);

		if(InputArray[0] == '/'){
			//////////////////
			//command: /join//
			//////////////////
			if(strncmp(InputArray, "/join ",6) == 0){
				if(CurrentChannelCount > MAX_CONCURRENT_CHANNELS){
					printf("You reached the limit of channels you are able to join.\n");
					continue;
				}
				int ChannelLength = (int)strlen(InputArray + 6);
				char ChannelName[MAX_USERNAME_LENGTH + 1] = {};
				if(4 <= (ChannelLength-1) && ChannelLength-1 < MAX_USERNAME_LENGTH+1){
					memcpy(ChannelName, (InputArray + 6), ChannelLength);
					if(ChannelName[ChannelLength - 1] == '\n'){
						ChannelName[ChannelLength - 1] = '\0';
						for(int i = 0; i < ChannelLength - 1; i++){
							ChannelName[i] = (char)tolower(ChannelName[i]);
						}
						char JoinMessage[10 + MAX_USERNAME_LENGTH] = {};
						sprintf(JoinMessage, "JOIN #%s\r\n", ChannelName);
						send(Socket, JoinMessage, (int)strlen(JoinMessage), 0);
					} else{
						printf("Channel Name max 25 characters.\n");
					}
				} else{
					printf("Channel Name needs at least 4 and at most 25 characters.\n");
				}
			}
			//////////////////////
			//command: /leaveall//
			//////////////////////
			else if(strcmp(InputArray, "/leaveall\n") == 0){
				if(CurrentChannelCount <= 0){
					printf("You didn't join any chat yet.\n");
					continue;
				}
				int i = 0;
				char **CurrentChannelsRef = CurrentChannels;
				while(i < CurrentChannelCount){
					if((*CurrentChannelsRef) != NULL){

						char PartMessage[10 + MAX_USERNAME_LENGTH] = {};
						sprintf(PartMessage, "PART #%s\r\n", *CurrentChannelsRef);
						send(Socket, PartMessage, (int)strlen(PartMessage), 0);

						free(*CurrentChannelsRef);
						*CurrentChannelsRef = NULL;
						i++;
					}
					CurrentChannelsRef++;
					Assert(CurrentChannelsRef <= CurrentChannels + MAX_CONCURRENT_CHANNELS);
				}
				CurrentChannelCount = 0;
				printf("You left all channels.\n");
			}
			//////////////////////
			//command: /leave   //
			//////////////////////
			else if(strncmp(InputArray, "/leave ", 7)==0){
				int ChannelLength = (int)strlen(InputArray + 7);
				char ChannelName[MAX_USERNAME_LENGTH + 1] = {};
				if(4 <= (ChannelLength-1) &&ChannelLength-1 < MAX_USERNAME_LENGTH+1){
					memcpy(ChannelName, (InputArray + 7), ChannelLength);
						if(ChannelName[ChannelLength - 1] == '\n'){
							ChannelName[ChannelLength - 1] = '\0';
							for(int i = 0; i < ChannelLength - 1; i++){
								ChannelName[i] = (char)tolower(ChannelName[i]);
							}
							int Index = IsInArray(CurrentChannels, CurrentChannelCount, ChannelName, ChannelLength-1);
							if(Index >= 0){
								free(CurrentChannels[Index]);
								CurrentChannels[Index] = NULL;
								CurrentChannelCount--;

								char PartMessage[10 + MAX_USERNAME_LENGTH] = {};
								sprintf(PartMessage, "PART #%s\r\n", ChannelName);
								send(Socket, PartMessage, (int)strlen(PartMessage), 0);
								printf("You left %s\n", ChannelName);
							} else{
								printf("You aren't connected to %s.\n", ChannelName);
							}

						} else{
							printf("Channel Name max 25 characters.\n");
						}
				} else{
					printf("Channel Name max 25 characters.\n");
				}
			}

			//////////////////////
			//command: /list    //
			//////////////////////
			else if(strcmp(InputArray, "/list\n")==0){
				if(CurrentChannelCount <= 0){
					printf("You didn't join any chat yet.\n");
					continue;
				}
				int i = 0;
				char* *CurrentChannelsRef = CurrentChannels;
				printf("\033[2KYou joined:\n");
				while(i < CurrentChannelCount){
					if((*CurrentChannelsRef)!= NULL){
						i++;
						printf("\033[2K%s\n", *CurrentChannelsRef);
					}
					CurrentChannelsRef++;
				}
			}

			//////////////////////////
			//command: /clear       //
			//////////////////////////
			else if(strcmp(InputArray, "/clear\n")==0){
				printf("\033[2J\033[H");
			}

			//////////////////////////
			//command: /setcolormode//
			//////////////////////////
			else if(strncmp(InputArray, "/setcolormode ",14)==0){
				if(strcmp(InputArray + 14,"0\n") == 0|| strcmp(InputArray + 14, "uniform\n") == 0){
					IsUniformColors = 1;
					printf("Color mode: Uniform\n");
				}

				else if(strcmp(InputArray + 14, "1\n") == 0 || strcmp(InputArray + 14, "rgb\n") == 0){
					IsUniformColors = 0;
					printf("Color mode: RGB\n");
				}
				else{
					printf("Unknown color mode.\n");
				}

			}
			/////////////////////////////
			//command: /setcolor       //
			/////////////////////////////
			else if(strncmp(InputArray, "/setcolor ",10)==0){
				char *InputArrayRef = InputArray + 10;
				if(strncmp(InputArrayRef, "-c ", 3) == 0){
					InputArrayRef += 3;
					int Result = SetColorStructFromString(&UniformChannelColor, InputArrayRef);
					if(Result == -2){
						printf("Wrong format. To change the color use /setcolor [-c,-u] <red>;<green>;<blue>;\n");
					}
					if(Result <= 0){
						continue;
					}
					InputArrayRef += Result;
					if(*InputArrayRef == ' '){
						InputArrayRef++;
					}
				}
				else if(strncmp(InputArrayRef, "-u ", 3) == 0){
					InputArrayRef += 3;
					int Result = SetColorStructFromString(&UniformUserColor, InputArrayRef);
					if(Result == -2){
						printf("Wrong format. To change the color use /setcolor [-c,-u] <red>;<green>;<blue>;\n");
						continue;
					}
					if(Result <= 0){
						continue;
					}
					InputArrayRef += Result;
					if(*InputArrayRef == ' '){
						InputArrayRef++;
					}
				}
				else{
					printf("Wrong format. To change the color use /setcolor [-c,-u] <red>;<green>;<blue>;\n");
					continue;
				}
				if(*InputArrayRef == ' '){
					InputArrayRef++;
				}

				if(strncmp(InputArrayRef, "-c ", 3) == 0){
					InputArrayRef += 3;
					int Result = SetColorStructFromString(&UniformChannelColor, InputArrayRef);
					if(Result == -2){
						printf("Wrong format. To change the color use /setcolor [-c,-u] <red>;<green>;<blue>;\n");
					}
					if(Result <= 0){
						continue;
					}
					InputArrayRef += Result;
					if(*InputArrayRef == ' '){
						InputArrayRef++;
					}
				} else if(strncmp(InputArrayRef, "-u ", 3) == 0){
					InputArrayRef += 3;
					int Result = SetColorStructFromString(&UniformUserColor, InputArrayRef);
					if(Result == -2){
						printf("Wrong format. To change the color use /setcolor [-c,-u] <red>;<green>;<blue>;\n");
						continue;
					}
					if(Result <= 0){
						continue;
					}
					InputArrayRef += Result;
					if(*InputArrayRef == ' '){
						InputArrayRef++;
					}
					else{
						printf("Wrong format. To change the color use /setcolor [-c,-u] <red>;<green>;<blue>;\n");
						continue;
					}
				}
			}
			/////////////////////////////
			//command: /setchannelcolor//
			/////////////////////////////
			else if(strncmp(InputArray, "/setchannelcolor ", 17)==0){
				if(strcmp(InputArray + 17, "-d\n") == 0){
					UniformChannelColor = DefaultUniformChannelColor;
					continue;
				}
				char *InputArrayRef = InputArray + 17;
				int Result = SetColorStructFromString(&UniformChannelColor, InputArrayRef);
				if(Result == -2){
					printf("Wrong format. To change the color use:\"/setchannelcolor <red>;<green>;<blue>;\", where each color is a number between 0 and 255\n");
				}
			}
			/////////////////////////////
			//command: /setusercolor   //
			/////////////////////////////
			else if(strncmp(InputArray, "/setusercolor ", 14)==0){
				if(strcmp(InputArray + 14, "-d\n") == 0){
					UniformUserColor = DefaultUniformUserColor;
					continue;
				}
				char *InputArrayRef = InputArray + 14;
				int Result = SetColorStructFromString(&UniformUserColor, InputArrayRef);
				if(Result == -2){
					printf("Wrong format. To change the color use:\"/setusercolor <red>;<green>;<blue>;\", where each color is a number between 0 and 255\n");
				}
			}
			/////////////////////////////
			//command: /setrgbcolor    //
			/////////////////////////////
			else if(strncmp(InputArray, "/setrgbcolor ", 13) == 0){
				char *InputArrayRef = InputArray + 13;
				if(strncmp(InputArrayRef, "-a ", 3) == 0){
					InputArrayRef += 3;
					int Result = SetColorStructFromString(&Color1, InputArrayRef);
					if(Result == -2){
						printf("Wrong format. To change the color use /setrgbcolor [-a,-b] <red>;<green>;<blue>;\n");
					}
					if(Result <= 0){
						continue;
					}
					InputArrayRef += Result;
					if(*InputArrayRef == ' '){
						InputArrayRef++;
					}
				} else if(strncmp(InputArrayRef, "-b ", 3) == 0){
					InputArrayRef += 3;
					int Result = SetColorStructFromString(&Color2, InputArrayRef);
					if(Result == -2){
						printf("Wrong format. To change the color use /setrgbcolor [-a,-b] <red>;<green>;<blue>;\n");
						continue;
					}
					if(Result <= 0){
						continue;
					}
					InputArrayRef += Result;
					if(*InputArrayRef == ' '){
						InputArrayRef++;
					}
				} else{
					printf("Wrong format. To change the color use /setrgbcolor [-a,-b] <red>;<green>;<blue>;\n");
					continue;
				}
				if(*InputArrayRef == ' '){
					InputArrayRef++;
				}

				if(strncmp(InputArrayRef, "-b ", 3) == 0){
					InputArrayRef += 3;
					int Result = SetColorStructFromString(&Color2, InputArrayRef);
					if(Result == -2){
						printf("Wrong format. To change the color use /setrgbcolor [-a,-b] <red>;<green>;<blue>;\n");
					}
					if(Result <= 0){
						continue;
					}
					InputArrayRef += Result;
					if(*InputArrayRef == ' '){
						InputArrayRef++;
					}
				} else if(strncmp(InputArrayRef, "-a ", 3) == 0){
					InputArrayRef += 3;
					int Result = SetColorStructFromString(&Color1, InputArrayRef);
					if(Result == -2){
						printf("Wrong format. To change the color use /setrgbcolor [-a,-b] <red>;<green>;<blue>;\n");
						continue;
					}
					if(Result <= 0){
						continue;
					}
					InputArrayRef += Result;
					if(*InputArrayRef == ' '){
						InputArrayRef++;
					} else{
						printf("Wrong format. To change the color use /setrgbcolor [-a,-b] <red>;<green>;<blue>;\n");
						continue;
					}
				}
			}
			/////////////////////////////
			//command: /quit		   //
			/////////////////////////////
			else if(strcmp(InputArray, "/quit\n") == 0 || strcmp(InputArray, "/q\n") == 0){
				GlobalRunning = 0;
				return 0;
			}
			/////////////////////////////
			//command: /help		   //
			/////////////////////////////
			else if(strcmp(InputArray, "/help\n") == 0 || strcmp(InputArray, "/h\n")==0){
				const char *Help = "/help or /h: List of all available commands\n";
				const char *Join = "/join <channel>: Joins the chat of <channel>\n";
				const char *Leave = "/leave <channel>: Leaves the chat of <channel>\n";
				const char *LeaveAll = "/leaveall: Leaves all currently joined channels\n";
				const char *List = "/list: Lists all currently connected channels\n";
				const char *Clear = "/clear: Clears the screen.\n";
				const char *SetColorMode = "/setcolormode: Changes the color scheme of channel and user name.\n    \"uniform\" or \"0\": The color is equal for all channels and user names\n    \"rgb\" or \"1\": The channel and user name color is varying, but consistent for each user\n";
				const char *SetChannelColor = "/setchannelcolor: Changes the channel color to the specified rgb color.\n    The format is: \"/setchannelcolor <red>;<green>;<blue>;\"\n    The color is a whole number between 0 and 255\n";
				const char *SetUserColor = "/setusercolor: Changes the user name color to the specified rgb color.\n    The format is: \"/setuser color <red>;<green>;<blue>;\"\n    The color is a whole number between 0 and 255\n";
				const char *SetColor = "/setcolor: a shorter form of changing the color of channel and username.\n    The format is: \"/setcolor [-c,-u] <red>;<green>;<blue>; [-c,-u] <red>;<green>;<blue>;\"\n        -c: changes the channel color\n        -u: changes the username color\n    Each color is a whole number between 0 and 255\n";
				const char *SetRGBColor = "/setrgbcolor: sets the two colors used to calculate the rgb color.\n    The format is: \"setrgbcolor [-a,-b] <red>;<green>;<blue>; [-a,-b] <red>;<green>;<blue>;\"\n        -a: sets the first color\n        -b: sets the second color\n    Each color is a whole number between 0 and 255\n";
				const char *Quit = "/quit or / q: Exits the application.\n";
				


				printf("Available commands: \n%s%s%s%s%s%s%s%s%s%s%s%s=============================================================================================================\n",Help,Join,Leave,LeaveAll,List,Clear,SetColorMode,SetChannelColor,SetUserColor,SetColor,SetRGBColor,Quit);
			}
			
			else{
				printf("Unknown command. Use /help or /h for a list of available commands.\n");
			}
		}
	}
	lpParameter = 0;
	return 0;
}



int main() {
	SetConsoleOutputCP(65001);
	SetConsoleCP(65001);

	//Init Winsock
	WSADATA WSAData;
	int Result = WSAStartup(MAKEWORD(2, 2), &WSAData);
	Assert(Result == NO_ERROR);
	
	Socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	Assert(Socket != INVALID_SOCKET);

	const char *IrcChatIPv4 = "44.237.40.50";

	sockaddr_in Address = {};
	Address.sin_family = AF_INET;
	Address.sin_port = htons(6667);
	int r = inet_pton(AF_INET, IrcChatIPv4, &Address.sin_addr.S_un.S_addr);
	Assert(r);

	if(connect(Socket, (sockaddr *)&Address, sizeof(Address)) == SOCKET_ERROR){
		int Error = WSAGetLastError();
		printf("Error: %d\n", Error);
	}

	const char *LoginMessage = "PASS asdf\r\nNICK justinfan15\r\n";
	Result = send(Socket, LoginMessage,(int) strlen(LoginMessage), 0);
	if(Result == SOCKET_ERROR){
		int Error = WSAGetLastError();
		printf("Error: %d\n", Error);
	}


	char Buffer[4096];
	fd_set SocketSet = {1,Socket};
	TIMEVAL Timeout = {0,1};

	DWORD ThreadID;
	CreateThread(0, 0, ThreadProc, NULL, 0, &ThreadID);

	
	
	while (GlobalRunning) {
		SocketSet.fd_count = 1;
		Result = select(0, &SocketSet, NULL, NULL, &Timeout);
		if(Result == 0){
			continue;
		}
		if(Result < 0){
			int Error = WSAGetLastError();
			printf("Error: %d\n", Error);
			Assert(0);
		}

		int BytesRead = ReceiveMessage(Socket, Buffer, sizeof(Buffer));
		//LogBuffer(Buffer);
		char FormattedOutput[4096] = {};


#ifdef MEASURE
		LARGE_INTEGER Start, End;
		
#endif
		if (BytesRead > 0) {
#ifdef MEASURE
			QueryPerformanceCounter(&Start);
#endif // _DEBUG

			FORMAT_RESULT FormatResult = FormatTwitchUserMessage(Buffer, sizeof(Buffer), FormattedOutput, sizeof(FormattedOutput));
#ifdef MEASURE
			QueryPerformanceCounter(&End);
			long long time = End.QuadPart - Start.QuadPart;
			printf("Time:%lldus\n", time);
#endif
			if (FormatResult == FORMAT_PING) {
				
				send(Socket, FormattedOutput, (int)strlen(FormattedOutput), 0);
				printf("\033[38;2;255;255;0m%s", FormattedOutput);
			}
			else if (FormatResult == FORMAT_SUCCESS) {

				LogTwitchMessage(FormattedOutput);
			}
#ifdef _DEBUG
			else{
				switch(FormatResult){
				case FORMAT_NON_MESSAGE:
					OutputDebugStringA("FORMAT_NON_MESSAGE\n");
					break;
				case FORMAT_OUT_OF_BOUNDS:
					OutputDebugStringA("FORMAT_OUT_OF_BOUNDS\n");
					break;
				case FORMAT_UNEXPECTED_CHAR:
					OutputDebugStringA("FORMAT_UNEXPECTED_CHAR\n");
					break;
				case FORMAT_TOO_LONG_MESSAGE_TYPE:
					OutputDebugStringA("FORMAT_TOO_LONG_MESSAGE_TYPE\n");
					break;
				case FORMAT_BUFFER_IN_SIZE_ZERO:
					OutputDebugStringA("FORMAT_BUFFER_IN_SIZE_ZERO\n");
					break;
				case FORMAT_BUFFER_OUT_SIZE_ZERO:
					OutputDebugStringA("FORMAT_BUFFER_OUT_SIZE_ZERO\n");
					break;
				case FORMAT_BUFFER_IN_SIZE_NEGATIVE:
					OutputDebugStringA("FORMAT_BUFFER_IN_SIZE_NEGATIVE\n");
					break;
				case FORMAT_BUFFER_OUT_SIZE_NEGATIVE:
					OutputDebugStringA("FORMAT_BUFFER_OUT_SIZE_NEGATIVE\n");
					break;
				case FORMAT_BUFFER_IN_NULL:
					OutputDebugStringA("FORMAT_BUFFER_IN_NULL\n");
					break;
				case FORMAT_BUFFER_OUT_NULL:
					OutputDebugStringA("FORMAT_BUFFER_OUT_NULL\n");
					break;
				case FORMAT_MESSAGE_TRUNCATED:
					OutputDebugStringA("FORMAT_MESSAGE_TRUNCATED\n");
					break;
				case FORMAT_OUTPUT_OUT_OF_MEMORY:
					OutputDebugStringA("FORMAT_OUTPUT_OUT_OF_MEMORY\n");
					break;
				case FORMAT_COMMAND:
					OutputDebugStringA("FORMAT_COMMAND\n");
					break;
				case FORMAT_JOIN_RESPONSE:
					OutputDebugStringA("FORMAT_JOIN_RESPONSE\n");
					break;
				case FORMAT_BUFFER_IN_TOO_SMALL:
					OutputDebugStringA("FORMAT_BUFFER_IN_TOO_SMALL\n");
					break;
				case FORMAT_BUFFER_OUT_TOO_SMALL:
					OutputDebugStringA("FORMAT_BUFFER_OUT_TOO_SMALL\n");
					break;
				case FORMAT_NO_MESSAGE:
					OutputDebugStringA("FORMAT_NO_MESSAGE\n");
					break;
				default:
					break;
				}
				
			}
#endif
		}
	}

	if(CurrentChannelCount > 0){
		int i = 0;
		char **CurrentChannelsRef = CurrentChannels;
		while(i < CurrentChannelCount){
			if((*CurrentChannelsRef) != NULL){
				char PartMessage[10 + MAX_USERNAME_LENGTH] = {};
				sprintf_s(PartMessage,sizeof(PartMessage), "PART #%s\r\n", *CurrentChannelsRef);
				send(Socket, PartMessage, (int)strlen(PartMessage), 0);

				free(*CurrentChannelsRef);
				*CurrentChannelsRef = NULL;
				i++;
			}
			CurrentChannelsRef++;
			Assert(CurrentChannelsRef <= CurrentChannels + MAX_CONCURRENT_CHANNELS);
		}
		CurrentChannelCount = 0;
	}
	r = closesocket(Socket);
	Assert(r == 0);
}