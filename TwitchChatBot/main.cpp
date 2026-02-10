#define _WINSOCK_DEPRECATED_NO_WARNINGS
#include <WinSock2.h>
#include "ws2tcpip.h"
#include "stdio.h"
#include "malloc.h"
#define Assert(x) if(!(x)) *(char*)0=0;

enum FORMAT_ERROR{
	FORMAT_PLACEHOLDER,
	FORMAT_SUCCESS,
	FORMAT_OUT_OF_BOUNDS,
	FORMAT_UNEXPECTED_CHAR,
	FORMAT_TOO_LONG_MESSAGE_TYPE,
	FORMAT_BUFFER_IN_SIZE_ZERO,
	FORMAT_BUFFER_OUT_SIZE_ZERO,
	FORMAT_BUFFER_IN_SIZE_NEGATIVE,
	FORMAT_BUFFER_OUT_SIZE_NEGATIVE,
	FORMAT_BUFFER_IN_NULL,
	FORMAT_BUFFER_OUT_NULL,
	FORMAT_MESSAGE_CUT,

	FORMAT_ERROR_COUNT
};
//TODO: Format multiple messages in a single buffer
//:: -> :
//: -> new message

static FORMAT_ERROR FormatTwitchUserMessage(char *BufferIn, int BufferInSize, char *BufferOut, int BufferOutSize){
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

start:
	Assert(*CurrentChar == ':');
	CurrentChar++;
	//Max Twitch name length is 25
	char UserArray[26] = {};
	int i = 0;
	while(*CurrentChar != '!' && CurrentChar < BufferEnd && i < 26){
		Assert(CurrentChar != NULL);
		UserArray[i] = *CurrentChar;
		i++;
		CurrentChar++;
	}
	Assert(i < 26);
	if(CurrentChar >= BufferEnd){
		return FORMAT_OUT_OF_BOUNDS;
	}
	if(*CurrentChar != '!'){
		return FORMAT_UNEXPECTED_CHAR;
	}
	Assert(*CurrentChar=='!')
	CurrentChar++;

	//Skip over uninteresting part
	while(*CurrentChar != '@' && CurrentChar < BufferEnd){
		Assert(CurrentChar != NULL);
		CurrentChar++;
	}
	if(CurrentChar >= BufferEnd){
		return FORMAT_OUT_OF_BOUNDS;
	}
	if(*CurrentChar != '@'){
		return FORMAT_UNEXPECTED_CHAR;
	}
	Assert(*CurrentChar == '@');
	CurrentChar++;

	Assert(' ' == 32);
	//Skip over uninteresting part
	while(*CurrentChar != 32 && CurrentChar < BufferEnd){
		Assert(CurrentChar != NULL);
		CurrentChar++;
	}
	if(CurrentChar >= BufferEnd){
		return FORMAT_OUT_OF_BOUNDS;
	}
	//Space
	if(*CurrentChar != 32){
		return FORMAT_UNEXPECTED_CHAR;
	}
	Assert(*CurrentChar == 32);
	CurrentChar++;

	char MessageType[65] = {};
	i = 0;
	while(*CurrentChar != 32 && CurrentChar < BufferEnd && i < 65){
		Assert(CurrentChar != NULL);
		MessageType[i] = *CurrentChar;
		i++;
		CurrentChar++;
	}
	if(i >= 65){
		return FORMAT_TOO_LONG_MESSAGE_TYPE;
	}
	if(CurrentChar >= BufferEnd){
		return FORMAT_OUT_OF_BOUNDS;
	}
	//Space
	if(*CurrentChar != 32){
		return FORMAT_UNEXPECTED_CHAR;
	}
	Assert(*CurrentChar == 32);
	CurrentChar++;

	//Skip over channel

	while(*CurrentChar != ':' && CurrentChar < BufferEnd){
		Assert(CurrentChar != NULL);
		CurrentChar++;
	}
	if(CurrentChar > BufferEnd){
		return FORMAT_OUT_OF_BOUNDS;
	}
	Assert(*CurrentChar = ':');
	CurrentChar++;
	const int FormatCharCount = 10;
	int AvailableMessageSize = BufferOutSize - sizeof(UserArray) - sizeof(MessageType) - FormatCharCount - 1;

	char *UserMessage = (char*) _malloca(AvailableMessageSize+1);
	Assert(UserMessage);
	memset(UserMessage, 0, AvailableMessageSize);

	i = 0;
	while(*CurrentChar != '\0' && CurrentChar < BufferEnd && i < AvailableMessageSize){
		UserMessage[i] = *CurrentChar;
		i++;
		CurrentChar++;
	}
	if(i >= AvailableMessageSize && CurrentChar != NULL){
		return FORMAT_MESSAGE_CUT;
	}
	if(CurrentChar > BufferEnd){
		return FORMAT_OUT_OF_BOUNDS;
	}
	Assert(*CurrentChar == '\0');

	
	i = 0;
	
	size_t UserNameLength = strlen(UserArray);
	size_t MessageTypeLength = strlen(MessageType);
	size_t MessageLength = strlen(UserMessage);

	Assert(UserNameLength + MessageTypeLength + MessageLength + FormatCharCount < BufferOutSize);
	memset(BufferOut, 0, BufferOutSize);
	char *BufferOutRef = BufferOut;
	Assert(memcpy(BufferOutRef, UserArray, UserNameLength));
	BufferOutRef += UserNameLength;
	*BufferOutRef = ':';
	BufferOutRef++;
	*BufferOutRef = ' ';
	BufferOutRef++;
	Assert(memcpy(BufferOutRef, UserMessage, MessageLength));
	BufferOutRef++;
	if(*BufferOutRef == ':'){
		goto start;
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

static void LogBuffer(int PrintCount, char *Buffer){
	printf("%s\n", Buffer);
}

int main(){
	const char *ChannelToJoin= "jp_hatecrew";
	
	const char *NicknameArray[2] = {"jp_hatecrew","hcjpdev"};
	const char *TokenArray[2] = {"eslbu593z3nmnoxsxz9e1l7qjw0000","goha4lu5m9tp9ulzan6k0csy0ckijm"};

	
	
	//Init Winsock
	WSADATA WSAData;
	int Result = WSAStartup(MAKEWORD(2, 2), &WSAData);
	Assert(Result == NO_ERROR);

	SOCKET Socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	Assert(Socket!= INVALID_SOCKET);

	sockaddr_in Address = {};
	Address.sin_family = AF_INET;
	Address.sin_port = htons(6667);
	Address.sin_addr.s_addr = inet_addr("44.237.40.50");


	if(connect(Socket, (sockaddr *)&Address, sizeof(Address)) == SOCKET_ERROR){
		int Error = WSAGetLastError();
		printf("Error: %d\n", Error);
	}
	
	const char *LoginMessage = "PASS oauth:eslbu593z3nmnoxsxz9e1l7qjw0000\r\nNICK jp_hatecrew\r\n";
	Result = send(Socket, LoginMessage, (int)strlen(LoginMessage), 0);
	if(Result == SOCKET_ERROR){
		int Error = WSAGetLastError();
		printf("Error: %d\n", Error);
	}

	char Buffer[4096];
	int BytesRead = ReceiveMessage(Socket, Buffer, sizeof(Buffer));
	LogBuffer(BytesRead, Buffer);



	const char *JoinMessage = "JOIN #shlorox\r\n";
	Result = send(Socket, JoinMessage, (int)strlen(JoinMessage), 0);
	if(Result == SOCKET_ERROR){
		int Error = WSAGetLastError();
		printf("Error: %d\n", Error);
	}

	BytesRead = ReceiveMessage(Socket, Buffer, sizeof(Buffer));
	LogBuffer(BytesRead, Buffer);
	BytesRead = ReceiveMessage(Socket, Buffer, sizeof(Buffer));
	LogBuffer(BytesRead, Buffer);

	while(1){
		BytesRead = ReceiveMessage(Socket, Buffer, sizeof(Buffer));
		//Assert(FormatTwitchUserMessage(Buffer, sizeof(Buffer), Buffer, sizeof(Buffer)));
		LogBuffer(BytesRead, Buffer);
	}



}