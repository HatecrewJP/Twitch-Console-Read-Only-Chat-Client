#define _WINSOCK_DEPRECATED_NO_WARNINGS
#include <WinSock2.h>
#include "ws2tcpip.h"
#include "stdio.h"
#include "malloc.h"
#include "consoleapi2.h"
#define Assert(x) if(!(x)) __debugbreak();

#define MAX_CONST_CHAR_STRING_LEN 4096


struct CustomString{
	int Size;
	char *Data;
};

static CustomString CreateString(const char *Src){
	size_t SrcSize = strlen(Src);
	CustomString NewString = {};
	if(SrcSize > MAX_CONST_CHAR_STRING_LEN){
		return NewString;
	}
	NewString.Data = (char*) malloc(SrcSize+1);
	if(NewString.Data){
		NewString.Size = (int)SrcSize;
		memcpy(NewString.Data, Src, NewString.Size);
	}
	NewString.Data[NewString.Size] = '\0';
	return NewString;
}

static bool TestChannelName(CustomString Src){
	for(int i = 0; i < Src.Size; i++){
		bool IsUppercaseLetter = Src.Data[i] >= 'A' && Src.Data[i] <= 'Z';
		bool IsLowercaseLetter = Src.Data[i] >= 'a' && Src.Data[i] <= 'z';
		bool IsNumber = Src.Data[i] >= '0' && Src.Data[i] <= '9';
		bool IsUnderScore = Src.Data[i] == '_';
		bool IsValid = IsUppercaseLetter || IsLowercaseLetter || IsNumber || IsUnderScore;
		if(!IsValid){
			return 0;
		}
	}
	return 1;
}


static void StringToLower(CustomString String){
	for(int i = 0; i <= String.Size; i++){
		signed char Current = String.Data[i];
		if(Current & 0x80){
			Current -= 0x80;
			if((Current >= 'A' && Current <= 'Z') || (Current >= 'a' && Current <= 'z')){
				int diff = (int)'Z' - (int)Current;

				if(diff > 0){
					Current += 32;
				}
			}
			Assert(Current < 0x80);
			if(String.Data[i] & 0x80){
				Current += 0x80;
			}

		} else{
			if((Current >= 'A' && Current <= 'Z') || (Current >= 'a' && Current <= 'z')){
				int diff = (int)'Z' - (int)Current;

				if(diff > 0){
					Current += 32;
				}
			}
			Assert(Current < 0x80);
		}
		
		
		String.Data[i] = Current;
	}
}

static void FreeString(struct CustomString *String){
	if(String->Data){
		free(String->Data);
		String->Data = NULL;
	}
	String->Size = 0;
}
enum REALLOC_RESULT{
	REALLOC_FAILED,
	REALLOC_SUCCESS,
	REALLOC_FREE,

	REALLOC_COUNT
};

static REALLOC_RESULT ReallocString(struct CustomString *String, int NewSize){
	char *tmp = (char*) realloc(String->Data, NewSize+1);
	if(tmp){
		String->Data = tmp;
		String->Size = NewSize;
		return REALLOC_SUCCESS;
	}
	if(NewSize == 0){
		Assert(tmp == NULL);
		String->Data = tmp;
		String->Size = NewSize;
		return REALLOC_FREE;
	}
	return REALLOC_FAILED;
}

static void ConcatinateString(struct CustomString *Dest, struct CustomString Src){
	int OldSize = Dest->Size;
	int NewSize = Dest->Size + Src.Size;

	Assert(ReallocString(Dest, NewSize) == REALLOC_SUCCESS);
	Assert(Dest->Size == NewSize);
	memcpy(Dest->Data + OldSize, Src.Data, Src.Size);
	Dest->Data[Dest->Size] = '\0';
}

static bool ConcatinateCString(struct CustomString *Dest, const char* Src){
	size_t SrcLen = strlen(Src);
	if(SrcLen > MAX_CONST_CHAR_STRING_LEN){
		return 0;
	}
	int OldSize = Dest->Size;
	int NewSize = Dest->Size + (int)SrcLen;
	Assert(ReallocString(Dest, NewSize) == REALLOC_SUCCESS);
	Assert(Dest->Size == NewSize);
	memcpy(Dest->Data + OldSize, Src, SrcLen);
	Dest->Data[Dest->Size] = '\0';
	return 1;
}

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


	FORMAT_ERROR_COUNT
};





