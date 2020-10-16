#include "ipcsockets.h"

static char* serializeInt(int num)
{
    char* result = malloc(4);
    result[0] = num >> 24;
    result[1] = num >> 16;
    result[2] = num >> 8;
    result[3] = num;
    return result;
}

static int deserializeInt(unsigned char* str)
{
    if (str == NULL)
        return -1;
    int result = str[0];
    result = (result << 8) + str[1];
    result = (result << 8) + str[2];
    result = (result << 8) + str[3];
    return result;
}

/*
 *  Either fetches the FileDescriptor given by systemd socket activation or creates
 *  a new socket for communication. Only needed if the Service needs to accept an
 *  incoming IPC-socket connection. 
 */
int createIPCSocket(char* socketname)
{
    if(sd_listen_fds(0) == 1)
    {
        return SD_LISTEN_FDS_START;
    }
    else
    {
        struct sockaddr_un addr;
        int fd;
        if ((fd = socket(AF_UNIX, SOCK_STREAM, 0)) == -1) {
            perror("socket error");
            return -1;
        }

        memset(&addr, 0, sizeof(addr));
        addr.sun_family = AF_UNIX;

        *addr.sun_path = '\0';
        strncpy(addr.sun_path+1, socketname, sizeof(addr.sun_path)-2);

        // +3 because of 2 bytes sun_family and 1 byte 0 character
        if (bind(fd, (struct sockaddr*)&addr, 3 + strlen(socketname)) == -1) 
        {
            perror("bind error");
            return -1;
        }

        if (listen(fd, 5) == -1) 
        {
            perror("listen error");
            return -1;
        }

        return fd;
    }
}

/*
 *  Creates a new socket connection to the socket specified by socketname.
 *  Used for a connection request from the client-side. 
 */ 
IPCSocketConnection* connectToIPCSocket(char* socketname, IPCmsgHandler messageHandler)
{
    int fd;
    struct sockaddr_un addr;
    IPCSocketConnection* connection = malloc(sizeof *connection);

    if ((fd = socket(AF_UNIX, SOCK_STREAM, 0)) == -1) {
		perror("socket error");
        free(connection);
		return NULL;
	}

    connection->fd = fd;
    connection->socketname = socketname;
    connection->buffer = NULL;
    connection->open = 1;

    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;

    *addr.sun_path = '\0';
    strncpy(addr.sun_path+1, socketname, sizeof(addr.sun_path)-2);

    // +3 because of 2 bytes sun_family and 1 byte 0 character
    if (connect(fd, (struct sockaddr*)&addr, 3 + strlen(socketname)) == -1) {
    	perror("connect error");
        free(connection);
    	return NULL;
    }

    if(pthread_create(&connection->thread, NULL, messageHandler, connection)) {
        fprintf(stderr, "Error creating IPC socket thread\n");
        return NULL;
    }

    printf("started connection with %s\n", socketname);

    return connection;
}

/*
 *  Accepts an incoming connection request on the socket specified by fd
 *  from the socket specified by socketname.
 */ 
IPCSocketConnection* acceptIPCConnection(int fd, char* socketname, IPCmsgHandler messageHandler)
{
    IPCSocketConnection* connection = malloc(sizeof *connection);
    connection->socketname = socketname;
    connection->buffer = NULL;
    connection->open = 1;

    if ((connection->fd = accept(fd, NULL, NULL)) == -1) 
    {
        free(connection);
        perror("accept error");
        return NULL;
    }

    if(pthread_create(&connection->thread, NULL, messageHandler, connection)) {
        fprintf(stderr, "Error creating IPC socket thread\n");
        return NULL;
    }

    printf("accepted connection from %s\n", socketname);

    return connection;
}

/*
 *  Sends a message to the IPC socket specified by ipcsc.
 */
