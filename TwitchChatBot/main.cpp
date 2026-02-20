#define _WINSOCK_DEPRECATED_NO_WARNINGS

#include "ws2tcpip.h"
#include "stdio.h"
#include "malloc.h"
#include "consoleapi2.h"
#define Assert(x) if(!(x)) __debugbreak();

#define MAX_CONST_CHAR_STRING_LEN 4096


struct Arena{
	size_t Size;
	byte *Data;
	size_t NextFreeIndex;
};


static Arena CreateArena(size_t Size){
	Arena NewArena = {};
	void *Memory = VirtualAlloc(NULL, Size, MEM_COMMIT, PAGE_READWRITE);
	if(Memory == NULL){
		return NewArena;
	}
	Assert(Memory);
	NewArena.Size = Size;
	NewArena.Data = (byte*) Memory;
#ifdef _DEBUG
	memset(NewArena.Data, 0xcd, NewArena.Size);
#endif // _DEBUG

	return NewArena;
}

static void FreeArena(Arena *Arena){
	if(Arena->Data){
		VirtualFree(Arena->Data, 0, MEM_RELEASE);
		memset(Arena, 0, sizeof(Arena));
	}
}

static void* ArenaAlloc(struct Arena *Arena, size_t Size){
	if((Arena->Size - Arena->NextFreeIndex) >= Size){
		void *Ptr = (Arena->Data + Arena->NextFreeIndex);
#ifdef _DEBUG
		memset(Ptr, 0xab, Size);
#endif
		Arena->NextFreeIndex+= Size;
		return Ptr;
	}
	return NULL;
}

static void ResetArena(struct Arena *Arena){
	memset(Arena->Data, 0, Arena->Size);
#ifdef _DEBUG
	memset(Arena->Data, 0xcd, Arena->Size);
#endif // _DEBUG


	Arena->NextFreeIndex= 0;
}

static void* ArenaRealloc(struct Arena *Arena, void *Ptr,size_t OldSize, size_t NewSize){
	long int Offset = (long int) ((byte*)Ptr - Arena->Data);
	if(Offset < 0 || Offset > Arena->Size){
		return NULL;
	}
	if((Arena->Size - Arena->NextFreeIndex) >= NewSize){
		if(Offset == (Arena->NextFreeIndex - OldSize)){
			Arena->NextFreeIndex += (NewSize-OldSize);
#ifdef _DEBUG
			memset((byte*)Ptr + OldSize, 0xab, (NewSize - OldSize));
#endif
			return Ptr;

		}
		void *NewPtr = Arena->Data + Arena->NextFreeIndex;
		Arena->NextFreeIndex += NewSize;
		memcpy(NewPtr, Ptr, OldSize);

#ifdef _DEBUG
		memset((byte *)NewPtr + OldSize, 0xab, (NewSize - OldSize));
#endif
		return NewPtr;
	}
	return NULL;
}

struct CustomString{
	int Size;
	char *Data;
};