#ifdef _DEBUG
	#define DEBUG_FORMATTING_INPUT()\
	*DebugCharIn = *CurrentChar;\
	DebugCharIn++;
#else
	#define DEBUG_FORMATTING_INPUT()
#endif
#ifdef _DEBUG
#define DEBUG_FORMATTING_OUTPUT()\
	*DebugCharOut = *BufferOutRef;\
	DebugCharOUT++;
#else
#define DEBUG_FORMATTING_OUTPUT()
#endif

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

	char *BufferEnd = BufferIn + BufferInSize;
	char *CurrentChar = BufferIn;

	char *DebugBuffer = (char*) malloc(BufferInSize);
	char *DebugCharIn = DebugBuffer;
	char *BufferOutRef = BufferOut;
start:
	if(*CurrentChar == 'P'){
		DEBUG_FORMATTING_INPUT();
		CurrentChar++;
		Assert(*CurrentChar == 'I');
		DEBUG_FORMATTING_INPUT();
		CurrentChar++;
		Assert(*CurrentChar == 'N');
		DEBUG_FORMATTING_INPUT();
		CurrentChar++;
		Assert(*CurrentChar == 'G');
		while(*CurrentChar != '\r' && CurrentChar < BufferEnd){
			DEBUG_FORMATTING_INPUT();
			CurrentChar++;
		}
		if(CurrentChar >= BufferEnd){
			return FORMAT_OUT_OF_BOUNDS;
		}
		Assert(memcpy(BufferOut, "PONG", 4));
		int i = 0;
		while ((i < BufferOutSize - 6) && (*CurrentChar!='\r')) {
			BufferOut[i] = *CurrentChar;
			DEBUG_FORMATTING_INPUT();
			CurrentChar++;
		}
		Assert(*CurrentChar == '\r');
		DEBUG_FORMATTING_INPUT();
		CurrentChar++;
		i++;
		Assert(*CurrentChar == '\n');
		DEBUG_FORMATTING_INPUT();
		CurrentChar++;
		i++;
		Assert(*CurrentChar == '\0');
		return FORMAT_PING;
		
	}

	Assert(*CurrentChar == ':');
	DEBUG_FORMATTING_INPUT();
	CurrentChar++;
	//Max Twitch name length is 25
	char UserArray[26] = {};
	int i = 0;
	while(*CurrentChar != '!' && CurrentChar < BufferEnd && i < 26){
		Assert(CurrentChar != NULL);
		UserArray[i] = *CurrentChar;
		i++;
		DEBUG_FORMATTING_INPUT();
		CurrentChar++;
	}
	Assert(i < 26);
	if(CurrentChar >= BufferEnd){
		return FORMAT_OUT_OF_BOUNDS;
	}
	if(*CurrentChar != '!'){
		return FORMAT_UNEXPECTED_CHAR;
	}
	Assert(*CurrentChar == '!');
	DEBUG_FORMATTING_INPUT();
	CurrentChar++;

	//Skip over uninteresting part
	while(*CurrentChar != '@' && CurrentChar < BufferEnd){
		Assert(CurrentChar != NULL);
		DEBUG_FORMATTING_INPUT();
		CurrentChar++;
	}
	if(CurrentChar >= BufferEnd){
		return FORMAT_OUT_OF_BOUNDS;
	}
	if(*CurrentChar != '@'){
		return FORMAT_UNEXPECTED_CHAR;
	}
	Assert(*CurrentChar == '@');
	DEBUG_FORMATTING_INPUT();
	CurrentChar++;

	//Skip over uninteresting part
	while(*CurrentChar != ' ' && CurrentChar < BufferEnd){
		Assert(CurrentChar != NULL);
		DEBUG_FORMATTING_INPUT();
		CurrentChar++;
	}
	if(CurrentChar >= BufferEnd){
		return FORMAT_OUT_OF_BOUNDS;
	}
	//Space
	if(*CurrentChar != 32){
		return FORMAT_UNEXPECTED_CHAR;
	}
	Assert(*CurrentChar == ' ');
	DEBUG_FORMATTING_INPUT();
	CurrentChar++;

	char MessageType[65] = {};
	i = 0;
	while(*CurrentChar != ' ' && CurrentChar < BufferEnd && i < 65){
		Assert(CurrentChar != NULL);
		MessageType[i] = *CurrentChar;
		i++;
		DEBUG_FORMATTING_INPUT();
		CurrentChar++;
	}
	if(i >= 65){
		return FORMAT_TOO_LONG_MESSAGE_TYPE;
	}
	if(CurrentChar >= BufferEnd){
		return FORMAT_OUT_OF_BOUNDS;
	}
	//Space
	if(*CurrentChar != ' '){
		return FORMAT_UNEXPECTED_CHAR;
	}
	Assert(*CurrentChar == ' ');
	DEBUG_FORMATTING_INPUT();
	CurrentChar++;

	char ChannelName[26] = {};
	Assert(*CurrentChar == '#');
	DEBUG_FORMATTING_INPUT();
	CurrentChar++;
	i = 0;
	while(*CurrentChar != ' ' && CurrentChar < BufferEnd){
		Assert(CurrentChar != NULL);
		ChannelName[i] = *CurrentChar;
		i++;
		DEBUG_FORMATTING_INPUT();
		CurrentChar++;
	}
	if(CurrentChar > BufferEnd){
		return FORMAT_OUT_OF_BOUNDS;
	}
	Assert(*CurrentChar == ' ');
	DEBUG_FORMATTING_INPUT();
	CurrentChar++;
	Assert(*CurrentChar == ':');
	CurrentChar++;