int sendMessageIPC(IPCSocketConnection* ipcsc, MessageType messageType, char* msg, int length)
{
    int fragments = length / (MAX_MESSAGE_SIZE - MESSAGE_HEADER_SIZE) + 1;

    int messageLength[fragments];
    int completeLength[fragments];

    for (int i = 0; i < fragments-1; i++)
    {
        messageLength[i] = MAX_MESSAGE_SIZE - MESSAGE_HEADER_SIZE;
        completeLength[i] = MAX_MESSAGE_SIZE;
    }

    messageLength[fragments-1] = length % (MAX_MESSAGE_SIZE - MESSAGE_HEADER_SIZE);
    completeLength[fragments-1] = messageLength[fragments-1] + MESSAGE_HEADER_SIZE;

    for (int i = 0; i < fragments; i++)
    {
        // prepare buffer for sending the message 
        ipcsc->buffer = malloc(completeLength[i]);

        // prepare message header
        char* type = serializeInt(messageType);
        char* length = serializeInt(messageLength[i]);
        char* fragmentNumber = serializeInt(i);
        char* isLastFragment = serializeInt(i == (fragments-1));

        memcpy(ipcsc->buffer, type, sizeof(int));
        memcpy(ipcsc->buffer+sizeof(int), length, sizeof(int));
        memcpy(ipcsc->buffer+sizeof(int)*2, fragmentNumber, sizeof(int));
        memcpy(ipcsc->buffer+sizeof(int)*3, isLastFragment, sizeof(int));

        free(type);
        free(length);
        free(fragmentNumber);
        free(isLastFragment);

        // copy message into buffer
        memcpy(ipcsc->buffer+MESSAGE_HEADER_SIZE, msg+i*(MAX_MESSAGE_SIZE-MESSAGE_HEADER_SIZE), messageLength[i]);

        if (write(ipcsc->fd, ipcsc->buffer, completeLength[i]) != completeLength[i])
        {
            if (completeLength > 0) fprintf(stderr,"partial write\n");
            else
            {
                perror("write error");
                free(ipcsc->buffer);
                return -1;
            }
        }
        //printf("TYPE:    %d\nLENGTH:  %d\nCONTENT: %s\n", messageType, messageLength, msg);
        free(ipcsc->buffer);
    }

    return 0;
}

/*
 *  Reads the specified amount of bytes from the given socket. 
 */ 
static char* readFromSocket(IPCSocketConnection* ipcsc, int bytes)
{
    int rc;
    char* result = malloc(bytes);
    if ((rc = read(ipcsc->fd, result, bytes)) > 0)
    {
        if (rc == bytes)
            return result;
        else 
            return NULL;
    }
    if (rc == 0)
    {
        return NULL;
    }
    else if (rc == -1)
    {
        perror("read");
        return NULL;
    }
}

/*
 *  
 */ 
unsigned int hasMessages(IPCSocketConnection* ipcsc)
{
    int count;
    ioctl(ipcsc->fd, FIONREAD, &count);
    return count;
}

/*
 *  Receives a SocketMessage from the IPC socket specified by ipcsc.
 */
static SocketMessage receiveSocketMessageIPC(IPCSocketConnection* ipcsc)
{
    SocketMessage result;

    char* type = readFromSocket(ipcsc, sizeof(int));
    char* length = readFromSocket(ipcsc, sizeof(int));
    char* fragmentNumber = readFromSocket(ipcsc, sizeof(int));
    char* isLastFragment = readFromSocket(ipcsc, sizeof(int));
    
    result.type = deserializeInt(type);
    result.length = deserializeInt(length);
    result.fragmentNumber = deserializeInt(fragmentNumber);
    result.isLastFragment = deserializeInt(isLastFragment);

    free(type);
    free(length);
    free(fragmentNumber);
    free(isLastFragment);

    if (result.type == IPCMSGTYPE_INTERRUPTED)
    {   
        result.type = IPCMSGTYPE_INTERRUPTED;
        result.length = 0;
        result.fragmentNumber = 0;
        result.isLastFragment = 1;
        result.content = NULL;
        return result;
    }

    result.content = readFromSocket(ipcsc, result.length);
    return result;
}

/*
 *  Receives a Message from the IPC socket specified by ipcsc.
 */
Message receiveMessageIPC(IPCSocketConnection* ipcsc)
{
    Message result;
    SocketMessage incoming = receiveSocketMessageIPC(ipcsc);

    result.type = incoming.type;
    result.length = incoming.length;
    result.content = malloc(incoming.length+1);
    memcpy(result.content, incoming.content, incoming.length);

    while(!incoming.isLastFragment)
    {
        free(incoming.content);
        incoming = receiveSocketMessageIPC(ipcsc);
        result.content = realloc(result.content, incoming.fragmentNumber * (MAX_MESSAGE_SIZE - MESSAGE_HEADER_SIZE) + incoming.length + 1);
        memcpy(result.content + incoming.fragmentNumber * (MAX_MESSAGE_SIZE - MESSAGE_HEADER_SIZE), incoming.content, incoming.length);
        result.length += incoming.length;
    }

    free(incoming.content);
    result.content[incoming.fragmentNumber * (MAX_MESSAGE_SIZE - MESSAGE_HEADER_SIZE) + incoming.length] = '\0';
    return result;
}

/*
 *  Closes an IPC Connection
 */
void closeIPCConnection(IPCSocketConnection* ipcsc)
{
    if (ipcsc->open)
        sendMessageIPC(ipcsc, IPCMSGTYPE_CLOSEDCONNECTION, NULL, 0);
    close(ipcsc->fd);
    ipcsc->open = 0;
    pthread_join(ipcsc->thread, NULL);
    printf("closed connection to %s\n", ipcsc->socketname);
    free(ipcsc);
}