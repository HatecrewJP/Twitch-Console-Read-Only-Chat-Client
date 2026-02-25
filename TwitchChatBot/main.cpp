#define _WINSOCK_DEPRECATED_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS

#include "math.h"


#include "ws2tcpip.h"
#include "stdio.h"
#include "malloc.h"
#include "consoleapi2.h"
#define Assert(x) if(!(x)) __debugbreak();



	

#define MAX_CONCURRENT_CHANNELS 64
#define MAX_LENGTH 500
#define MAX_COMMAND_LENGTH 20
#define MAX_USERNAME_LENGTH 25
static char *CurrentChannels[MAX_CONCURRENT_CHANNELS] = {};
static int CurrentChannelCount = 0;

struct RGB{
	unsigned char R;
	unsigned char G;
	unsigned char B;
};

static RGB UniformChannelColor = {255,125,125};
static RGB UniformUserColor = {255,0,125};
static bool IsUniformColors = 0;



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

struct Slice{
	char *Ptr;
	size_t Length;
};



static FORMAT_RESULT FormatTwitchUserMessage(char *BufferIn, int BufferInSize, char *BufferOut, int BufferOutSize){
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
	char *CurrentChar = BufferIn;
	char *BufferOutRef = BufferOut;
	char *BufferOutEnd = BufferOut + BufferOutSize;

	if (BufferInSize < 4) {
		return FORMAT_BUFFER_IN_TOO_SMALL;
	}
	while (*CurrentChar != '\0') {
		bool IsPing = !strncmp(CurrentChar, "PING", 4);
		if (IsPing) {
			//Output: "PONG <text in>\r\n"
			Assert(memcpy(BufferOut, "PONG", 4));
			CurrentChar += 4;
			//start writing after "PONG"
			int i = 4;
			//copy <text in>
			//6 = 4(PONG) + 2(\r\n)
			while ((i < BufferOutSize - 6) && (*CurrentChar != '\r') && CurrentChar < BufferInEnd) {
				BufferOut[i] = *CurrentChar;
				CurrentChar++;
				i++;
			}
			if (CurrentChar >= BufferInEnd) {
				return FORMAT_OUT_OF_BOUNDS;
			}
			if ((*CurrentChar != '\r') && (i > (BufferOutSize - 6))) {
				return FORMAT_OUTPUT_OUT_OF_MEMORY;
			}
			Assert(strncmp(CurrentChar, "\r\n", 2) == 0);
			memcpy(&BufferOut[i], "\r\n", 2);
			CurrentChar += 2;
			return FORMAT_PING;
		}
		//recover buffer overflow
		if (*CurrentChar != ':') {
			while (*CurrentChar != '\n' && CurrentChar < BufferInEnd) {
				CurrentChar++;
			}
			if (CurrentChar >= BufferInEnd) {
				return FORMAT_NO_MESSAGE;
			}
			Assert(*(CurrentChar - 1) == '\r' && *CurrentChar == '\n');
			CurrentChar++;
			if (CurrentChar >= BufferInEnd) {
				return FORMAT_OUT_OF_BOUNDS;
			}
			static unsigned DroppedMessageCount = 0;
			DroppedMessageCount++;
			printf("\033[38;2;255;0;0mTotal Dropped Messages: %d\n\033[0m", DroppedMessageCount);
		}
		if (*CurrentChar == ':') {
			while (*CurrentChar == ':') {
				///////////////////
				//Extract Message//
				///////////////////
				CurrentChar++;
				Slice UserName;
				UserName.Ptr = CurrentChar;
				while (*CurrentChar != '!' && *CurrentChar != '.' && CurrentChar < BufferInEnd) {
					CurrentChar++;
				}
				if (CurrentChar >= BufferInEnd) {
					return FORMAT_OUT_OF_BOUNDS;
				}
				if (*CurrentChar == '.') {
					while (*CurrentChar != '\n' && CurrentChar < BufferInEnd) {
						CurrentChar++;
					}
					if (CurrentChar >= BufferInEnd) {
						return FORMAT_OUT_OF_BOUNDS;
					}
					Assert(*(CurrentChar - 1) == '\r' && *CurrentChar == '\n');
					CurrentChar++;
					if (CurrentChar >= BufferInEnd) {
						return FORMAT_OUT_OF_BOUNDS;
					}
					return FORMAT_NON_MESSAGE;
				}
				UserName.Length = CurrentChar - UserName.Ptr;
	bool IsPing = !strncmp(CurrentChar, "PING", 4);
	if(IsPing){
		//Output: "PONG <text in>\r\n"
		Assert(memcpy(BufferOut, "PONG", 4));
		CurrentChar += 4;
		//start writing after "PONG"
		int i = 4;
		//copy <text in>
		//6 = 4(PONG) + 2(\r\n)
		while((i < BufferOutSize - 6) && (*CurrentChar != '\r') && CurrentChar < BufferInEnd){
			BufferOut[i] = *CurrentChar;
			CurrentChar++;
			i++;
		}
		if(CurrentChar >= BufferInEnd){
			return FORMAT_OUT_OF_BOUNDS;
		}
		if((*CurrentChar != '\r') && (i > (BufferOutSize - 6))){
			return FORMAT_OUTPUT_OUT_OF_MEMORY;
		}
		Assert(strncmp(CurrentChar, "\r\n", 2) == 0);
		memcpy(&BufferOut[i], "\r\n", 2);
		CurrentChar += 2;
		return FORMAT_PING;
	}

	if(*CurrentChar == ':'){
		while(*CurrentChar == ':'){
			///////////////////
			//Extract Message//
			///////////////////
			CurrentChar++;
			Slice UserName;
			UserName.Ptr = CurrentChar;
			unsigned UserColorCalc = (unsigned)((*(unsigned*)CurrentChar) & 0x00ffffff);
			while(*CurrentChar != '!' && *CurrentChar != '.' && CurrentChar < BufferInEnd){
	
				CurrentChar++;
			}
			if(CurrentChar >= BufferInEnd){
				return FORMAT_OUT_OF_BOUNDS;
			}
			if(*CurrentChar == '.'){
				while(*CurrentChar != '\n' && CurrentChar < BufferInEnd){
					CurrentChar++;
				}
				if(CurrentChar >= BufferInEnd){
					return FORMAT_OUT_OF_BOUNDS;
				}
				Assert(*(CurrentChar-1) == '\r' && *CurrentChar == '\n');
				CurrentChar++;
				if(CurrentChar >= BufferInEnd){
					return FORMAT_OUT_OF_BOUNDS;
				}
				return FORMAT_NON_MESSAGE;
			}
			UserName.Length = CurrentChar - UserName.Ptr;
			RGB Color1 = {200,20,20};
			RGB Color2 = {255,120,255};

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

			
			while(*CurrentChar != ' ' && CurrentChar < BufferInEnd){
				CurrentChar++;
			}
			if(CurrentChar >= BufferInEnd){
				return FORMAT_OUT_OF_BOUNDS;
			}
			Assert(*CurrentChar == ' ');
			CurrentChar++;
			if(CurrentChar >= BufferInEnd){
				return FORMAT_OUT_OF_BOUNDS;
			}

				Slice MessageType;
				MessageType.Ptr = CurrentChar;
				while (*CurrentChar != ' ' && CurrentChar < BufferInEnd) {
					CurrentChar++;
				}

			if(CurrentChar >= BufferInEnd){
				return FORMAT_OUT_OF_BOUNDS;
			}
			MessageType.Length = CurrentChar - MessageType.Ptr;
			CurrentChar++;
			if(CurrentChar >= BufferInEnd){
				return FORMAT_OUT_OF_BOUNDS;
			}
			if(strncmp(MessageType.Ptr, "PRIVMSG",7) == 0){
				Assert(*CurrentChar == '#');
				CurrentChar++;
				if(CurrentChar >= BufferInEnd){
					return FORMAT_OUT_OF_BOUNDS;
				}
				Slice ChannelName;
				ChannelName.Ptr = CurrentChar;
				unsigned ChannelColorCalc = *(unsigned*)CurrentChar;
				while(*CurrentChar != ':' && CurrentChar < BufferInEnd){
					
					CurrentChar++;
				}
				if(CurrentChar >= BufferInEnd){
					return FORMAT_OUT_OF_BOUNDS;
				}
				float CalcChannelR = (float)(ChannelColorCalc >> 16 & 0xff);
				float CalcChannelG = (float)(ChannelColorCalc >> 8 & 0xff);
				float CalcChannelB = (float)(ChannelColorCalc & 0xff);

				RGB ChannelColorRGB;
				ChannelColorRGB.R = (unsigned char)(StepSizeR * (float)('z' - CalcChannelR) + min(Color1.R, Color2.R));
				ChannelColorRGB.G = (unsigned char)(StepSizeG * (float)('z' - CalcChannelG) + min(Color1.G, Color2.G));
				ChannelColorRGB.B = (unsigned char)(StepSizeB * (float)('z' - CalcChannelB) + min(Color1.B, Color2.B));

				Assert(*CurrentChar == ':');
				ChannelName.Length = CurrentChar - ChannelName.Ptr - 1;

					CurrentChar++;
					if (CurrentChar >= BufferInEnd) {
						return FORMAT_OUT_OF_BOUNDS;
					}

					Slice UserMessage;
					UserMessage.Ptr = CurrentChar;
					while (*CurrentChar != '\r' && CurrentChar < BufferInEnd) {
						CurrentChar++;
					}
					if (CurrentChar >= BufferInEnd) {
						return FORMAT_OUT_OF_BOUNDS;
					}
					Assert(*CurrentChar == '\r' && *(CurrentChar + 1) == '\n');
					UserMessage.Length = CurrentChar - UserMessage.Ptr;
					CurrentChar += 2;
					if (CurrentChar >= BufferInEnd) {
						return FORMAT_OUT_OF_BOUNDS;
					}
					//////////////////
					//Copy To Output//
					//////////////////
#define SAFETY_PADDING 4
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


					long int BufferOutSizeFree = (long int)(BufferOutEnd - BufferOutRef);

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
					Assert(*CurrentChar == '#');
					CurrentChar++;
					if (CurrentChar >= BufferInEnd) {
						return FORMAT_OUT_OF_BOUNDS;
					}
					Slice JoinedChannel;
					JoinedChannel.Ptr = CurrentChar;
					while (*CurrentChar != '\n' && CurrentChar < BufferInEnd) {
						CurrentChar++;
					}
					if (CurrentChar >= BufferInEnd) {
						return FORMAT_OUT_OF_BOUNDS;
					}
					Assert(*(CurrentChar - 1) == '\r' && *CurrentChar == '\n');
					JoinedChannel.Length = CurrentChar - JoinedChannel.Ptr - 1;
					CurrentChar++;
					if (CurrentChar >= BufferInEnd) {
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
	Assert(*CurrentChar == '\0');
	return FORMAT_SUCCESS;

}

static int ReceiveMessage(SOCKET Socket ,char *Buffer, int BufferSize){
	memset(Buffer, 0, BufferSize);
	int Result = recv(Socket, Buffer, BufferSize, 0);
	if(Result == SOCKET_ERROR){
		int Error = WSAGetLastError();
		printf("Error: %d\n", Error);
	}
	return Result;
}

static void LogTwitchMessage( char *Buffer){
	printf("%s", Buffer);
	
}
static void LogBuffer(char *Buffer){
	printf("%s\n", Buffer);
}

static SOCKET Socket;


static int IsInArray(char* *StringArray, int ArrayFillCount, char *ToFind,int ToFindSize){
	if(ArrayFillCount <= 0){
		return -1;
	}
	for(int i = 0; i < MAX_CONCURRENT_CHANNELS; i++){
		char *CurrentString = *(StringArray + i);
		if(strlen(CurrentString) == ToFindSize){
			if(strncmp(CurrentString, ToFind, ToFindSize)==0){
				return i;
			}
		}
	}
	return -1;
}



DWORD WINAPI ThreadProc(
	void* lpParameter
){
	char InputArray[MAX_LENGTH+1];
	printf("You are ready to join a chat. If you need help use /help.\n");
	
	while(1){
		memset(InputArray, 0, MAX_LENGTH+1);
		fgets(InputArray, MAX_LENGTH + 1, stdin);

		if(InputArray[0] == '/'){
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
			else if(strcmp(InputArray, "/clear\n")==0){
				printf("\033[2J\033[H");
			}
			else if(strcmp(InputArray, "/help\n") == 0 || strcmp(InputArray, "/h\n")==0){
				const char *Help = "/help or /h: List of all available commands\n";
				const char *Join = "/join <channel>: Joins the chat of <channel>\n";
				const char *Leave = "/leave <channel>: Leaves the chat of <channel>\n";
				const char *LeaveAll = "/leaveall: Leaves all currently joined channels\n";
				const char *List = "/list: Lists all currently connected channels\n";
				const char *Clear = "/clear: Clears the screen.\n";
				printf("Available commands: \n%s%s%s%s%s%s=====================================================\n",Help,Join,Leave,LeaveAll,List,Clear);
			}



			else{
				printf("Unknown command. Use /help or /h for a list of available commands.\n");
			}
			

		}

	}
		
	lpParameter = 0;
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




	sockaddr_in Address = {};
	Address.sin_family = AF_INET;
	Address.sin_port = htons(6667);
	Address.sin_addr.s_addr = inet_addr("44.237.40.50");

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
	DWORD ThreadID;
	CreateThread(0, 0, ThreadProc, NULL, 0, &ThreadID);


	char Buffer[4096];

	while (1) {
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
		}
	}
}