#define SAFETY_PADDING 4
	const char ColorEscapeChannel[] = "\033[38;2;255;125;125m";
	int ChannelEscapeCharCount = sizeof(ColorEscapeChannel) - 1;
	const char ColorEscapeName[] = "\033[38;2;255;0;125m";
	int NameEscapeCharCount = sizeof(ColorEscapeName)-1;
	const char ColorEscapeClear[] = "\033[0m";
	int ClearEscapeCharCount = sizeof(ColorEscapeClear)-1;
	const int FormatCharCount = NameEscapeCharCount + ClearEscapeCharCount + ChannelEscapeCharCount + 3 + SAFETY_PADDING;
	int AvailableMessageSize = BufferOutSize - sizeof(UserArray) - sizeof(MessageType) - FormatCharCount - 1;

	char *UserMessage = (char*) malloc(AvailableMessageSize+1);
	Assert(UserMessage);
	memset(UserMessage, 0, AvailableMessageSize);

	i = 0;
	while(*CurrentChar != '\r' && CurrentChar < BufferEnd && i < AvailableMessageSize){
		UserMessage[i] = *CurrentChar;
		i++;
		DEBUG_FORMATTING_INPUT();
		CurrentChar++;
	}
	if(i >= AvailableMessageSize && CurrentChar != NULL){
		return FORMAT_MESSAGE_TRUNCATED;
	}
	if(CurrentChar > BufferEnd){
		return FORMAT_OUT_OF_BOUNDS;
	}
	Assert(*CurrentChar == '\r');
	DEBUG_FORMATTING_INPUT();
	CurrentChar++;
	Assert(CurrentChar!= NULL);
	Assert(*(CurrentChar) == '\n');
	DEBUG_FORMATTING_INPUT();
	CurrentChar++;
	i = 0;
	
	size_t UserNameLength = strlen(UserArray);
	//size_t MessageTypeLength = strlen(MessageType);
	size_t ChannelNameLength = strlen(ChannelName);
	size_t MessageLength = strlen(UserMessage);


	

	Assert(UserNameLength + ChannelNameLength + MessageLength + FormatCharCount < BufferOutSize);
	
	Assert(memcpy(BufferOutRef, ColorEscapeChannel, ChannelEscapeCharCount));
	BufferOutRef += ChannelEscapeCharCount;

	Assert(memcpy(BufferOutRef, ChannelName, ChannelNameLength));
	BufferOutRef += ChannelNameLength;

	*BufferOutRef = ':';
	BufferOutRef++;

	Assert(memcpy(BufferOutRef, ColorEscapeName, NameEscapeCharCount));
	BufferOutRef += NameEscapeCharCount;

	Assert(memcpy(BufferOutRef, UserArray, UserNameLength));
	BufferOutRef += UserNameLength;

	Assert(memcpy(BufferOutRef, ColorEscapeClear, ClearEscapeCharCount));
	BufferOutRef += ClearEscapeCharCount;


	*BufferOutRef = ':';
	BufferOutRef++;
	Assert(memcpy(BufferOutRef, UserMessage, MessageLength));
	BufferOutRef+=MessageLength;
	*BufferOutRef = '\n';
	BufferOutRef++;

	if(*CurrentChar == ':'){
		goto start;
	}
	if (DebugBuffer) {
		free(DebugBuffer);
		DebugBuffer = NULL;
	}
	if (UserMessage) {
		free(UserMessage);
		UserMessage = NULL;
	}
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