static CustomString CreateString(Arena *Arena, const char *Src){
	size_t SrcSize = strlen(Src);
	CustomString NewString = {};
	if(SrcSize > MAX_CONST_CHAR_STRING_LEN){
		return NewString;
	}
	NewString.Data = (char *)ArenaAlloc(Arena, SrcSize+1);
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

static REALLOC_RESULT ReallocString(struct Arena *Arena,struct CustomString *String, int NewSize){
	char *tmp = (char*) ArenaRealloc(Arena,String->Data,String->Size+1, NewSize + 1);
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

static void ConcatinateString(struct Arena *Arena,struct CustomString *Dest, struct CustomString Src){
	int OldSize = Dest->Size + 1;
	Assert(Dest->Data[Dest->Size] == '\0');
	//Dest is 0 terminated
	int NewSize = (OldSize - 1) + Src.Size;

	Assert(ReallocString(Arena,Dest, NewSize) == REALLOC_SUCCESS);
	Assert(Dest->Size == NewSize);
	memcpy(Dest->Data + OldSize - 1, Src.Data, Src.Size);
	Dest->Data[Dest->Size] = '\0';
}

static bool ConcatinateCString(struct Arena *Arena,struct CustomString *Dest, const char* Src){
	size_t SrcLen = strlen(Src);
	if(SrcLen > MAX_CONST_CHAR_STRING_LEN){
		return 0;
	}
	int OldSize = Dest->Size + 1;
	Assert(Dest->Data[Dest->Size] == '\0');
	//Dest is 0 terminated
	int NewSize = (OldSize - 1) + (int)SrcLen ;
	Assert(ReallocString(Arena,Dest, NewSize) == REALLOC_SUCCESS);
	Assert(Dest->Size == NewSize);
	memcpy(Dest->Data + OldSize - 1, Src, SrcLen);
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
	#define DEBUG_FORMATTING_INPUT_N(n)\
	memcpy(DebugCharIn,CurrentChar,n);\
	DebugCharIn+=n;
#else
#define DEBUG_FORMATTING_INPUT_N(n)
#endif // _DEBUG


#ifdef _DEBUG
#define DEBUG_FORMATTING_OUTPUT()\
	*DebugCharOut = *BufferOutRef;\
	DebugCharOUT++;
#else
#define DEBUG_FORMATTING_OUTPUT()
#endif

static FORMAT_RESULT FormatTwitchUserMessage(struct Arena *ScratchArena,char *BufferIn, int BufferInSize, char *BufferOut, int BufferOutSize){
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

#ifdef _DEBUG
	char *DebugBuffer = (char*)malloc(BufferInSize);
	Assert(DebugBuffer);
	char *DebugCharIn = DebugBuffer;
	
#endif	
char *BufferOutRef = BufferOut;
start:
	ResetArena(ScratchArena);
	bool IsPing = strncmp(CurrentChar, "PING", 4);
	
	if(IsPing==0){
		//Output: "PONG <text in>\r\n"
		DEBUG_FORMATTING_INPUT_N(4);
		CurrentChar += 4;
		Assert(memcpy(BufferOut, "PONG", 4));
		//start writing after "PONG"
		int i = 4;
		//copy <text in>
		//6 = 4(PONG) + 2(\r\n)
		while ((i < BufferOutSize - 6) && (*CurrentChar!='\r')) {
			BufferOut[i] = *CurrentChar;
			DEBUG_FORMATTING_INPUT();
			CurrentChar++;
			i++;
		}
		if((*CurrentChar != '\r') && (i > (BufferOutSize - 6))){
			return FORMAT_OUT_OF_BOUNDS;
		}
		Assert(strncmp(CurrentChar, "\r\n", 2) == 0);
		memcpy(&BufferOut[i], "\r\n", 2);
		DEBUG_FORMATTING_INPUT_N(2);
		CurrentChar += 2;
		Assert(*CurrentChar == '\0');
#ifdef _DEBUG
		free(DebugBuffer);
#endif
		return FORMAT_PING;
	}

	//Twitch Message Layout: ":<name>!<name>@<name>.tmi.twitch.tv <Type> #<channel> :<message>\r\n"
	
	//':'
	if(*CurrentChar == ':'){
		DEBUG_FORMATTING_INPUT();
		CurrentChar++;

#pragma region UserName
		//Max Twitch name length is 25
		//<name>
		char UserName[26] = {};
		int i = 0;
		while(*CurrentChar != '!' && CurrentChar < BufferEnd && i < 26){
			Assert(CurrentChar != NULL);
			UserName[i] = *CurrentChar;
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
		//'!'
		Assert(*CurrentChar == '!');
		DEBUG_FORMATTING_INPUT();
		CurrentChar++;
#pragma endregion


#pragma region IrrelevantMessagePart
		//Skip over "@<name>.tmi.twitch.tv "
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
		if(*CurrentChar != ' '){
			return FORMAT_UNEXPECTED_CHAR;
		}
		Assert(*CurrentChar == ' ');
		DEBUG_FORMATTING_INPUT();
		CurrentChar++;
#pragma endregion


#pragma region Type
		//<Type>
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
		if(*CurrentChar != ' '){
			return FORMAT_UNEXPECTED_CHAR;
		}
		Assert(*CurrentChar == ' ');
		DEBUG_FORMATTING_INPUT();
		CurrentChar++;
#pragma endregion


#pragma region ChannelName
		//#<channel>:
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
#pragma endregion


#pragma region EscapeCodes
#define SAFETY_PADDING 4
		const char ColorEscapeChannel[] = "\033[38;2;255;125;125m";
		int ChannelEscapeCharCount = sizeof(ColorEscapeChannel) - 1;
		const char ColorEscapeName[] = "\033[38;2;255;0;125m";
		int NameEscapeCharCount = sizeof(ColorEscapeName) - 1;
		const char ColorEscapeClear[] = "\033[0m";
		int ClearEscapeCharCount = sizeof(ColorEscapeClear) - 1;
		const int FormatCharCount = NameEscapeCharCount + ClearEscapeCharCount + ChannelEscapeCharCount + 3 + SAFETY_PADDING;
#pragma endregion


#pragma region Message
		//<message>\r\n
		int AvailableMessageSize = BufferOutSize - sizeof(UserName) - sizeof(MessageType) - FormatCharCount - 1;
		char *UserMessage = (char *)ArenaAlloc(ScratchArena,AvailableMessageSize + 1);
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
		Assert(CurrentChar != NULL);
		Assert(*(CurrentChar) == '\n');
		DEBUG_FORMATTING_INPUT();
		CurrentChar++;
		i = 0;
#pragma endregion


#pragma region Output
		size_t UserNameLength = strlen(UserName);
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

		Assert(memcpy(BufferOutRef, UserName, UserNameLength));
		BufferOutRef += UserNameLength;

		Assert(memcpy(BufferOutRef, ColorEscapeClear, ClearEscapeCharCount));
		BufferOutRef += ClearEscapeCharCount;

		*BufferOutRef = ':';
		BufferOutRef++;
		Assert(memcpy(BufferOutRef, UserMessage, MessageLength));
		BufferOutRef += MessageLength;
		*BufferOutRef = '\n';
		BufferOutRef++;
#pragma endregion
		if(*CurrentChar == ':'){
			goto start;
		}
#ifdef _DEBUG
		free(DebugBuffer);
		DebugBuffer = NULL;
		
#endif
		return FORMAT_SUCCESS;
	}
	return FORMAT_NON_MESSAGE;
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



int main() {
	SetConsoleOutputCP(65001);
	SetConsoleCP(65001);
	char ChannelNameInput[26] = {};

	Arena StringArena = CreateArena(4096);

	

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


	CustomString LoginMessage = CreateString(&StringArena,"PASS asdf\r\nNICK justinfan74123\r\n");


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
	CustomString ChannelToJoin = CreateString(&StringArena,ChannelNameInput);
	if (!TestChannelName(ChannelToJoin)) {
		printf("Invalid Channel Name: Channel Names only consist of Letters, Numbers and Underscores.\n");
		goto JoinPrompt;
	}
	StringToLower(ChannelToJoin);

	CustomString JoinMessage = CreateString(&StringArena,"JOIN #");
	ConcatinateString(&StringArena,&JoinMessage, ChannelToJoin);
	ConcatinateCString(&StringArena,&JoinMessage, "\r\n");
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
	CustomString Test = CreateString(&StringArena, "");
	ConcatinateCString(&StringArena, &Test, "Test");
	ConcatinateString(&StringArena, &LoginMessage, Test);
	

	BytesRead = ReceiveMessage(Socket, Buffer, sizeof(Buffer));
	CustomString ChannelJoined = CreateString(&StringArena,"You joined: ");
	ConcatinateString(&StringArena,&ChannelJoined, ChannelToJoin);
	printf("%s\n", ChannelJoined.Data);

	BytesRead = ReceiveMessage(Socket, Buffer, sizeof(Buffer));


#define MEASURE
	Arena ScratchArena = CreateArena(4096*2);

	while (1) {
		BytesRead = ReceiveMessage(Socket, Buffer, sizeof(Buffer));
		//LogBuffer(Buffer);
		char FormattedOutput[4096] = {};
#ifdef MEASURE
		LARGE_INTEGER Start, End;
		
#endif
		if (BytesRead > 0) {
			ResetArena(&ScratchArena);
			QueryPerformanceCounter(&Start);
			FORMAT_RESULT FormatResult = FormatTwitchUserMessage(&ScratchArena,Buffer, sizeof(Buffer), FormattedOutput, sizeof(FormattedOutput));
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
		}
	}
}