int TestFormattingMultipleMessages(){
	CustomString ToTest = CreateString(
		"e"
	);
	char Buffer[4096];
	FormatTwitchUserMessage(ToTest.Data,ToTest.Size,Buffer,4096);
	return 1;
}





int main() {
	SetConsoleOutputCP(65001);
	SetConsoleCP(65001);
	char ChannelNameInput[26] = {};

SelectChannel:
	//Init Winsock
	WSADATA WSAData;
	int Result = WSAStartup(MAKEWORD(2, 2), &WSAData);
	Assert(Result == NO_ERROR);

	SOCKET Socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	Assert(Socket != INVALID_SOCKET);




	sockaddr_in Address = {};
	Address.sin_family = AF_INET;
	Address.sin_port = htons(6667);
	Address.sin_addr.s_addr = inet_addr("44.237.40.50");

	if (connect(Socket, (sockaddr*)&Address, sizeof(Address)) == SOCKET_ERROR) {
		int Error = WSAGetLastError();
		printf("Error: %d\n", Error);
	}


	CustomString LoginMessage = CreateString("PASS asdf\r\nNICK justinfan74123\r\n");


	Result = send(Socket, LoginMessage.Data, LoginMessage.Size, 0);
	if (Result == SOCKET_ERROR) {
		int Error = WSAGetLastError();
		printf("Error: %d\n", Error);
	}

	char Buffer[4096];
	int BytesRead = ReceiveMessage(Socket, Buffer, sizeof(Buffer));
	//LogBuffer(Buffer);
JoinPrompt:
	printf("Which channel do you want to join?\n");
	fgets(ChannelNameInput, 25, stdin);
	size_t Len = strlen(ChannelNameInput);
	Assert(ChannelNameInput[Len - 1] == '\n');
	ChannelNameInput[Len - 1] = '\0';
	CustomString ChannelToJoin = CreateString(ChannelNameInput);
	if (!TestChannelName(ChannelToJoin)) {
		printf("Invalid Channel Name: Channel Names only consist of Letters, Numbers and Underscores.\n");
		goto JoinPrompt;
	}
	StringToLower(ChannelToJoin);

	CustomString JoinMessage = CreateString("JOIN #");
	ConcatinateString(&JoinMessage, ChannelToJoin);
	ConcatinateCString(&JoinMessage, "\r\n");
	Result = send(Socket, JoinMessage.Data, JoinMessage.Size, 0);
	if (Result == SOCKET_ERROR) {
		int Error = WSAGetLastError();
		printf("Error: %d\n", Error);
	}
	fd_set SocketSet = { 1,Socket };
	timeval TimeoutValue = { 5,0 };
	if (select(0, &SocketSet, NULL, NULL, &TimeoutValue) == 0) {
		printf("Joining the channel %s failed. Please make sure, that the channel exists and try again.\n", ChannelToJoin.Data);
		goto SelectChannel;
	}

	BytesRead = ReceiveMessage(Socket, Buffer, sizeof(Buffer));
	CustomString ChannelJoined = CreateString("You joined: ");
	ConcatinateString(&ChannelJoined, ChannelToJoin);
	printf("%s\n", ChannelJoined.Data);

	BytesRead = ReceiveMessage(Socket, Buffer, sizeof(Buffer));



	while (1) {
		BytesRead = ReceiveMessage(Socket, Buffer, sizeof(Buffer));
		//LogBuffer(Buffer);
		char FormattedOutput[4096] = {};
#ifdef MEASURE
		LARGE_INTEGER Start, End;
		QueryPerformanceCounter(&Start);
#endif
		if (BytesRead > 0) {
			FORMAT_RESULT FormatResult = FormatTwitchUserMessage(Buffer, sizeof(Buffer), FormattedOutput, sizeof(FormattedOutput));

			if (FormatResult == FORMAT_PING) {
				const char* PongMessage = "PONG\r\n";
				send(Socket, PongMessage, (int)strlen(PongMessage), 0);
				printf("\033[38;2;255;255;0m%s", PongMessage);
			}
			else if (FormatResult == FORMAT_SUCCESS) {
#ifdef MEASURE
				QueryPerformanceCounter(&End);
				long long time = End.QuadPart - Start.QuadPart;
				printf("Time:%lldus\n", time);
#endif
				LogTwitchMessage(FormattedOutput);
			}
		}
	}
}